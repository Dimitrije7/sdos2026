"""Tests for ci/dt_memory.py: the device-tree parser and the memory-map audit.

The audit cases start from the committed board extraction and the two
manifests, which agree, and break one thing each.
"""

from __future__ import annotations

import copy
import json
import struct
import unittest
from pathlib import Path

from ci import dt_memory

ROOT = Path(__file__).resolve().parents[1]


def _fdt(root: dict, memreserve: tuple[tuple[int, int], ...] = ()) -> bytes:
    """A flattened device tree from {"props": {...}, "children": {name: node}}."""
    strings = bytearray()
    offsets: dict[str, int] = {}
    body = bytearray()

    def word(value: int) -> None:
        body.extend(struct.pack(">I", value))

    def pad() -> None:
        while len(body) % 4:
            body.append(0)

    def node(name: str, spec: dict) -> None:
        word(1)
        body.extend(name.encode() + b"\0")
        pad()
        for key, value in spec.get("props", {}).items():
            if key not in offsets:
                offsets[key] = len(strings)
                strings.extend(key.encode() + b"\0")
            word(3)
            word(len(value))
            word(offsets[key])
            body.extend(value)
            pad()
        for child, child_spec in spec.get("children", {}).items():
            node(child, child_spec)
        word(2)

    node("", root)
    word(9)
    rsv = b"".join(struct.pack(">QQ", a, s) for a, s in memreserve) + struct.pack(">QQ", 0, 0)
    off_rsv = 40
    off_struct = off_rsv + len(rsv)
    off_strings = off_struct + len(body)
    total = off_strings + len(strings)
    header = struct.pack(">10I", 0xD00DFEED, total, off_struct, off_strings, off_rsv, 17, 16, 0,
                         len(strings), len(body))
    return header + rsv + bytes(body) + bytes(strings)


def cells(*values: int) -> bytes:
    return struct.pack(f">{len(values)}I", *values)


def text(*values: str) -> bytes:
    return b"".join(v.encode() + b"\0" for v in values)


SMALL = {
    "props": {"model": text("Test board"), "#address-cells": cells(2), "#size-cells": cells(2)},
    "children": {
        "memory@80000000": {"props": {"device_type": text("memory"), "reg": cells(0, 0x80000000, 0, 0x80000000)}},
        "reserved-memory": {
            "props": {"#address-cells": cells(2), "#size-cells": cells(2)},
            "children": {
                "pool@a0000000": {"props": {"compatible": text("shared-dma-pool"), "no-map": b"",
                                            "reg": cells(0, 0xA0000000, 0, 0x100000), "phandle": cells(7)}},
                "fw@a0100000": {"props": {"no-map": b"", "reg": cells(0, 0xA0100000, 0, 0xF00000),
                                          "phandle": cells(8)}},
            },
        },
        "bus@100000": {
            "props": {"#address-cells": cells(2), "#size-cells": cells(2)},
            "children": {
                "dsp@64800000": {"props": {
                    "compatible": text("ti,j721e-c71-dsp"), "firmware-name": text("fw-name"),
                    "reg": cells(0, 0x64800000, 0, 0x80000, 0, 0x64E00000, 0, 0xC000),
                    "reg-names": text("l2sram", "l1dram"), "memory-region": cells(7, 8)}},
                "sram@70000000": {
                    "props": {"compatible": text("mmio-sram"), "reg": cells(0, 0x70000000, 0, 0x7F0000),
                              "#address-cells": cells(1), "#size-cells": cells(1)},
                    "children": {"atf-sram@0": {"props": {"reg": cells(0, 0x20000)}}},
                },
            },
        },
    },
}


class ParseTest(unittest.TestCase):
    def test_extracts_memory_facts(self):
        dt = dt_memory.extract(_fdt(SMALL, ((0x9E800000, 0x1000),)))
        self.assertEqual(dt["model"], "Test board")
        self.assertEqual(dt["memory"], [{"base": "0x80000000", "size": "0x80000000"}])
        self.assertEqual(dt["memreserve"], [{"base": "0x9e800000", "size": "0x1000"}])
        pool, fw = dt["reserved_memory"]
        self.assertEqual(pool["reg"], [{"base": "0xa0000000", "size": "0x100000"}])
        self.assertTrue(pool["no_map"])
        self.assertEqual(pool["used_by"], ["/bus@100000/dsp@64800000"])
        self.assertNotIn("compatible", fw)
        (core,) = dt["remote_cores"]
        self.assertEqual(core["device"], "64800000.dsp")
        self.assertEqual(core["memory_region"], ["pool@a0000000", "fw@a0100000"])
        self.assertEqual(core["local"]["l2sram"], {"base": "0x64800000", "size": "0x80000"})
        (sram,) = dt["srams"]
        self.assertEqual(sram["carveouts"], [{"node": "atf-sram@0", "base": "0x70000000", "size": "0x20000"}])

    def test_refuses_what_is_not_a_device_tree(self):
        with self.assertRaises(ValueError):
            dt_memory.extract(b"\0" * 64)
        blob = bytearray(_fdt(SMALL))
        struct.pack_into(">I", blob, 0, 0x12345678)
        with self.assertRaises(ValueError):
            dt_memory.extract(bytes(blob))


