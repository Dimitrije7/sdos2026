#!/usr/bin/env python3
"""Real-linker tests of the layout contract (Task 041).

Each case writes a variant of the default C66x_0 SDOS.cmd, relinks the
FreeRTOS release firmware with the pinned TI linker, and checks that the build
passes or fails for the stated reason. Valid alternatives must build; invalid
ownership, range, and alignment must be refused by the post-link validator.
Skipped where the TI compilers are not installed (fork pull requests).
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "firmware" / "linker" / "c66x_0" / "SDOS.cmd"
PRESET = "c66x_0-freertos-release"
TI = Path(os.environ.get("SDOS_TI_C6000_ROOT", "/nonexistent"), "bin", "cl6x").exists() and shutil.which("cmake")


@unittest.skipUnless(TI, "needs the TI C6000 compiler (run in the TI container)")
class LinkerLayoutTests(unittest.TestCase):
    build: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.tmp = Path(tempfile.mkdtemp())
        cls.build = cls.tmp / "build"

    @classmethod
    def tearDownClass(cls) -> None:
        shutil.rmtree(cls.tmp)

    def link(self, name: str, edit) -> subprocess.CompletedProcess[str]:
        script = self.tmp / f"{name}.cmd"
        script.write_text(edit(DEFAULT.read_text(encoding="utf-8")), encoding="utf-8")
        configure = subprocess.run(
            ["cmake", "--preset", PRESET, "-B", str(self.build), f"-DSDOS_LINKER_SCRIPT={script}"],
            cwd=ROOT, capture_output=True, text=True, check=False)
        self.assertEqual(configure.returncode, 0, configure.stdout + configure.stderr)
        return subprocess.run(["cmake", "--build", str(self.build)], cwd=ROOT, capture_output=True, text=True,
                              check=False)

    def assert_refused(self, result: subprocess.CompletedProcess[str], reason: str) -> None:
        self.assertNotEqual(result.returncode, 0, "an invalid layout built")
        self.assertIn(reason, result.stdout + result.stderr)

    def test_default_and_a_reordered_layout_build(self) -> None:
        self.assertEqual(self.link("default", lambda s: s).returncode, 0)
        # A different but valid arrangement: stacks and runtime state in DDR.
        moved = self.link("ddr_stacks", lambda s: s.replace(".stack   : {} > L2SRAM", ".stack   : {} > DDR_DATA")
                          .replace(".sdos.runtime    : {} > L2SRAM", ".sdos.runtime    : {} > DDR_DATA"))
        self.assertEqual(moved.returncode, 0, moved.stdout[-2000:])

    def test_a_section_in_the_vring_pool_is_refused(self) -> None:
        result = self.link("pool", lambda s: s.replace(
            ".sdos.runtime    : {} > L2SRAM", ".sdos.runtime    : {} load = SDOS_DMA_POOL_ORIGIN"))
        self.assert_refused(result, "overlaps dma_pool, which is reserved")

    def test_a_section_in_the_freertos_l2_cache_is_refused(self) -> None:
        def into_cache(s: str) -> str:
            s = s.replace("    L2SRAM    (RWIX) : origin = SDOS_L2SRAM_ORIGIN,           length = SDOS_L2SRAM_USABLE",
                          "    L2SRAM    (RWIX) : origin = SDOS_L2SRAM_ORIGIN,           length = SDOS_L2SRAM_LENGTH")
            return s.replace(".sdos.runtime    : {} > L2SRAM",
                             ".sdos.runtime    : {} load = SDOS_L2SRAM_ORIGIN + SDOS_L2SRAM_USABLE")
        self.assert_refused(self.link("cache", into_cache), "freertos runtime uses as cache")

    def test_a_misaligned_entry_point_is_refused(self) -> None:
        result = self.link("entry", lambda s: s.replace(
            ".text:_c_int00 : {} > DDR_CODE, align = 0x400",
            ".text:_c_int00 : {} load = SDOS_DDR_ORIGIN + 0x00100200"))
        self.assert_refused(result, "is not 0x400-aligned")


if __name__ == "__main__":
    unittest.main()
