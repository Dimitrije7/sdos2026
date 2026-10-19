#!/usr/bin/env python3
"""Firmware ELF checks against synthetic files; no TI binary enters the repository."""

from __future__ import annotations

import struct
import tempfile
import unittest
from pathlib import Path

from ci import firmware_elf


def resource_table(trace: bool = True, rings: int = 2, trace64: bool = False, trace_address: int = 0xA6580000) -> bytes:
    entries = []
    vdev = struct.pack("<IIIIIIBBxx", firmware_elf.RSC_VDEV, 7, 0, 1, 0, 0, 0, rings)
    vdev += b"".join(struct.pack("<IIIII", 0xFFFFFFFF, 4096, 256, n + 1, 0) for n in range(rings))
    entries.append(vdev)
    if trace and trace64:  # TI's C7x layout: the 64-bit address is 8-byte aligned after the type
        entries.append(struct.pack("<IIQII", firmware_elf.RSC_TRACE64, 0, 0xA8C10000, 0x10000, 0)
                       + b"trace:c7x".ljust(32, b"\0"))
    elif trace:
        entries.append(struct.pack("<IIII", firmware_elf.RSC_TRACE, trace_address, 0x1000, 0) + b"trace:dsp".ljust(32, b"\0"))
    header_size = 16 + 4 * len(entries)
    offsets, body = [], b""
    for entry in entries:
        offsets.append(header_size + len(body))
        body += entry
    return struct.pack("<IIII", 1, len(entries), 0, 0) + struct.pack(f"<{len(entries)}I", *offsets) + body


def elf32(machine: int, loads: list[tuple[int, int]], table: bytes | None) -> bytes:
    """A minimal little-endian ELF32 with PT_LOAD headers and optional .resource_table."""
    names = b"\0.shstrtab\0.resource_table\0"
    phoff = 52
    data_off = phoff + 32 * len(loads)
    blob_off = data_off
    names_off = blob_off + (len(table) if table else 0)
    shoff = names_off + len(names)
    sections = [struct.pack("<10I", *([0] * 10)), struct.pack("<10I", 1, 3, 0, 0, names_off, len(names), 0, 0, 1, 0)]
    if table:
        sections.append(struct.pack("<10I", 11, 1, 2, loads[0][0], blob_off, len(table), 0, 0, 4096, 0))
    header = b"\x7fELF" + bytes([1, 1, 1]) + bytes(9)
    header += struct.pack("<HHIIIIIHHHHHH", 2, machine, 1, loads[0][0], phoff, shoff, 0, 52, 32, len(loads), 40,
                          len(sections), 1)
    programs = b"".join(struct.pack("<8I", 1, data_off, address, address, 0, size, 6, 8) for address, size in loads)
    return header + programs + (table or b"") + names + b"".join(sections)


def image_elf(parts: list[tuple[str, int, int, bytes]], machine: int = 140) -> bytes:
    """An ELF32 whose parts (name, address, p_flags, bytes) are each one PT_LOAD
    segment holding one section of that name, with real file contents."""
    names = b"\0.shstrtab\0" + b"".join(name.encode() + b"\0" for name, _a, _f, _d in parts)
    phoff = 52
    data_off = phoff + 32 * len(parts)
    offsets, blob = [], b""
    for _name, _address, _flags, data in parts:
        offsets.append(data_off + len(blob))
        blob += data
    names_off = data_off + len(blob)
    shoff = names_off + len(names)
    sections = [struct.pack("<10I", *([0] * 10)), struct.pack("<10I", 1, 3, 0, 0, names_off, len(names), 0, 0, 1, 0)]
    name_at = 11
    for (name, address, flags, data), offset in zip(parts, offsets):
        sh_flags = 2 | (4 if flags & 1 else 0) | (1 if flags & 2 else 0)
        sections.append(struct.pack("<10I", name_at, 1, sh_flags, address, offset, len(data), 0, 0, 4, 0))
        name_at += len(name) + 1
    header = b"\x7fELF" + bytes([1, 1, 1]) + bytes(9)
    header += struct.pack("<HHIIIIIHHHHHH", 2, machine, 1, parts[0][1], phoff, shoff, 0, 52, 32, len(parts), 40,
                          len(sections), 1)
    programs = b"".join(struct.pack("<8I", 1, offset, address, address, len(data), len(data), flags, 4)
                        for (_n, address, flags, data), offset in zip(parts, offsets))
    return header + programs + blob + names + b"".join(sections)


