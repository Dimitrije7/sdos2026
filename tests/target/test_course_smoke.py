#!/usr/bin/env python3
"""course-smoke against a simulated board shell; no board needed."""

from __future__ import annotations

import shlex
import tempfile
import unittest
from pathlib import Path

from ci.test_firmware_elf import elf32, resource_table
from tests.target.sdos_board import course_smoke
from tests.target.sdos_board.cores import CORES

LOAD_ADDRESS = {"c66x_0": (140, 0xA6100000), "c66x_1": (140, 0xA7100000), "c71x_0": (145, 0xA8200000)}


def fake_build(root: Path, broken: str | None = None) -> Path:
    for name, path in course_smoke.artifacts(root).items():
        path.parent.mkdir(parents=True, exist_ok=True)
        if name == "dspctl":
            path.write_bytes(b"\x7fELF dspctl")
            continue
        machine, address = LOAD_ADDRESS[name.rsplit("-", 1)[0]]
        if name == broken:
            address = 0xA8000000  # the C71x vring pool, which Linux allocates from
        path.write_bytes(elf32(machine, [(address, 0x100)], resource_table(trace_address=address)))
    return root


class ShellBoard:
    """Answers each step the way a healthy board with dspctl would."""

    def __init__(self, echo_works: bool = True) -> None:
        self.echo_works = echo_works
        self.uploads: list[str] = []

    def __call__(self, argv, stdin):
        if argv[-2:] != ["sh", "-s"]:  # an upload
            self.uploads.append(argv[-1].split("cat > ")[1].split(".part")[0])
            return 0, b""
        script = stdin.decode()
        commands = [line for line in script.splitlines() if line.startswith("( ")]
        echo_words = shlex.split(next(c for c in commands if " echo " in c)[2:].split(" ) 2>&1")[0])
        out = []
        for line in script.splitlines():
            if not line.startswith("echo '@@step "):
                continue
            step = line.split("@@step ")[1].rstrip("'")
            out.append(f"@@step {step}")
            status = 0
            if step == "start":
                out.append("running")
            elif step == "health":
                out.append('{"name": "x", "state": "ok", "fault": "none", "heartbeat": 3}')
            elif step == "echo":
                out.append(echo_words[-1] if self.echo_works else "dspctl: echo: did not answer")
                status = 0 if self.echo_works else 1
            elif step == "restored":
                out.append("offline j7-c66_0-fw j7-c66_0-fw")
            out.append(f"@@status {status}")
        return 0, ("\n".join(out) + "\n").encode()


class CourseSmokeTests(unittest.TestCase):
    def test_every_core_and_runtime_runs_and_is_restored(self) -> None:
        board = ShellBoard()
        with tempfile.TemporaryDirectory() as tmp:
            build = fake_build(Path(tmp) / "build")
            results = course_smoke.run("board", None, Path(tmp) / "out", list(CORES.values()), board, build)
            self.assertEqual(len(results), 6)
            self.assertTrue(all(r["passed"] for r in results), [r["checks"] for r in results])
            self.assertTrue((Path(tmp) / "out" / "c71x_0-baremetal.json").exists())
        self.assertIn("/tmp/sdos-smoke/dspctl", board.uploads)
        self.assertEqual(len(board.uploads), 7)  # dspctl and six firmware files

    def test_a_silent_core_fails_echo_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            build = fake_build(Path(tmp) / "build")
            results = course_smoke.run("board", None, Path(tmp) / "out", [CORES["c66x_1"]],
                                       ShellBoard(echo_works=False), build)
        self.assertEqual([r["checks"]["echo"] for r in results], [False, False])
        self.assertTrue(all(r["checks"]["started"] for r in results))

    def test_misplaced_firmware_is_refused_before_any_upload(self) -> None:
        board = ShellBoard()
        with tempfile.TemporaryDirectory() as tmp:
            build = fake_build(Path(tmp) / "build", broken="c71x_0-freertos")
            with self.assertRaisesRegex(RuntimeError, "c71x_0-freertos: segment 0xa8000000.*overlaps dma_pool"):
                course_smoke.run("board", None, Path(tmp) / "out", list(CORES.values()), board, build)
        self.assertEqual(board.uploads, [])

    def test_script_restores_the_original_firmware_name(self) -> None:
        script = course_smoke.remote_script(CORES["c66x_0"], "/tmp/sdos-smoke/x.xe66", "tok")
        self.assertIn("4d80800000.dsp", script)
        self.assertIn('echo "$original" > "$rp/firmware"', script)
        self.assertEqual(script.count("@@step"), len(course_smoke.STEPS) + 1)


if __name__ == "__main__":
    unittest.main()
