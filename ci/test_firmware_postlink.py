#!/usr/bin/env python3
"""Post-link planners: the Python twins agree with the firmware's C planners."""

from __future__ import annotations

import unittest

from ci import firmware_elf, firmware_postlink


class PlannerTests(unittest.TestCase):
    def test_mpax_split_matches_the_c_planner(self) -> None:
        # The same cases as firmware/common/safety/tests/test_safety.c.
        self.assertEqual(firmware_postlink.mpax_split(0xA6100000, 0xA7000000), 4)
        self.assertEqual(firmware_postlink.mpax_split(0, 1 << 32), 1)
        self.assertEqual(firmware_postlink.mpax_split(0xA6100000, 0xA6100000), 0)

    def test_the_default_c66x_layout_fits_the_mpax_with_its_device_windows(self) -> None:
        core = firmware_elf.load_core("c66x_0")
        zones = {"code": (0xA6400000, 0xA6800000), "rodata": (0xA6200000, 0xA6400000), "l2code": (0x800000, 0x804000)}
        # 7 window segments, 4 for the carve-out, 1 pool, 1 read-only data, 1 code.
        self.assertEqual(firmware_postlink.mpax_segments(core, zones), 14 - 1)

    def test_mmu_tables_count_split_blocks_and_gigabytes(self) -> None:
        class Elf:
            symbols: dict[str, int] = {}

        core = {"regions": [{"name": "l2sram", "base": "0x64800000", "size": "0x70000"},
                            {"name": "ddr", "base": "0xa8100000", "size": "0xf00000"},
                            {"name": "dma_pool", "base": "0xa8000000", "size": "0x100000"}],
                "mmio_windows": [{"base": "0x0", "size": "0x20000000"}]}
        zones = {"code": (0xA8200000, 0xA8A00000), "rodata": (0xA8A00000, 0xA8C00000)}
        # GiB 0, 1, 2; blocks split at 0x64870000 (L2 end) and 0xa8100000 (pool and carve-out).
        self.assertEqual(firmware_postlink.mmu_tables(Elf(), core, zones, []), 1 + 3 + 2)
        # A guard page in a new block costs one more table.
        self.assertEqual(firmware_postlink.mmu_tables(Elf(), core, zones, [0xA8D42000]), 1 + 3 + 3)


if __name__ == "__main__":
    unittest.main()
