"""Fault-injection suite on a board: every safety mechanism must catch its fault.

For each core and runtime, the host checks and uploads the ``*-fi`` firmware
(built with ``-DSDOS_FAULT_INJECTION=ON``) and ``dspctl``, then runs one shell
script on the board. Per case the script starts the firmware, has it commit
one fault, reads the health line twice a few seconds apart, restarts the core,
and reads the health line again. The host classifies what the firmware
reported and compares it with what docs/safety.md promises for that core and
runtime. The other C66x is stopped while a core runs the suite, because one
case writes into that core's memory; its previous state is restored.

Nothing here runs without a board; tests/target/test_fault_injection.py
exercises the script and the classification against a simulated board.
"""

from __future__ import annotations

import json
import shlex
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence

from ci import firmware_elf

from .cores import Core
from .course_smoke import EXTENSION, REMOTE_DIR, ROOT, RUNTIMES, TIMED_OUT, Runner, _run, kernel_log, parse, upload
from .report import ssh_command

SCHEMA = "sdos-fault-injection/1"
# Whose memory the write case targets (fault_injection.c), stopped meanwhile.
NEIGHBOUR = {"c66x_0": "c66x_1", "c66x_1": "c66x_0", "c71x_0": "c66x_1"}
STALE_SECONDS = 3.5  # longer than the bare-metal monitor period and any case


@dataclass(frozen=True)
class Case:
    name: str
    algorithm: int
    params: str = ""  # hexadecimal bytes
    budget_us: int = 0
    run_timeout_ms: int = 3000
    # Acceptable outcomes by "family-runtime", runtime, family, or "*".
    expected: tuple[tuple[str, frozenset[str]], ...] = ()

    def expects(self, family: str, runtime: str) -> frozenset[str]:
        table = dict(self.expected)
        for key in (f"{family}-{runtime}", runtime, family, "*"):
            if key in table:
                return table[key]
        return frozenset()


def _e(**outcomes: set[str]) -> tuple[tuple[str, frozenset[str]], ...]:
    return tuple((key.replace("_", "-"), frozenset(value)) for key, value in outcomes.items())


CASES = (
    Case("write_foreign", 0xFA000001, expected=_e(**{"*": {"access"}})),
    # The C66x XMC refuses writes outside its grants but not reads (board,
    # Task 042; docs/safety.md): a read of foreign memory goes unnoticed there.
    Case("read_foreign", 0xFA000002, expected=_e(c6000={"undetected"}, c7000={"access"})),
    Case("write_code", 0xFA000003, expected=_e(**{"*": {"access"}})),
    # A blocked C66x instruction fetch may also stop the core outright.
    Case("exec_data", 0xFA000004, expected=_e(c6000={"access", "exception", "stale"},
                                               c7000={"access", "exception"})),
    # The bare-metal C71x wake tick doubles as watchdog; a bare-metal C66x has
    # no interrupt at all, so only its heartbeat stops (Linux stops it anyway).
    Case("hang", 0xFA000006, expected=_e(freertos={"watchdog"}, c7000_baremetal={"watchdog"},
                                          baremetal={"stale"})),
    Case("overrun", 0xFA000007, params="88130000", budget_us=1000,  # 5 ms against a 1 ms budget
         expected=_e(**{"*": {"deadline"}})),
    Case("corrupt_image", 0xFA000008, expected=_e(**{"*": {"integrity"}})),
    Case("heap", 0xFA000009, expected=_e(freertos={"heap"}, baremetal={"unsupported"})),
    Case("write_l2_code", 0xFA00000A, expected=_e(**{"*": {"access"}})),
    Case("read_shared", 0xFA00000B, expected=_e(c6000={"undetected"}, c7000={"access"})),
    # Last: on the C71x an overflow into the guard page still hangs the core
    # beyond recovery (Task 042 handoff), which would fail every later case.
    Case("stack", 0xFA000005, expected=_e(**{"*": {"stack"}})),
)


def artifacts(build: Path = ROOT / "build") -> dict[str, Path]:
    found = {"dspctl": build / "aarch64-gcc-release" / "host" / "dspctl"}
    for name, family in (("c66x_0", "c6000"), ("c66x_1", "c6000"), ("c71x_0", "c7000")):
        for runtime in RUNTIMES:
            found[f"{name}-{runtime}"] = (build / f"{name}-{runtime}-fi" / "firmware"
                                          / f"sdos-{name}-{runtime}-fi.{EXTENSION[family]}")
    return found


def check_artifacts(found: dict[str, Path]) -> list[str]:
    problems = [f"{name}: {path} is missing; run make build PRESET=board-host,firmware"
                for name, path in found.items() if not path.is_file()]
    for name, path in found.items():
        if name == "dspctl" or not path.is_file():
            continue
        core, runtime = name.rsplit("-", 1)
        report = firmware_elf.inspect(path, firmware_elf.load_core(core), require_trace=True, runtime=runtime,
                                      require_integrity=True)
        problems += [f"{name}: {problem}" for problem in report.problems]
    return problems


