#!/usr/bin/env python3
"""Task 013 procedure tests: the on-board agent runs against a simulated kernel."""

from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from typing import Any

from ci.test_firmware_elf import elf32, resource_table
from tests.target.sdos_board import dsp_ipc
from tests.target.sdos_board.cores import CORES, load

CORE = CORES["c66x_0"]


def agent() -> dict[str, Any]:
    namespace: dict[str, Any] = {"__name__": "sdos_agent"}
    exec(compile(dsp_ipc.REMOTE_AGENT, "remote_agent", "exec"), namespace)  # noqa: S102 - our own source
    return namespace


class SimulatedBoard:
    """remoteproc and rpmsg_char as the agent sees them, with TI echo behavior."""

    def __init__(self, boots: bool = True, replies: bool = True) -> None:
        self.attrs = {"state": "offline", "firmware": "j7-c66_0-fw"}
        self.boots, self.replies = boots, replies
        self.open_endpoints = 0
        self.log: list[str] = []

    # Board interface
    def read(self, path: str) -> str:
        return self.attrs[path.rsplit("/", 1)[1]]

    def write(self, path: str, value: str) -> None:
        name = path.rsplit("/", 1)[1]
        self.log.append(f"{name}={value}")
        if name == "firmware":
            if self.attrs["state"] != "offline":
                raise OSError(16, "Device or resource busy")
            self.attrs["firmware"] = value
        elif value == "start" and self.boots:
            self.attrs["state"] = "running"
        elif value == "stop":
            self.attrs["state"] = "offline"

    def rproc_for(self, device: str) -> str | None:
        return "/sys/class/remoteproc/remoteproc0" if device == CORE.device else None

    def ctrl_for(self, device: str) -> str | None:
        return "/dev/rpmsg_ctrl0" if self.attrs["state"] == "running" else None

    def create_endpoint(self, ctrl: str, name: str, dst: int) -> str:
        assert (name, dst) == ("rpmsg_chrdev", 14)
        self.open_endpoints += 1
        return "/dev/rpmsg0"

    def open_endpoint(self, dev: str) -> int:
        return 3

    def exchange(self, fd: int, payload: bytes, timeout: float) -> bytes | None:
        if not self.replies:
            return None
        text = payload.decode()
        return (text.replace("ping", "pong", 1) if text.startswith("ping") else text).encode()

    def destroy_endpoint(self, fd: int) -> None:
        self.open_endpoints -= 1

    def trace(self, rproc: str) -> str:
        return "[0] IPC Echo test\n"


def run_agent(board: SimulatedBoard, cycles: int = 2) -> tuple[int, list[dict[str, Any]]]:
    plan = dsp_ipc.plan_for(CORE, "ipc_echo_test_c66xdsp_1_release_strip.xe66", count=3, cycles=cycles)
    plan["start_timeout"] = plan["stop_timeout"] = 0.05
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = agent()["run"](board, plan)
    return code, dsp_ipc.events(out.getvalue().encode())


class AgentTests(unittest.TestCase):
    def test_healthy_core_passes_two_cycles_and_is_restored(self) -> None:
        board = SimulatedBoard()
        code, events = run_agent(board)
        verdict = dsp_ipc.evaluate(events, cycles=2)
        self.assertEqual(code, 0)
        self.assertTrue(verdict["passed"], verdict)
        self.assertEqual(board.attrs, {"state": "offline", "firmware": "j7-c66_0-fw"})
        self.assertEqual(board.open_endpoints, 0)
        echo = [e for e in events if e["event"] == "echo"]
        self.assertEqual([e["received"] for e in echo], [4, 4])  # 3 pings + 1 payload echo

    def test_core_that_never_runs_fails_but_is_left_offline(self) -> None:
        board = SimulatedBoard(boots=False)
        code, events = run_agent(board)
        verdict = dsp_ipc.evaluate(events, cycles=2)
        self.assertEqual(code, 1)
        self.assertFalse(verdict["checks"]["started"])
        self.assertTrue(verdict["checks"]["left_offline"])
        self.assertEqual(board.attrs["firmware"], "j7-c66_0-fw")

    def test_silent_firmware_fails_echo_and_still_stops(self) -> None:
        board = SimulatedBoard(replies=False)
        code, events = run_agent(board, cycles=1)
        verdict = dsp_ipc.evaluate(events, cycles=1)
        self.assertEqual(code, 1)
        self.assertFalse(verdict["checks"]["echo"])
        self.assertTrue(verdict["checks"]["stopped"])
        self.assertEqual(board.open_endpoints, 0)

    def test_unknown_device_is_reported_without_touching_anything(self) -> None:
        board = SimulatedBoard()
        plan = dsp_ipc.plan_for(CORES["c71x_0"], "x.xe71")
        with contextlib.redirect_stdout(io.StringIO()) as out:
            code = agent()["run"](board, plan)
        self.assertEqual(code, 1)
        self.assertEqual(board.log, [])
        self.assertIn('"ok": false', out.getvalue())


class HostTests(unittest.TestCase):
    def test_firmware_for_another_core_is_refused_before_any_ssh(self) -> None:
        calls: list[Any] = []
        with tempfile.TemporaryDirectory() as tmp:
            firmware = Path(tmp) / "c66x_1.xe66"
            firmware.write_bytes(elf32(140, [(0xA7100000, 0x100)], resource_table()))
            with self.assertRaisesRegex(RuntimeError, "does not fit c66x_0"):
                dsp_ipc.run_core(CORE, "board", None, firmware, "00" * 32, "x", Path(tmp),
                                 runner=lambda argv, stdin: calls.append(argv) or (0, b""))
        self.assertEqual(calls, [])

    def test_core_identities_come_from_the_manifest(self) -> None:
        self.assertEqual(
            {name: core.device for name, core in load().items()},
            {"c66x_0": "4d80800000.dsp", "c66x_1": "4d81800000.dsp", "c71x_0": "64800000.dsp"},
        )
        with self.assertRaises(ValueError):
            dsp_ipc.resolve(["remoteproc0"])

    def test_run_core_verifies_the_upload_on_the_board_and_archives_logs(self) -> None:
        board = SimulatedBoard()
        calls: list[list[str]] = []

        def runner(argv, stdin):
            calls.append(list(argv))
            if "python3" in argv:
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    code = agent()["run"](board, json.loads(argv[-1].strip("'")))
                return code, out.getvalue().encode()
            if argv[-2:] == ["date", "+%s"]:
                return 0, b"1760000000\n"
            return 0, b"6.18.55-arm64-k3-r59\n" if argv[-1] == "-r" else b""

        with tempfile.TemporaryDirectory() as tmp:
            firmware = Path(tmp) / "ipc_echo_test_c66xdsp_1_release_strip.xe66"
            firmware.write_bytes(elf32(140, [(0xA6100000, 0x100)], resource_table()))
            result = dsp_ipc.run_core(CORE, "board", None, firmware, "ab" * 32, CORE.ti_echo_input,
                                      Path(tmp) / "out", count=2, cycles=2, runner=runner)
            self.assertTrue(result["passed"], result["checks"])
            self.assertTrue((Path(tmp) / "out" / "c66x_0-kernel.txt").exists())
            self.assertIn("IPC Echo test", (Path(tmp) / "out" / "c66x_0-trace.txt").read_text())
        upload = calls[0][-1]
        self.assertIn("sha256sum -c", upload)
        self.assertIn("/lib/firmware/sdos/ti-ipc/", upload)
        self.assertTrue(any("--since=@1760000000" in arg for arg in calls[3]))


if __name__ == "__main__":
    unittest.main()
