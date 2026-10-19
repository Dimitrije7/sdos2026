#!/usr/bin/env python3
"""fault-injection against a simulated board; no board needed."""

from __future__ import annotations

import json
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

from ci import firmware_elf
from ci.test_firmware_elf import image_elf, resource_table, with_record
from tests.target.sdos_board import fault_injection as fi
from tests.target.sdos_board.cores import CORES

BASE = {"c66x_0": (140, 0xA6100000, 0xA6200000), "c66x_1": (140, 0xA7100000, 0xA7200000),
        "c71x_0": (145, 0xA8100000, 0xA8200000)}


def fake_firmware(core: str) -> bytes:
    machine, meta, code = BASE[core]
    marker = struct.pack("<II", firmware_elf.INTEGRITY_MAGIC, 1).ljust(firmware_elf.INTEGRITY_SIZE, b"\0")
    return with_record(image_elf([(".text", code, 5, bytes(range(256))),
                                  (".resource_table", meta, 4, resource_table(trace_address=meta + 0x10000)),
                                  (firmware_elf.INTEGRITY_SECTION, meta + 0x2000, 4, marker)], machine))


def fake_build(root: Path) -> Path:
    for name, path in fi.artifacts(root).items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"\x7fELF dspctl" if name == "dspctl" else fake_firmware(name.rsplit("-", 1)[0]))
    return root


def health(state: str = "ok", fault: str = "none", heartbeat: int = 1) -> str:
    return json.dumps({"name": "x", "state": state, "fault": fault, "heartbeat": heartbeat})


class SimulatedBoard:
    """Reports, for each case, what docs/safety.md promises, except `misses`."""

    def __init__(self, family: str, runtime: str, misses: frozenset[str] = frozenset()) -> None:
        self.family, self.runtime, self.misses = family, runtime, misses

    def output(self) -> str:
        lines = ["@@step load", "loaded", "@@status 0"]
        for case in fi.CASES:
            outcome = sorted(case.expects(self.family, self.runtime))[0]
            if case.name in self.misses:
                outcome = "undetected"
            run_text = {"deadline": "dspctl: run: deadline missed", "unsupported": "unknown algorithm"}.get(outcome, "")
            first, later = health(), health(heartbeat=2)
            if outcome == "stale":
                later = health(heartbeat=1)
            elif outcome not in ("deadline", "unsupported", "undetected"):
                first = health("faulted", outcome)
            lines += [f"@@step {case.name}:start", "running", "@@status 0",
                      f"@@step {case.name}:run", run_text, "@@status 1",
                      f"@@step {case.name}:health", first, "@@status 0",
                      f"@@step {case.name}:later", later, "@@status 0",
                      f"@@step {case.name}:recover", "running", health(), "@@status 0"]
        lines += ["@@step restored", "offline j7-fw j7-fw", "@@status 0"]
        return "\n".join(lines) + "\n"

    def __call__(self, argv, stdin):
        if argv[-2:] != ["sh", "-s"]:
            return 0, b""
        return 0, self.output().encode()


class FaultInjectionTests(unittest.TestCase):
    def test_the_board_script_is_valid_shell_and_drives_every_case(self) -> None:
        script = fi.remote_script(CORES["c71x_0"], "/tmp/sdos-smoke/sdos-c71x_0-freertos-fi.xe71")
        checked = subprocess.run(["sh", "-n"], input=script.encode(), capture_output=True, check=False)
        self.assertEqual(checked.returncode, 0, checked.stderr)
        for case in fi.CASES:
            self.assertIn(f"{case.algorithm:#x}", script)
        self.assertIn("stop c66x_1", script)  # the C71x writes into C66x_1's memory
        self.assertIn("--budget-us 1000 run c71x_0 0xfa000007", script)

    def test_classification(self) -> None:
        case = fi.CASES[0]
        steps = {f"{case.name}:health": {"output": [health("halted", "access")]}}
        self.assertEqual(fi.classify(steps, case), "access")
        steps = {f"{case.name}:health": {"output": [health()]}, f"{case.name}:later": {"output": [health()]}}
        self.assertEqual(fi.classify(steps, case), "stale")
        steps[f"{case.name}:later"] = {"output": [health(heartbeat=9)]}
        self.assertEqual(fi.classify(steps, case), "undetected")
        self.assertEqual(fi.classify({}, case), "unreadable")

    def test_expectations_differ_by_runtime_where_the_mechanism_does(self) -> None:
        hang = next(c for c in fi.CASES if c.name == "hang")
        self.assertEqual(hang.expects("c6000", "freertos"), {"watchdog"})
        self.assertEqual(hang.expects("c7000", "baremetal"), {"watchdog"})  # the wake tick is its watchdog
        self.assertEqual(hang.expects("c6000", "baremetal"), {"stale"})

    def test_a_full_run_passes_when_every_mechanism_reports(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build = fake_build(Path(tmp) / "build")
            results = fi.run("board", None, Path(tmp) / "out", [CORES["c66x_0"]],
                             runner=SimulatedBoard("c6000", "freertos"), build=build)
            self.assertTrue(results[0]["passed"], json.dumps(results[0]["cases"], indent=1))

    def test_a_missed_fault_fails_the_run(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build = fake_build(Path(tmp) / "build")
            results = fi.run("board", None, Path(tmp) / "out", [CORES["c71x_0"]],
                             runner=SimulatedBoard("c7000", "freertos", frozenset({"stack"})), build=build)
            self.assertFalse(results[0]["passed"])
            self.assertEqual(results[0]["cases"]["stack"]["outcome"], "undetected")

    def test_firmware_without_an_integrity_record_is_refused(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build = fake_build(Path(tmp) / "build")
            path = fi.artifacts(build)["c66x_1-baremetal"]
            data = bytearray(path.read_bytes())
            data[data.index(bytes(range(256)))] ^= 0xFF  # change code after the record was written
            path.write_bytes(bytes(data))
            with self.assertRaisesRegex(RuntimeError, "integrity record does not match"):
                fi.run("board", None, Path(tmp) / "out", [CORES["c66x_1"]], runner=SimulatedBoard("c6000", "x"),
                       build=build)


if __name__ == "__main__":
    unittest.main()