def remote_script(core: Core, firmware: str) -> str:
    dspctl = f"{REMOTE_DIR}/dspctl"
    neighbour = NEIGHBOUR[core.name]
    lines = ["rp=''", "for r in /sys/class/remoteproc/remoteproc*; do",
             f"  [ \"$(basename \"$(readlink -f \"$r/device\")\")\" = {shlex.quote(core.device)} ] && rp=$r",
             "done", 'original=$(cat "$rp/firmware")',
             f"was=$({dspctl} --json status {neighbour} 2>/dev/null | grep -c '\"state\": \"running\"')",
             f"{dspctl} stop {neighbour} >/dev/null 2>&1",
             f"{dspctl} stop {core.name} >/dev/null 2>&1",
             f"echo '@@step load'; ( {dspctl} load {core.name} {shlex.quote(firmware)} ) 2>&1; echo \"@@status $?\""]
    for case in CASES:
        run = (f"{dspctl} --timeout {case.run_timeout_ms} --budget-us {case.budget_us} run {core.name} "
               f"{case.algorithm:#x} - - {shlex.quote(case.params)}")  # no data: no shared buffer needed
        for step, command in ((f"{case.name}:start", f"{dspctl} --timeout 10000 recover {core.name}"),
                              (f"{case.name}:run", run),
                              (f"{case.name}:health", f"sleep 1; {dspctl} --json health {core.name}"),
                              (f"{case.name}:later", f"sleep {STALE_SECONDS}; {dspctl} --json health {core.name}"),
                              (f"{case.name}:recover", f"{dspctl} --timeout 10000 recover {core.name} && "
                                                       f"{dspctl} --json health {core.name}")):
            lines += [f"echo '@@step {step}'", f"( {command} ) 2>&1", "echo \"@@status $?\""]
    lines += [f"{dspctl} stop {core.name} >/dev/null 2>&1", 'echo "$original" > "$rp/firmware"',
              f"[ \"$was\" = 1 ] && {dspctl} start {neighbour} >/dev/null 2>&1",
              "echo '@@step restored'", 'echo "$(cat "$rp/state") $(cat "$rp/firmware") $original"',
              "echo '@@status 0'"]
    return "\n".join(lines) + "\n"


def _health(step: dict[str, Any] | None) -> dict[str, Any] | None:
    for line in (step or {}).get("output", []):
        if line.startswith("{"):
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                return None
            return value if isinstance(value, dict) else None
    return None


def classify(steps: dict[str, dict[str, Any]], case: Case) -> str:
    """What the firmware reported after the case ran."""
    health = _health(steps.get(f"{case.name}:health"))
    later = _health(steps.get(f"{case.name}:later"))
    run_output = " ".join(steps.get(f"{case.name}:run", {}).get("output", []))
    if health is None:
        return "unreadable"
    if health.get("state") in ("faulted", "halted"):
        return str(health.get("fault"))
    if later is not None and later.get("state") in ("faulted", "halted"):
        return str(later.get("fault"))
    if later is not None and later.get("heartbeat") == health.get("heartbeat"):
        return "stale"
    if "deadline missed" in run_output:
        return "deadline"
    if "unknown algorithm" in run_output:
        return "unsupported"
    return "undetected"


def evaluate(steps: dict[str, dict[str, Any]], core: Core, runtime: str) -> dict[str, dict[str, Any]]:
    results = {}
    for case in CASES:
        outcome = classify(steps, case)
        recovered = _health(steps.get(f"{case.name}:recover"))
        expected = case.expects(core.family, runtime)
        results[case.name] = {
            "outcome": outcome, "expected": sorted(expected),
            "detected": outcome in expected,
            "recovered": recovered is not None and recovered.get("state") == "ok",
        }
    return results


def run(host: str, key: Path | None, out: Path, cores: Sequence[Core], runner: Runner = _run,
        build: Path = ROOT / "build", follow_kernel: bool = False,
        runtimes: Sequence[str] = RUNTIMES) -> list[dict[str, Any]]:
    found = artifacts(build)
    problems = check_artifacts(found)
    if problems:
        raise RuntimeError("refusing to start: " + "; ".join(problems))
    upload(host, key, found["dspctl"], f"{REMOTE_DIR}/dspctl", "0755", runner)
    out.mkdir(parents=True, exist_ok=True)
    results = []
    for core in cores:
        for runtime in runtimes:
            local = found[f"{core.name}-{runtime}"]
            remote = f"{REMOTE_DIR}/{local.name}"
            upload(host, key, local, remote, "0644", runner)
            with kernel_log(host, key, out / f"{core.name}-{runtime}-kernel.txt", follow_kernel):
                code, output = runner(ssh_command(host, key) + ["sh", "-s"], remote_script(core, remote).encode())
            steps = parse(output.decode("utf-8", errors="replace"))
            cases = evaluate(steps, core, runtime)
            restored = " ".join(steps.get("restored", {}).get("output", [])).split()
            result = {
                "schema": SCHEMA, "core": core.name, "runtime": runtime, "firmware": local.name,
                "captured_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "ssh_exit": code,
                "loaded": steps.get("load", {}).get("status") == 0, "cases": cases,
                "restored": len(restored) == 3 and restored[0] == "offline" and restored[1] == restored[2],
                "steps": steps,
            }
            result["passed"] = (result["loaded"] and result["restored"]
                                and all(c["detected"] and c["recovered"] for c in cases.values()))
            (out / f"{core.name}-{runtime}.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            results.append(result)
            if code == TIMED_OUT:
                return results  # the board stopped answering: nothing later can run
    return results
