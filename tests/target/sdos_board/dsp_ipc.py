"""Task 013: drive one DSP through load, start, echo, stop, and restart.

The host uploads a manifest-pinned firmware file to ``/lib/firmware/sdos/`` on
the board and checks its SHA-256 there. It then runs ``REMOTE_AGENT`` with the
board's ``python3``. The agent resolves the core by device-tree unit address,
never by ``remoteprocN``, and drives remoteproc and rpmsg_char itself, so
timings do not include SSH round trips. It prints one JSON event per line. The
host archives the events, the kernel log for the run, and the firmware trace,
then evaluates the run.

The agent only touches the selected core. It restores that core's firmware
name and leaves the core offline, also after a failure.
"""

from __future__ import annotations

import json
import shlex
import subprocess
import time
from pathlib import Path
from typing import Any, Callable, Sequence

from ci import firmware_elf

from .cores import CORES, Core
from .report import ssh_command

SCHEMA = "sdos-dsp-ipc/1"
FIRMWARE_DIR = "sdos/ti-ipc"  # relative to /lib/firmware, as remoteproc expects
TI_ECHO_SERVICE = "rpmsg_chrdev"
TI_ECHO_ENDPOINT = 14

Runner = Callable[[Sequence[str], bytes | None], tuple[int, bytes]]

