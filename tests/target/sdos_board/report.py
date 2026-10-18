"""Read-only board report: collect, evaluate, and summarize a board's state.

``collect`` runs one fixed shell script on the board over SSH. The script only
reads files and runs query commands; it prints sections delimited by
``@@section <name>``. ``parse`` turns that text into a report, and
``evaluate`` derives a pass, fail, or info result for every Phase 1 health
item. The raw device tree blob is copied separately for the memory audit.
"""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
from pathlib import Path
from typing import Any, Callable, Sequence

SCHEMA = "sdos-board-report/1"

# Every command reads state; none writes. The remote shell runs with `set +e`
# so one missing tool cannot hide the remaining sections.
REMOTE_SCRIPT = r"""
set +e
s() { echo "@@section $1"; }
s dogtag; cat /etc/dogtag
s os_release; cat /etc/os-release
s uname; uname -srvm
s cmdline; cat /proc/cmdline
s model; tr -d '\0' < /proc/device-tree/model; echo
s compatible; tr '\0' '\n' < /proc/device-tree/compatible
s uboot; tr -d '\0' < /proc/device-tree/chosen/u-boot,version 2>/dev/null; echo
s packages; dpkg-query -W -f='${Package} ${Version}\n' 'linux-image-*' 'bb-u-boot-*' 'ti-*' 'bbb.io-*' 'bb-*' 2>/dev/null
s package_list_sha256; dpkg-query -W -f='${Package} ${Version}\n' | sort | sha256sum | cut -d' ' -f1
s fdt_sha256; sha256sum /sys/firmware/fdt | cut -d' ' -f1
s firmware; ls -l /lib/firmware/ 2>/dev/null | grep -E ' j7|ti-' ; ls -l /lib/firmware/ti-ipc /lib/firmware/ti-eth 2>/dev/null | head -40
s remoteproc
for r in /sys/class/remoteproc/remoteproc*; do
  printf '%s|%s|%s|%s|%s\n' "$(basename "$r")" "$(cat "$r/name")" "$(cat "$r/state")" "$(cat "$r/firmware" 2>/dev/null)" "$(readlink -f "$r/device")"
done
s rpmsg; ls /sys/bus/rpmsg/devices 2>/dev/null; ls -l /dev/rpmsg* /dev/rpmsg_ctrl* 2>/dev/null
s kconfig
cfg=/boot/config-$(uname -r); { [ -r "$cfg" ] && cat "$cfg"; } || zcat /proc/config.gz 2>/dev/null
s meminfo; grep -E '^(MemTotal|MemAvailable|CmaTotal|CmaFree):' /proc/meminfo
s reserved_memory
for n in /proc/device-tree/reserved-memory/*/; do
  printf '%s|%s|%s|%s\n' "$(basename "$n")" "$(od -An -tx1 -v "$n/reg" 2>/dev/null | tr -d ' \n')" "$(tr -d '\0' < "$n/compatible" 2>/dev/null)" "$([ -e "$n/no-map" ] && echo no-map)"
done
s iomem; cat /proc/iomem
s mounts; findmnt -rno TARGET,SOURCE,FSTYPE,OPTIONS
s df; df -B1 --output=target,size,avail / /boot/firmware 2>/dev/null
s lsblk; lsblk -rno NAME,SIZE,TYPE,FSTYPE,LABEL,MOUNTPOINT
s debugfs; findmnt -rno TARGET -t debugfs; findmnt -rno TARGET -t tracefs
s time; timedatectl show -p NTPSynchronized -p Timezone -p LocalRTC 2>/dev/null; date -u +%Y-%m-%dT%H:%M:%SZ; ls /dev/rtc* 2>/dev/null
s thermal
for z in /sys/class/thermal/thermal_zone*; do printf '%s|%s\n' "$(cat "$z/type")" "$(cat "$z/temp")"; done
s hwmon
for h in /sys/class/hwmon/hwmon*; do printf '%s|%s\n' "$(cat "$h/name")" "$(ls "$h" | grep -cE '^(fan|pwm)[0-9]')"; done
s journal; ls -d /var/log/journal 2>/dev/null; journalctl --list-boots --no-pager 2>/dev/null | tail -n 5
s network; ip -o link; ip -o -4 addr; ip -o -6 addr
s failed_units; systemctl --failed --no-legend --plain 2>/dev/null
s boot_time; systemd-analyze 2>/dev/null | head -n 1
s uptime; cat /proc/uptime
s dmesg_errors; dmesg --level=err,crit,alert,emerg 2>/dev/null | tail -n 40
"""