def with_record(data: bytes) -> bytes:
    """What ci/firmware_postlink.py does: write the record into .sdos.integrity."""
    elf = firmware_elf.read_elf(data)
    section = elf.section(firmware_elf.INTEGRITY_SECTION)
    assert section is not None
    record = firmware_elf.integrity_record(firmware_elf.integrity_ranges(elf))
    return data[: section.offset] + record + data[section.offset + len(record) :]


class IntegrityTests(unittest.TestCase):
    CODE = bytes(range(256)) * 4
    RODATA = b"coefficients" * 10

    def image(self, code_flags: int = 5) -> bytes:
        marker = struct.pack("<II", firmware_elf.INTEGRITY_MAGIC, 1).ljust(firmware_elf.INTEGRITY_SIZE, b"\0")
        return image_elf([(".text", 0xA6200000, code_flags, self.CODE), (".const", 0xA6600000, 4, self.RODATA),
                          (".resource_table", 0xA6111000, 4, b"\x01" + bytes(15)),
                          (firmware_elf.INTEGRITY_SECTION, 0xA6110000, 4, marker)])

    def test_the_record_covers_read_only_segments_except_linux_written_ones(self) -> None:
        ranges = firmware_elf.integrity_ranges(firmware_elf.read_elf(self.image()))
        self.assertEqual([(a, n) for a, n, _c in ranges], [(0xA6200000, len(self.CODE)), (0xA6600000, len(self.RODATA))])
        self.assertEqual(ranges[0][2], firmware_elf.crc32c(self.CODE))

    def test_a_patched_image_passes_and_any_later_change_is_caught(self) -> None:
        data = with_record(self.image())
        self.assertEqual(firmware_elf.integrity_problems(firmware_elf.read_elf(data)), [])
        code_at = data.index(self.CODE)
        changed = data[:code_at] + b"\xff" + data[code_at + 1 :]
        self.assertIn("does not match the image", firmware_elf.integrity_problems(firmware_elf.read_elf(changed))[0])

    def test_an_unpatched_image_is_refused(self) -> None:
        problems = firmware_elf.integrity_problems(firmware_elf.read_elf(self.image()))
        self.assertIn("does not match the image", problems[0])

    def test_writable_and_executable_segments_are_refused(self) -> None:
        problems = firmware_elf.integrity_problems(firmware_elf.read_elf(with_record(self.image(code_flags=7))))
        self.assertIn("both writable and executable", problems[0])

    def test_the_record_format_matches_the_firmware_parser(self) -> None:
        record = firmware_elf.integrity_record([(0xA6200000, 16, 0x12345678)])
        magic, version, count, crc = struct.unpack_from("<IIII", record)
        self.assertEqual((magic, version, count, len(record)), (0x52494453, 1, 1, firmware_elf.INTEGRITY_SIZE))
        self.assertEqual(crc, firmware_elf.crc32c(record[:12] + record[16:32]))