# Runs on the board under python3 (Debian 13 ships 3.13). It must stay
# self-contained: the board has no copy of this repository.
REMOTE_AGENT = r'''
import fcntl, glob, json, os, select, sys, time

RPMSG_CREATE_EPT_IOCTL = 0x4028b501  # _IOW(0xb5, 1, struct rpmsg_endpoint_info), 40 bytes
RPMSG_DESTROY_EPT_IOCTL = 0xb502     # _IO(0xb5, 2)


def emit(event, **fields):
    fields["event"] = event
    fields["t"] = round(time.monotonic(), 6)
    print(json.dumps(fields), flush=True)


class Board:
    """Kernel interfaces. Tests replace this class with a simulation."""

    def __init__(self, root="/"):
        self.root = root

    def path(self, *parts):
        return os.path.join(self.root, *parts)

    def read(self, path):
        with open(path) as f:
            return f.read().strip()

    def write(self, path, value):
        with open(path, "w") as f:
            f.write(value)

    def rproc_for(self, device):
        for entry in sorted(glob.glob(self.path("sys/class/remoteproc/remoteproc*"))):
            if os.path.basename(os.path.realpath(os.path.join(entry, "device"))) == device:
                return entry
        return None

    def ctrl_for(self, device):
        for entry in sorted(glob.glob(self.path("sys/class/rpmsg/rpmsg_ctrl*"))):
            if "/" + device + "/" in os.path.realpath(entry):
                return self.path("dev", os.path.basename(entry))
        return None

    def endpoints(self):
        return set(glob.glob(self.path("dev/rpmsg[0-9]*")))

    def create_endpoint(self, ctrl, name, dst):
        before = self.endpoints()
        info = name.encode().ljust(32, b"\0") + (0xFFFFFFFF).to_bytes(4, "little") + dst.to_bytes(4, "little")
        fd = os.open(ctrl, os.O_RDWR)
        try:
            fcntl.ioctl(fd, RPMSG_CREATE_EPT_IOCTL, info)
        finally:
            os.close(fd)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            new = self.endpoints() - before
            if new:
                return sorted(new)[0]
            time.sleep(0.05)
        raise RuntimeError("no /dev/rpmsgN appeared after RPMSG_CREATE_EPT_IOCTL")

    def open_endpoint(self, dev):
        return os.open(dev, os.O_RDWR)

    def exchange(self, fd, payload, timeout):
        os.write(fd, payload)
        ready, _, _ = select.select([fd], [], [], timeout)
        if not ready:
            return None
        return os.read(fd, 512)

    def destroy_endpoint(self, fd):
        try:
            fcntl.ioctl(fd, RPMSG_DESTROY_EPT_IOCTL)
        finally:
            os.close(fd)

    def trace(self, rproc):
        path = self.path("sys/kernel/debug/remoteproc", os.path.basename(rproc), "trace0")
        try:
            with open(path, errors="replace") as f:
                return f.read()[-4096:]
        except OSError as exc:
            return "unavailable: %s" % exc


def wait_for(probe, timeout, interval=0.02):
    start = time.monotonic()
    while True:
        value = probe()
        if value:
            return value, time.monotonic() - start
        if time.monotonic() - start >= timeout:
            return None, time.monotonic() - start
        time.sleep(interval)


def set_state(board, rproc, command, wanted, timeout):
    state = board.read(os.path.join(rproc, "state"))
    if state == wanted:
        return True, 0.0
    board.write(os.path.join(rproc, "state"), command)
    ok, elapsed = wait_for(lambda: board.read(os.path.join(rproc, "state")) == wanted, timeout)
    return bool(ok), elapsed


def echo_round(board, fd, plan):
    rtts, mismatches = [], []
    messages = ["ping %d" % i for i in range(1, plan["count"] + 1)]
    messages.append("sdos echo " + "x" * (plan["payload"] - len("sdos echo ")))
    for message in messages:
        expected = message.replace("ping", "pong", 1) if message.startswith("ping") else message
        sent = time.monotonic()
        reply = board.exchange(fd, message.encode(), plan["reply_timeout"])
        if reply is None:
            mismatches.append({"sent": message[:32], "reply": None})
            break
        rtts.append(time.monotonic() - sent)
        text = reply.rstrip(b"\0").decode(errors="replace")
        if text != expected:
            mismatches.append({"sent": message[:32], "reply": text[:32]})
    return rtts, mismatches, len(messages)


def cycle(board, rproc, plan, number):
    ok, elapsed = set_state(board, rproc, "start", "running", plan["start_timeout"])
    emit("start", cycle=number, ok=ok, seconds=round(elapsed, 4), state=board.read(os.path.join(rproc, "state")))
    if not ok:
        return False
    ctrl, elapsed = wait_for(lambda: board.ctrl_for(plan["device"]), plan["start_timeout"])
    emit("rpmsg_ctrl", cycle=number, ok=bool(ctrl), seconds=round(elapsed, 4), path=ctrl)
    emit("trace", cycle=number, text=board.trace(rproc))
    if not ctrl:
        return False
    dev = board.create_endpoint(ctrl, plan["service"], plan["endpoint"])
    fd = board.open_endpoint(dev)
    try:
        rtts, mismatches, sent = echo_round(board, fd, plan)
    finally:
        board.destroy_endpoint(fd)
    passed = not mismatches and len(rtts) == sent
    emit("echo", cycle=number, ok=passed, endpoint=dev, sent=sent, received=len(rtts),
         mismatches=mismatches, rtt_us={
             "min": round(min(rtts) * 1e6, 1) if rtts else None,
             "max": round(max(rtts) * 1e6, 1) if rtts else None,
             "mean": round(sum(rtts) / len(rtts) * 1e6, 1) if rtts else None,
         })
    ok, elapsed = set_state(board, rproc, "stop", "offline", plan["stop_timeout"])
    emit("stop", cycle=number, ok=ok, seconds=round(elapsed, 4))
    return passed and ok


def run(board, plan):
    rproc = board.rproc_for(plan["device"])
    if rproc is None:
        emit("resolve", ok=False, device=plan["device"])
        return 1
    firmware_attr = os.path.join(rproc, "firmware")
    original = board.read(firmware_attr)
    emit("resolve", ok=True, device=plan["device"], remoteproc=os.path.basename(rproc),
         state=board.read(os.path.join(rproc, "state")), firmware=original)
    passed = False
    try:
        ok, elapsed = set_state(board, rproc, "stop", "offline", plan["stop_timeout"])
        emit("offline", ok=ok, seconds=round(elapsed, 4))
        if ok:
            board.write(firmware_attr, plan["firmware"])
            emit("load", ok=board.read(firmware_attr) == plan["firmware"], firmware=plan["firmware"])
            passed = all(cycle(board, rproc, plan, n) for n in range(1, plan["cycles"] + 1))
    except Exception as exc:  # report and clean up; the host evaluates the events
        emit("error", message="%s: %s" % (type(exc).__name__, exc))
        passed = False
    finally:
        try:
            set_state(board, rproc, "stop", "offline", plan["stop_timeout"])
            board.write(firmware_attr, original)
        except Exception as exc:
            emit("cleanup", ok=False, message=str(exc))
        emit("final", state=board.read(os.path.join(rproc, "state")), firmware=board.read(firmware_attr))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(run(Board(), json.loads(sys.argv[1])))
'''


def _run(argv: Sequence[str], stdin: bytes | None) -> tuple[int, bytes]:
    result = subprocess.run(list(argv), input=stdin, capture_output=True, check=False, timeout=300)
    return result.returncode, result.stdout


def plan_for(core: Core, firmware_name: str, count: int = 10, cycles: int = 2) -> dict[str, Any]:
    return {
        "device": core.device,
        "firmware": f"{FIRMWARE_DIR}/{firmware_name}",
        "service": TI_ECHO_SERVICE,
        "endpoint": TI_ECHO_ENDPOINT,
        "count": count,
        "cycles": cycles,
        "payload": 200,
        "reply_timeout": 2.0,
        "start_timeout": 15.0,
        "stop_timeout": 10.0,
    }