REQUIRED_KCONFIG = (
    "CONFIG_REMOTEPROC",
    "CONFIG_TI_K3_DSP_REMOTEPROC",
    "CONFIG_RPMSG",
    "CONFIG_RPMSG_CHAR",
    "CONFIG_RPMSG_VIRTIO",
    "CONFIG_CMA",
    "CONFIG_DMA_CMA",
    "CONFIG_DEBUG_FS",
    "CONFIG_FTRACE",
)
# DSP cores by their fixed address, never by remoteprocN enumeration.
DSP_DEVICES = {"C66x_0": "4d80800000.dsp", "C66x_1": "4d81800000.dsp", "C71x_0": "64800000.dsp"}
MAC = re.compile(r"\b(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\b", re.I)
MAX_TEMP_MILLI_C = 85000

Runner = Callable[[Sequence[str], bytes | None], tuple[int, bytes]]


def ssh_command(host: str, key: Path | None, user: str = "root") -> list[str]:
    argv = ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", "-o", "LogLevel=ERROR"]
    if key:
        argv += ["-i", str(key), "-o", "IdentitiesOnly=yes"]
    return argv + [f"{user}@{host}"]


def _run(argv: Sequence[str], stdin: bytes | None) -> tuple[int, bytes]:
    result = subprocess.run(list(argv), input=stdin, capture_output=True, check=False, timeout=120)
    return result.returncode, result.stdout


def collect(host: str, key: Path | None = None, runner: Runner = _run) -> str:
    code, out = runner(ssh_command(host, key) + ["sh", "-s"], REMOTE_SCRIPT.encode())
    if code not in (0, 1) or b"@@section" not in out:
        raise RuntimeError(f"board-report could not collect from {host} (ssh exit {code})")
    return out.decode("utf-8", errors="replace")


def fetch_fdt(host: str, destination: Path, key: Path | None = None, runner: Runner = _run) -> str:
    """Copy the live device tree blob; return its SHA-256."""
    code, blob = runner(ssh_command(host, key) + ["cat", "/sys/firmware/fdt"], None)
    if code != 0 or not blob:
        raise RuntimeError("could not read /sys/firmware/fdt")
    destination.write_bytes(blob)
    return hashlib.sha256(blob).hexdigest()


def sections(text: str) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    current = None
    for line in text.splitlines():
        if line.startswith("@@section "):
            current = line.split(" ", 1)[1].strip()
            result[current] = []
        elif current is not None:
            result[current].append(line)
    return result


def _redact(value: str) -> str:
    return MAC.sub(lambda m: "mac-" + hashlib.sha256(m.group(0).lower().encode()).hexdigest()[:8], value)


def parse(text: str) -> dict[str, Any]:
    raw = sections(text)
    get = lambda name: [line for line in raw.get(name, []) if line.strip()]  # noqa: E731
    kconfig = {}
    for line in raw.get("kconfig", []):
        if line.startswith("CONFIG_") and "=" in line:
            name, value = line.split("=", 1)
            kconfig[name] = value
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            kconfig[line[2:].split()[0]] = "n"
    remoteprocs = []
    for line in get("remoteproc"):
        parts = line.split("|")
        if len(parts) == 5:
            remoteprocs.append(dict(zip(("sysfs", "name", "state", "firmware", "device"), parts)))
    reserved = []
    for line in get("reserved_memory"):
        name, reg, compatible, nomap = (line.split("|") + ["", "", "", ""])[:4]
        cells = [int(reg[i:i + 8], 16) for i in range(0, len(reg), 8)] if reg else []
        entry: dict[str, Any] = {"node": name, "compatible": compatible, "no_map": nomap == "no-map"}
        if len(cells) == 4:  # two address cells and two size cells
            entry["base"] = f"0x{(cells[0] << 32) | cells[1]:x}"
            entry["size"] = (cells[2] << 32) | cells[3]
        reserved.append(entry)
    meminfo = {}
    for line in get("meminfo"):
        key, value = line.split(":", 1)
        meminfo[key] = int(value.split()[0]) * 1024
    thermal = {}
    for line in get("thermal"):
        name, _, temp = line.partition("|")
        if temp.strip().lstrip("-").isdigit():
            thermal[name] = int(temp)
    hwmon = {}
    for line in get("hwmon"):
        name, _, count = line.partition("|")
        hwmon[name] = int(count or 0)
    time_lines = get("time")
    return {
        "schema": SCHEMA,
        "identity": {
            "dogtag": " ".join(get("dogtag")),
            "os": next((l.split("=", 1)[1].strip('"') for l in get("os_release") if l.startswith("PRETTY_NAME=")), ""),
            "kernel": " ".join(get("uname")),
            "cmdline": " ".join(get("cmdline")),
            "model": " ".join(get("model")),
            "compatible": get("compatible"),
            "u_boot": " ".join(get("uboot")),
            "packages": get("packages"),
            "package_list_sha256": " ".join(get("package_list_sha256")),
            "fdt_sha256": " ".join(get("fdt_sha256")),
        },
        "firmware_files": get("firmware"),
        "remoteproc": remoteprocs,
        "rpmsg": get("rpmsg"),
        "kconfig": {name: kconfig.get(name, "missing") for name in REQUIRED_KCONFIG},
        "memory": {"meminfo_bytes": meminfo, "reserved": reserved},
        "iomem_lines": len(get("iomem")),
        "mounts": get("mounts"),
        "disk": get("df"),
        "block_devices": get("lsblk"),
        "debugfs_tracefs": get("debugfs"),
        "time": time_lines,
        "thermal_milli_c": thermal,
        "hwmon_fan_channels": hwmon,
        "journal": get("journal"),
        "network": [_redact(line) for line in get("network")],
        "failed_units": get("failed_units"),
        "boot_time": " ".join(get("boot_time")),
        "uptime_s": float(get("uptime")[0].split()[0]) if get("uptime") else None,
        "dmesg_errors": [_redact(line) for line in get("dmesg_errors")],
    }


