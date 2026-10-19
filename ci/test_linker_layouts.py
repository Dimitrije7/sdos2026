#!/usr/bin/env python3
"""Real-linker tests of the layout contract (Tasks 041 and 042).

Each case writes a variant of the default C66x_0 SDOS.cmd, relinks the
FreeRTOS release firmware with the pinned TI linker, and checks that the build
passes or fails for the stated reason. Valid alternatives must build; invalid
ownership, range, alignment, and protection layouts must be refused by the
post-link steps.
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
RUNTIME = ".sdos.runtime       : {} > L2SRAM"
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
                          .replace(RUNTIME, ".sdos.runtime       : {} > DDR_DATA"))
        self.assertEqual(moved.returncode, 0, moved.stdout[-2000:])

    def test_a_section_in_the_vring_pool_is_refused(self) -> None:
        result = self.link("pool", lambda s: s.replace(RUNTIME, ".sdos.runtime : {} load = SDOS_DMA_POOL_ORIGIN"))
        self.assert_refused(result, "overlaps dma_pool, which is reserved")

    def test_a_section_in_the_freertos_l2_cache_is_refused(self) -> None:
        def into_cache(s: str) -> str:
            s = s.replace("length = SDOS_L2SRAM_USABLE", "length = SDOS_L2SRAM_LENGTH")
            return s.replace(RUNTIME, ".sdos.runtime : {} load = SDOS_L2SRAM_ORIGIN + SDOS_L2SRAM_USABLE")
        self.assert_refused(self.link("cache", into_cache), "freertos runtime uses as cache")

    def test_a_misaligned_entry_point_is_refused(self) -> None:
        result = self.link("entry", lambda s: s.replace(
            ".text:_c_int00 : {} > DDR_CODE, align = 0x400",
            ".text:_c_int00 : {} load = SDOS_DDR_ORIGIN + 0x00100200"))
        self.assert_refused(result, "is not 0x400-aligned")

    def test_writable_data_in_the_read_only_zone_is_refused(self) -> None:
        result = self.link("rw_in_rodata", lambda s: s.replace(".fardata : {} > DDR_DATA", ".fardata : {} > DDR_RODATA"))
        self.assert_refused(result, "lies in the rodata zone")

    def test_code_outside_the_code_zone_is_refused(self) -> None:
        result = self.link("text_in_data", lambda s: s.replace(".text   : {} > DDR_CODE", ".text   : {} > DDR_DATA"))
        self.assert_refused(result, "outside the code zone")

    def test_a_misaligned_zone_is_refused(self) -> None:
        result = self.link("zone", lambda s: s.replace("#define RODATA_ORIGIN (SDOS_DDR_ORIGIN + 0x00500000)",
                                                       "#define RODATA_ORIGIN (SDOS_DDR_ORIGIN + 0x00500800)"))
        self.assert_refused(result, "is not a 4 KiB-aligned range")

    def test_zones_beyond_the_mpax_segments_are_refused(self) -> None:
        # Page-granular boundaries need many power-of-two segments each.
        result = self.link("mpax", lambda s: s.replace("#define RODATA_ORIGIN (SDOS_DDR_ORIGIN + 0x00500000)",
                                                       "#define RODATA_ORIGIN (SDOS_DDR_ORIGIN + 0x004ff000)")
                           .replace("#define DATA_ORIGIN   (SDOS_DDR_ORIGIN + 0x00600000)",
                                    "#define DATA_ORIGIN   (SDOS_DDR_ORIGIN + 0x005ff000)"))
        self.assert_refused(result, "MPAX segments")


if __name__ == "__main__":
    unittest.main()