def upload(host: str, key: Path | None, local: Path, sha256: str, runner: Runner = _run) -> None:
    """Copy a file into /lib/firmware/sdos/ti-ipc and verify it on the board."""
    remote = f"/lib/firmware/{FIRMWARE_DIR}/{local.name}"
    script = (
        f"set -e; install -d /lib/firmware/{FIRMWARE_DIR}; cat > {shlex.quote(remote)}.part; "
        f"echo {shlex.quote(sha256 + '  ' + remote + '.part')} | sha256sum -c --quiet; "
        f"mv {shlex.quote(remote)}.part {shlex.quote(remote)}"
    )
    code, _ = runner(ssh_command(host, key) + [script], local.read_bytes())
    if code != 0:
        raise RuntimeError(f"upload of {local.name} to {host} failed or its SHA-256 differed (exit {code})")


def events(output: bytes) -> list[dict[str, Any]]:
    parsed = []
    for line in output.decode("utf-8", errors="replace").splitlines():
        try:
            parsed.append(json.loads(line))
        except ValueError:
            parsed.append({"event": "unparsed", "text": line})
    return parsed


def evaluate(run_events: list[dict[str, Any]], cycles: int) -> dict[str, Any]:
    """Pass only when every cycle started, echoed, and stopped, and cleanup held."""
    by_kind: dict[str, list[dict[str, Any]]] = {}
    for event in run_events:
        by_kind.setdefault(event["event"], []).append(event)
    checks = {
        "resolved": all(e.get("ok") for e in by_kind.get("resolve", [{}])),
        "loaded": bool(by_kind.get("load")) and all(e["ok"] for e in by_kind["load"]),
        "started": len([e for e in by_kind.get("start", []) if e["ok"]]) == cycles,
        "rpmsg": len([e for e in by_kind.get("rpmsg_ctrl", []) if e["ok"]]) == cycles,
        "echo": len([e for e in by_kind.get("echo", []) if e["ok"]]) == cycles,
        "stopped": len([e for e in by_kind.get("stop", []) if e["ok"]]) == cycles,
        "no_errors": not by_kind.get("error") and not by_kind.get("cleanup"),
        "left_offline": bool(by_kind.get("final")) and by_kind["final"][-1].get("state") == "offline",
    }
    return {"passed": all(checks.values()), "checks": checks}


def run_core(
    core: Core,
    host: str,
    key: Path | None,
    firmware: Path,
    sha256: str,
    input_id: str,
    out: Path,
    count: int = 10,
    cycles: int = 2,
    runner: Runner = _run,
) -> dict[str, Any]:
    fit = firmware_elf.inspect(firmware, firmware_elf.load_core(core.name))
    if not fit.ok:  # refuse before anything reaches the board
        raise RuntimeError(f"{firmware.name} does not fit {core.name}: {'; '.join(fit.problems)}")
    upload(host, key, firmware, sha256, runner)
    _, start = runner(ssh_command(host, key) + ["date", "+%s"], None)
    plan = plan_for(core, firmware.name, count, cycles)
    code, output = runner(
        ssh_command(host, key) + ["python3", "-", shlex.quote(json.dumps(plan))], REMOTE_AGENT.encode()
    )
    _, kernel_log = runner(
        ssh_command(host, key)
        + ["journalctl", "-k", "-o", "short-monotonic", "--no-pager", f"--since=@{start.decode().strip()}"],
        None,
    )
    _, uname = runner(ssh_command(host, key) + ["uname", "-r"], None)
    run_events = events(output)
    result = {
        "schema": SCHEMA,
        "core": core.name,
        "device": core.device,
        "captured_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "kernel": uname.decode().strip(),
        "firmware": {"input": input_id, "file": firmware.name, "sha256": sha256},
        "plan": plan,
        "agent_exit": code,
        "events": run_events,
        **evaluate(run_events, cycles),
    }
    out.mkdir(parents=True, exist_ok=True)
    stem = out / f"{core.name}"
    (stem.with_suffix(".json")).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    (out / f"{core.name}-kernel.txt").write_bytes(kernel_log)
    traces = [e.get("text", "") for e in run_events if e["event"] == "trace"]
    (out / f"{core.name}-trace.txt").write_text("\n@@cycle\n".join(traces), encoding="utf-8")
    return result


def resolve(names: Sequence[str]) -> list[Core]:
    unknown = [name for name in names if name not in CORES]
    if unknown:
        raise ValueError(f"unknown core {', '.join(unknown)}; known: {', '.join(CORES)}")
    return [CORES[name] for name in names]
