#!/usr/bin/env python3
"""Tests for the command facade's contract: selection, refusals, and exit codes."""

from __future__ import annotations

import contextlib
import io
import os
import unittest
from unittest import mock

from ci import facade


def run(*argv: str) -> tuple[int, str]:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
        code = facade.main(list(argv))
    return code, out.getvalue()


class SelectionTests(unittest.TestCase):
    def test_matrix_covers_every_required_variant(self) -> None:
        names = facade.presets()
        found = facade.groups(names)
        self.assertEqual(len(found["host"]), 4)  # GCC and Clang, debug and release
        self.assertEqual(len(found["board-host"]), 2)
        self.assertEqual(len(found["firmware"]), 12)  # 3 cores x 2 runtimes x 2 builds
        for core in ("c66x_0", "c66x_1", "c71x_0"):
            for runtime in ("freertos", "baremetal"):
                for build in ("debug", "release"):
                    self.assertIn(f"{core}-{runtime}-{build}", names)

    def test_groups_and_lists_expand_without_duplicates(self) -> None:
        names = facade.presets()
        self.assertEqual(
            facade.expand("host-gcc-debug,host", names),
            ["host-gcc-debug", "host-gcc-release", "host-clang-debug", "host-clang-release"],
        )

    def test_unknown_preset_is_a_usage_error(self) -> None:
        code, text = run("build", "--preset", "host-msvc-debug", "--dry-run")
        self.assertEqual(code, facade.EXIT_USAGE)
        self.assertIn("unknown preset 'host-msvc-debug'", text)


@mock.patch.dict(os.environ, {"SDOS_NATIVE": "1"})
class CommandTests(unittest.TestCase):
    def test_build_runs_configure_then_build(self) -> None:
        code, text = run("build", "--preset", "host-clang-release", "--dry-run")
        self.assertEqual(code, facade.EXIT_OK)
        self.assertIn("+ cmake --preset host-clang-release -DSDOS_WERROR=OFF", text)
        self.assertIn("+ cmake --build --preset host-clang-release", text)

    @mock.patch.object(facade, "_ti_installed", return_value=False)
    def test_quality_uses_werror_on_every_host_preset(self, _ti: mock.Mock) -> None:
        code, text = run("quality", "--dry-run")
        self.assertEqual(code, facade.EXIT_OK)
        self.assertEqual(text.count("-DSDOS_WERROR=ON"), 4)
        self.assertIn("ci/validate_versions.py", text)
        self.assertIn("+ hadolint toolchain/Dockerfile", text)
        self.assertIn("+ shellcheck ci/feasibility/", text)

    @mock.patch.object(facade, "_ti_installed", return_value=True)
    def test_quality_builds_every_firmware_preset_when_ti_is_installed(self, _ti: mock.Mock) -> None:
        code, text = run("quality", "--dry-run")
        self.assertEqual(code, facade.EXIT_OK)
        self.assertEqual(text.count("-DSDOS_WERROR=ON"), 16)
        self.assertEqual(text.count("+ ctest"), 4)  # DSP code is not run on the PC

    def test_tests_on_a_dsp_preset_are_not_available_yet(self) -> None:
        code, text = run("test", "--preset", "c71x_0-freertos-debug", "--dry-run")
        self.assertEqual(code, facade.EXIT_NOT_YET)
        self.assertIn("Task 033", text)

    def test_future_commands_fail_explicitly(self) -> None:
        for command, task in (("image", "Task 035"), ("package", "Task 022")):
            code, text = run(command)
            self.assertEqual(code, facade.EXIT_NOT_YET, command)
            self.assertIn(task, text)

    def test_board_commands_need_a_board(self) -> None:
        code, text = run("deploy")
        self.assertEqual(code, facade.EXIT_USAGE)
        self.assertIn("BOARD=<name>", text)
        code, _ = run("target-smoke", "--board", "bench-1")
        self.assertEqual(code, facade.EXIT_NOT_YET)

    def test_unknown_command_is_a_usage_error(self) -> None:
        code, _ = run("flash")
        self.assertEqual(code, facade.EXIT_USAGE)


if __name__ == "__main__":
    unittest.main()