class AuditTest(unittest.TestCase):
    def setUp(self):
        self.map = json.loads((ROOT / "manifest" / "memory-map.json").read_text())
        self.cores = json.loads((ROOT / "manifest" / "dsp-cores.json").read_text())
        self.dt = json.loads((ROOT / self.map["evidence"]).read_text())

    def problems(self, memory_map=None, cores=None, dt=None):
        return dt_memory.check(memory_map or self.map, cores or self.cores, dt or self.dt)

    def region(self, memory_map, node):
        return next(r for r in memory_map["reserved_memory"] if r["node"] == node)

    def test_committed_contract_agrees_with_the_board(self):
        self.assertEqual(self.problems(), [])

    def test_overlap_is_refused(self):
        m = copy.deepcopy(self.map)
        self.region(m, "memory@a7000000")["base"] = "0xa6f00000"
        self.assertTrue(any("overlap" in p for p in self.problems(memory_map=m)))

    def test_misalignment_is_refused(self):
        m = copy.deepcopy(self.map)
        self.region(m, "memory@aa000000")["size"] = "0x1c00800"
        self.assertTrue(any("4 KiB pages" in p for p in self.problems(memory_map=m)))

    def test_out_of_range_is_refused(self):
        m = copy.deepcopy(self.map)
        self.region(m, "optee@9e800000")["base"] = "0x10000000"
        self.assertTrue(any("outside every DDR bank" in p for p in self.problems(memory_map=m)))

    def test_owner_and_source_are_required(self):
        m = copy.deepcopy(self.map)
        del self.region(m, "memory@a8100000")["owner"]
        self.region(m, "memory@a8000000")["source"] = ""
        found = self.problems(memory_map=m)
        self.assertTrue(any("memory@a8100000: no owner" in p for p in found))
        self.assertTrue(any("memory@a8000000: no source" in p for p in found))

    def test_new_reservation_needs_an_owner(self):
        dt = copy.deepcopy(self.dt)
        dt["reserved_memory"].append({"node": "new@b0000000", "reg": [{"base": "0xb0000000", "size": "0x100000"}],
                                      "no_map": True, "used_by": []})
        self.assertIn("new@b0000000: reserved in the device tree but not in the memory map (no owner)",
                      self.problems(dt=dt))

    def test_moved_reservation_is_noticed(self):
        dt = copy.deepcopy(self.dt)
        next(r for r in dt["reserved_memory"] if r["node"] == "memory@a6100000")["reg"][0]["size"] = "0xe00000"
        self.assertTrue(any(p.startswith("memory@a6100000: memory map says") for p in self.problems(dt=dt)))

    def test_dsp_carveout_must_be_its_memory_region(self):
        cores = copy.deepcopy(self.cores)
        ddr = next(r for r in cores["cores"][0]["regions"] if r["name"] == "ddr")
        ddr["base"] = "0xa7100000"  # the other C66x's carve-out
        self.assertTrue(any(p.startswith("c66x_0: ddr 0xa7100000") for p in self.problems(cores=cores)))

    def test_swapped_memory_regions_are_noticed(self):
        dt = copy.deepcopy(self.dt)
        core = next(c for c in dt["remote_cores"] if c["device"] == "64800000.dsp")
        core["memory_region"].reverse()
        self.assertTrue(any(p.startswith("c71x_0: dma_pool") for p in self.problems(dt=dt)))

    def test_local_memory_larger_than_the_hardware_is_refused(self):
        cores = copy.deepcopy(self.cores)
        l2 = next(r for r in cores["cores"][2]["regions"] if r["name"] == "l2sram")
        l2["size"] = "0x100000"
        self.assertTrue(any("exceeds the device tree" in p for p in self.problems(cores=cores)))

    def test_msmc_must_avoid_the_firmware_carveout(self):
        cores = copy.deepcopy(self.cores)
        msmc = next(r for r in cores["cores"][1]["regions"] if r["name"] == "msmc")
        msmc["base"] = "0x70000000"
        self.assertTrue(any("atf-sram@0" in p for p in self.problems(cores=cores)))

    def test_cli_defaults_to_the_committed_evidence(self):
        self.assertEqual(dt_memory.main([]), 0)


if __name__ == "__main__":
    unittest.main()
