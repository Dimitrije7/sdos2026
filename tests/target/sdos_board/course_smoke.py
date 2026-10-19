"""Course firmware smoke test on a board: every core, both runtimes, via dspctl.

The host checks each locally built firmware against its core's carve-out,
uploads ``dspctl`` and the firmware with SHA-256 checks on the board, then runs
one shell script per core and runtime. The script drives ``dspctl`` through
load, start, status, health, echo, trace, and stop, and restores the core's original
firmware name. Each step is recorded with its exit status and output.
"""

from __future__ import annotations

import contextlib
import hashlib
import json
import shlex
import subprocess
import time
from pathlib import Path
from typing import Any, Callable, Sequence

from ci import firmware_elf

from .cores import CORES, Core
from .report import ssh_command

SCHEMA = "sdos-course-smoke/1"
ROOT = Path(__file__).resolve().parents[3]
REMOTE_DIR = "/tmp/sdos-smoke"
RUNTIMES = ("freertos", "baremetal")
EXTENSION = {"c6000": "xe66", "c7000": "xe71"}
STEPS = ("stop", "load", "start", "status", "health", "echo", "trace", "stop-again")

Runner = Callable[[Sequence[str], bytes | None], tuple[int, bytes]]


TIMED_OUT = 124  # exit status recorded when the board stops answering


def _run(argv: Sequence[str], stdin: bytes | None) -> tuple[int, bytes]:
    try:
        result = subprocess.run(list(argv), input=stdin, capture_output=True, check=False, timeout=300)
    except subprocess.TimeoutExpired as exc:
        partial = (exc.stdout or b"") + (exc.stderr or b"")
        return TIMED_OUT, partial + b"\n[no answer from the board for 300 s]\n"
    return result.returncode, result.stdout + result.stderr


class KernelLog(contextlib.AbstractContextManager):
    """Stream the board's kernel log into a local file while firmware runs.

    A fault that hangs the SoC takes SSH down with it; the messages already
    streamed here are then the only record of what happened."""

    def __init__(self, host: str, key: Path | None, path: Path) -> None:
        self.argv = ssh_command(host, key) + ["dmesg", "--follow-new", "--time-format", "iso"]
        self.path = path
        self.process: subprocess.Popen | None = None

    def __enter__(self) -> "KernelLog":
        with open(self.path, "wb") as sink:
            self.process = subprocess.Popen(self.argv, stdout=sink, stderr=subprocess.STDOUT,
                                            stdin=subprocess.DEVNULL)
        time.sleep(0.5)  # the follower is attached before the firmware starts
        return self

    def __exit__(self, *exc: object) -> None:
        if self.process is not None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()


def kernel_log(host: str, key: Path | None, path: Path, enabled: bool) -> contextlib.AbstractContextManager:
    return KernelLog(host, key, path) if enabled else contextlib.nullcontext()


def artifacts(build: Path = ROOT / "build") -> dict[str, Path]:
    """dspctl and each core/runtime firmware, from the release presets."""
    found = {"dspctl": build / "aarch64-gcc-release" / "host" / "dspctl"}
    for core in CORES.values():
        for runtime in RUNTIMES:
            found[f"{core.name}-{runtime}"] = (
                build / f"{core.name}-{runtime}-release" / "firmware"
                / f"sdos-{core.name}-{runtime}.{EXTENSION[core.family]}")
    return found


def check_artifacts(found: dict[str, Path]) -> list[str]:
    problems = [f"{name}: {path} is missing; run make build PRESET=board-host,firmware"
                for name, path in found.items() if not path.is_file()]
    for name, path in found.items():
        if name == "dspctl" or not path.is_file():
            continue
        core, runtime = name.rsplit("-", 1)
        report = firmware_elf.inspect(path, firmware_elf.load_core(core), require_trace=True, runtime=runtime)
        problems += [f"{name}: {problem}" for problem in report.problems]
    return problems


def upload(host: str, key: Path | None, local: Path, remote: str, mode: str, runner: Runner) -> None:
    digest = hashlib.sha256(local.read_bytes()).hexdigest()
    script = (
        f"set -e; mkdir -p {shlex.quote(str(Path(remote).parent))}; cat > {shlex.quote(remote)}.part; "
        f"echo {shlex.quote(digest + '  ' + remote + '.part')} | sha256sum -c --quiet; "
        f"chmod {mode} {shlex.quote(remote)}.part; mv {shlex.quote(remote)}.part {shlex.quote(remote)}"
    )
    code, _ = runner(ssh_command(host, key) + [script], local.read_bytes())
    if code != 0:
        raise RuntimeError(f"upload of {local.name} failed or its SHA-256 differed on the board (exit {code})")