def evaluate(report: dict[str, Any]) -> list[dict[str, str]]:
    """One result per Phase 1 health item: pass, fail, or info."""
    results = []

    def add(item: str, status: str, detail: str) -> None:
        results.append({"item": item, "status": status, "detail": detail})

    add("ssh", "pass", "report collected over SSH")
    by_device = {Path(r["device"]).name: r for r in report["remoteproc"]}
    for core, device in DSP_DEVICES.items():
        r = by_device.get(device)
        if r is None:
            add(f"remoteproc {core}", "fail", f"{device} is not a remoteproc device")
        else:
            add(f"remoteproc {core}", "pass", f"{device} is {r['sysfs']}, state {r['state']}, firmware {r['firmware']}")
    missing = [k for k, v in report["kconfig"].items() if v not in ("y", "m")]
    add("kernel features", "fail" if missing else "pass",
        ("missing: " + ", ".join(missing)) if missing else "all required options are y or m")
    cma = report["memory"]["meminfo_bytes"].get("CmaTotal", 0)
    add("CMA", "pass" if cma > 0 else "fail", f"CmaTotal {cma // (1024 * 1024)} MiB")
    reserved = [r["node"] for r in report["memory"]["reserved"]]
    add("reserved memory", "pass" if reserved else "fail", f"{len(reserved)} nodes")
    root = next((m.split() for m in report["mounts"] if m.split()[:1] == ["/"]), [])
    writable = len(root) == 4 and "rw" in root[3].split(",")
    add("storage", "pass" if writable else "fail",
        f"root {root[1]} ({root[2]}) is {'read-write' if writable else 'not writable'}" if len(root) == 4 else "root not found")
    tracing = report["debugfs_tracefs"]
    add("debugfs and tracefs", "pass" if len(tracing) >= 2 else "fail", ", ".join(tracing) or "not mounted")
    synced = any(line == "NTPSynchronized=yes" for line in report["time"])
    add("time", "pass" if synced else "fail", "NTP synchronized" if synced else "clock not synchronized; " + " ".join(report["time"]))
    hot = {k: v for k, v in report["thermal_milli_c"].items() if v >= MAX_TEMP_MILLI_C}
    hottest = max(report["thermal_milli_c"].values(), default=0)
    add("temperature", "fail" if hot or not report["thermal_milli_c"] else "pass",
        f"hottest zone {hottest / 1000:.1f} C over {len(report['thermal_milli_c'])} zones")
    fans = sum(report["hwmon_fan_channels"].values())
    add("fan", "info", f"{fans} fan or PWM channels visible")
    persistent = any(line.startswith("/var/log/journal") for line in report["journal"])
    add("persistent logs", "pass" if persistent else "fail",
        "journal stored in /var/log/journal" if persistent else "journal is volatile")
    eth_up = any(re.search(r"\beth0\b.*state UP", line) for line in report["network"])
    add("ethernet", "pass" if eth_up else "fail", "eth0 link up" if eth_up else "eth0 has no link")
    add("failed units", "pass" if not report["failed_units"] else "fail",
        ", ".join(u.split()[0] for u in report["failed_units"]) or "none")
    return results


def summary(report: dict[str, Any], results: list[dict[str, str]]) -> str:
    ident = report["identity"]
    lines = [
        f"{ident['model']} | {ident['dogtag']}",
        f"kernel {ident['kernel']} | U-Boot {ident['u_boot']} | DTB {ident['fdt_sha256'][:12]}",
        "",
    ]
    width = max(len(r["item"]) for r in results)
    lines += [f"{r['status'].upper():5} {r['item']:{width}}  {r['detail']}" for r in results]
    return "\n".join(lines) + "\n"


def write(report: dict[str, Any], results: list[dict[str, str]], out: Path) -> None:
    out.mkdir(parents=True, exist_ok=True)
    document = dict(report, results=results)
    (out / "board-report.json").write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    (out / "board-report.txt").write_text(summary(report, results), encoding="utf-8")