class FirmwareElfTests(unittest.TestCase):
    def check(self, core: str, data: bytes, require_trace: bool = False) -> firmware_elf.Report:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "fw.out"
            path.write_bytes(data)
            return firmware_elf.inspect(path, firmware_elf.load_core(core), require_trace)

    def test_firmware_inside_its_carveout_with_rpmsg_fits(self) -> None:
        report = self.check("c66x_0", elf32(140, [(0xA6100000, 0x8C), (0xA6200000, 0x1000)], resource_table()), True)
        self.assertEqual(report.problems, [])
        self.assertEqual([r["type"] for r in report.resources], ["vdev", "trace"])
        self.assertEqual(len(report.resources[0]["vrings"]), 2)

    def test_segment_in_another_cores_memory_is_refused(self) -> None:
        report = self.check("c66x_1", elf32(140, [(0xA6100000, 0x8C)], resource_table()))
        self.assertIn("outside every memory region of c66x_1", report.problems[0])

    def test_local_l2_sram_is_loadable_up_to_the_runtimes_cache(self) -> None:
        table = resource_table(trace=False)
        core = firmware_elf.load_core("c66x_0")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "fw.out"
            # 288 KiB of L2: all SRAM on bare metal, the top 64 KiB is cache under FreeRTOS
            path.write_bytes(elf32(140, [(0x00800000, 0x48000)], table))
            self.assertEqual(firmware_elf.inspect(path, core, runtime="baremetal").problems, [])
            freertos = firmware_elf.inspect(path, core, runtime="freertos").problems
            self.assertIn("freertos runtime uses as cache", freertos[0])
            path.write_bytes(elf32(140, [(0x00800000, 0x38000)], table))
            self.assertEqual(firmware_elf.inspect(path, core, runtime="freertos").problems, [])

    def test_reserved_and_unaudited_regions_are_refused_with_their_reason(self) -> None:
        pool = self.check("c66x_0", elf32(140, [(0xA7000000, 0x100)], resource_table()))
        self.assertIn("overlaps dma_pool, which is reserved: Linux allocates", pool.problems[0])
        msmc = self.check("c71x_0", elf32(145, [(0x70100000, 0x100)], resource_table()))
        self.assertIn("overlaps msmc, which is not confirmed free (Task 012)", msmc.problems[0])
        l1d = self.check("c66x_0", elf32(140, [(0x00F00000, 0x100)], resource_table()))
        self.assertIn("uses as cache", l1d.problems[0])

    def test_misaligned_entry_point_is_refused(self) -> None:
        # elf32 uses the first segment's address as the entry point
        ok = self.check("c71x_0", elf32(145, [(0xA8200000, 0x100)], resource_table(trace_address=0xA8200000)))
        self.assertEqual(ok.problems, [])
        bad = self.check("c71x_0", elf32(145, [(0xA8300000, 0x100)], resource_table(trace_address=0xA8300000)))
        self.assertIn("is not 0x200000-aligned", bad.problems[0])

    def test_overlapping_load_segments_are_refused(self) -> None:
        report = self.check("c66x_0", elf32(140, [(0xA6100000, 0x200), (0xA6100100, 0x200)], resource_table()))
        self.assertIn("overlap", report.problems[-1])

    def test_segment_crossing_the_end_of_the_carveout_is_refused(self) -> None:
        report = self.check("c66x_0", elf32(140, [(0xA6FFF000, 0x2000)], resource_table()))
        self.assertFalse(report.ok)

    def test_wrong_machine_is_refused(self) -> None:
        report = self.check("c71x_0", elf32(140, [(0xA8100000, 0x100)], resource_table()))
        self.assertIn("machine 140 is not c7000", report.problems[0])

    def test_missing_resource_table_or_trace_is_reported(self) -> None:
        self.assertIn("no .resource_table", self.check("c66x_0", elf32(140, [(0xA6100000, 0x10)], None)).problems[0])
        no_trace = self.check("c66x_0", elf32(140, [(0xA6100000, 0x100)], resource_table(trace=False)), True)
        self.assertEqual(no_trace.problems, ["resource table has no trace buffer"])
        one_ring = self.check("c66x_0", elf32(140, [(0xA6100000, 0x100)], resource_table(rings=1)))
        self.assertIn("two vrings", one_ring.problems[0])

    def test_ti_64_bit_trace_entry_counts_as_a_trace(self) -> None:
        report = self.check("c71x_0", elf32(145, [(0xA8200000, 0x100)], resource_table(trace64=True)), True)
        self.assertEqual(report.problems, [])
        trace = report.resources[1]
        self.assertEqual((trace["type"], trace["address"], trace["length"], trace["format"]),
                         ("trace", 0xA8C10000, 0x10000, "ti-64"))

    def test_non_elf_and_truncated_files_raise(self) -> None:
        with self.assertRaises(firmware_elf.ElfError):
            self.check("c66x_0", b"MZ not an elf")
        with self.assertRaises(firmware_elf.ElfError):
            self.check("c66x_0", elf32(140, [(0xA6100000, 0x10)], resource_table())[:60])


if __name__ == "__main__":
    unittest.main()