def remote_script(core: Core, firmware: str, token: str) -> str:
    """One shell script per run: each step prints a marker, its output, and its status."""
    dspctl = f"{REMOTE_DIR}/dspctl"
    steps = {
        "stop": f"{dspctl} stop {core.name} || true",
        "load": f"{dspctl} load {core.name} {shlex.quote(firmware)}",
        "start": f"{dspctl} --timeout 10000 start {core.name}",
        "status": f"{dspctl} status {core.name}",
        "health": f"{dspctl} --json health {core.name}",
        "echo": f"{dspctl} echo {core.name} {shlex.quote(token)}",
        "trace": f"{dspctl} trace {core.name} | tail -n 30",
        "stop-again": f"{dspctl} stop {core.name}",
    }
    lines = ["rp=''", "for r in /sys/class/remoteproc/remoteproc*; do",
             f"  [ \"$(basename \"$(readlink -f \"$r/device\")\")\" = {shlex.quote(core.device)} ] && rp=$r",
             "done", 'original=$(cat "$rp/firmware")']
    for name in STEPS:
        lines += [f"echo '@@step {name}'", f"( {steps[name]} ) 2>&1", "echo \"@@status $?\""]
    lines += [f'{dspctl} stop {core.name} >/dev/null 2>&1',
              'echo "$original" > "$rp/firmware"',
              "echo '@@step restored'", 'echo "$(cat "$rp/state") $(cat "$rp/firmware") $original"',
              "echo '@@status 0'"]
    return "\n".join(lines) + "\n"


def parse(output: str) -> dict[str, dict[str, Any]]:
    steps: dict[str, dict[str, Any]] = {}
    current = None
    for line in output.splitlines():
        if line.startswith("@@step "):
            current = line.split(" ", 1)[1]
            steps[current] = {"output": [], "status": None}
        elif line.startswith("@@status ") and current is not None:
            steps[current]["status"] = int(line.split(" ", 1)[1])
        elif current is not None:
            steps[current]["output"].append(line)
    return steps


def evaluate(steps: dict[str, dict[str, Any]], token: str) -> dict[str, bool]:
    def ok(name: str) -> bool:
        return steps.get(name, {}).get("status") == 0

    restored = " ".join(steps.get("restored", {}).get("output", [])).split()
    return {
        "loaded": ok("load"),
        "started": ok("start") and any("running" in line for line in steps.get("start", {}).get("output", [])),
        "status": ok("status"),
        # The health line is readable through remoteproc and every start-up
        # check passed: protection, integrity, layout (docs/safety.md).
        "healthy": ok("health") and any('"state": "ok"' in line for line in steps.get("health", {}).get("output", [])),
        "echo": ok("echo") and token in steps.get("echo", {}).get("output", []),
        "stopped": ok("stop-again"),
        # "<state> <firmware now> <firmware before>"
        "restored": len(restored) == 3 and restored[0] == "offline" and restored[1] == restored[2],
    }


def run(host: str, key: Path | None, out: Path, cores: Sequence[Core], runner: Runner = _run,
        build: Path = ROOT / "build", follow_kernel: bool = False,
        runtimes: Sequence[str] = RUNTIMES) -> list[dict[str, Any]]:
    found = artifacts(build)
    problems = check_artifacts(found)
    if problems:
        raise RuntimeError("refusing to start: " + "; ".join(problems))
    upload(host, key, found["dspctl"], f"{REMOTE_DIR}/dspctl", "0755", runner)
    results = []
    out.mkdir(parents=True, exist_ok=True)
    for core in cores:
        for runtime in runtimes:
            local = found[f"{core.name}-{runtime}"]
            remote = f"{REMOTE_DIR}/{local.name}"
            upload(host, key, local, remote, "0644", runner)
            token = f"sdos-smoke {core.name} {runtime} {int(time.time())}"
            with kernel_log(host, key, out / f"{core.name}-{runtime}-kernel.txt", follow_kernel):
                code, output = runner(ssh_command(host, key) + ["sh", "-s"],
                                      remote_script(core, remote, token).encode())
            text = output.decode("utf-8", errors="replace")
            steps = parse(text)
            checks = evaluate(steps, token)
            result = {
                "schema": SCHEMA, "core": core.name, "runtime": runtime, "firmware": local.name,
                "captured_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "ssh_exit": code,
                "checks": checks, "passed": all(checks.values()), "steps": steps,
            }
            (out / f"{core.name}-{runtime}.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            results.append(result)
            if code == TIMED_OUT:
                return results  # the board stopped answering: nothing later can run
    return results
