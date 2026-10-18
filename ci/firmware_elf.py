#!/usr/bin/env python3
"""Inspect a DSP firmware ELF against the core it is meant for.

remoteproc copies every loadable segment to its physical address, so a segment
outside the core's carve-out overwrites memory that belongs to something else.
This check refuses such a file before it reaches a board. It also decodes the
resource table that Linux reads to set up tracing and RPMsg.

Usage: ``python3 ci/firmware_elf.py <core> <file.elf>``; exit 0 when it fits.
Portable: standard library only, so it runs on the PC, in CI, and in tests.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
CORES = ROOT / "manifest" / "dsp-cores.json"

EM_TI_C6000 = 140
EM_TI_C7X = 145
MACHINE = {"c6000": EM_TI_C6000, "c7000": EM_TI_C7X}
PT_LOAD = 1

# include/linux/remoteproc.h
RSC_CARVEOUT, RSC_DEVMEM, RSC_TRACE, RSC_VDEV = 0, 1, 2, 3
RSC_TRACE64 = 0x10002  # TI's version-1 trace entry with a 64-bit address, used on C7x
VIRTIO_ID_RPMSG = 7


@dataclass
class Segment:
    address: int
    file_size: int
    memory_size: int
    flags: str


@dataclass
class Report:
    path: str
    core: str
    machine: int
    entry: int
    segments: list[Segment] = field(default_factory=list)
    resources: list[dict[str, Any]] = field(default_factory=list)
    problems: list[str] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.problems


class ElfError(ValueError):
    pass


def _unpack(fmt: str, data: bytes, offset: int) -> tuple[Any, ...]:
    end = offset + struct.calcsize(fmt)
    if offset < 0 or end > len(data):
        raise ElfError(f"truncated at offset {offset:#x}")
    return struct.unpack_from(fmt, data, offset)


def _sections(data: bytes, is64: bool, shoff: int, shentsize: int, shnum: int, shstrndx: int) -> dict[str, tuple[int, int, int]]:
    """name -> (address, file offset, size) for sections with file contents."""
    fmt = "<IIQQQQIIQQ" if is64 else "<IIIIIIIIII"
    headers = [_unpack(fmt, data, shoff + i * shentsize) for i in range(shnum)]
    if not headers or shstrndx >= len(headers):
        return {}
    names = headers[shstrndx]
    strtab_off = names[4]
    result = {}
    for header in headers:
        name_off, kind, _flags, address, offset, size = header[:6]
        end = data.find(b"\0", strtab_off + name_off)
        name = data[strtab_off + name_off : end].decode("ascii", errors="replace")
        if kind != 8:  # SHT_NOBITS has no bytes in the file
            result[name] = (address, offset, size)
    return result


def resource_table(blob: bytes) -> list[dict[str, Any]]:
    """Decode a remoteproc resource table (version 1)."""
    version, count = _unpack("<II", blob, 0)
    if version != 1:
        raise ElfError(f"resource table version {version}, expected 1")
    entries = []
    for i in range(count):
        (offset,) = _unpack("<I", blob, 16 + 4 * i)
        (kind,) = _unpack("<I", blob, offset)
        body = offset + 4
        if kind == RSC_TRACE:
            address, length, _reserved = _unpack("<III", blob, body)
            name = blob[body + 12 : body + 44].split(b"\0", 1)[0].decode("ascii", errors="replace")
            entries.append({"type": "trace", "address": address, "length": length, "name": name})
        elif kind == RSC_TRACE64:
            (address,) = _unpack("<Q", blob, offset + 8)  # 8-byte aligned after the type
            (length,) = _unpack("<I", blob, offset + 16)
            name = blob[offset + 24 : offset + 56].split(b"\0", 1)[0].decode("ascii", errors="replace")
            entries.append({"type": "trace", "address": address, "length": length, "name": name, "format": "ti-64"})
        elif kind == RSC_VDEV:
            vid, _nid, features, _gfeatures, config_len, status, rings = _unpack("<IIIIIBB", blob, body)
            vrings = []
            for r in range(rings):
                address, align, num, notify, _reserved = _unpack("<IIIII", blob, body + 24 + 20 * r)
                vrings.append({"address": address, "align": align, "num": num, "notify": notify})
            entries.append({"type": "vdev", "id": vid, "features": features, "config_len": config_len,
                            "status": status, "vrings": vrings})
        elif kind in (RSC_CARVEOUT, RSC_DEVMEM):
            device, physical, length, flags, _reserved = _unpack("<IIIII", blob, body)
            name = blob[body + 20 : body + 52].split(b"\0", 1)[0].decode("ascii", errors="replace")
            entries.append({"type": "carveout" if kind == RSC_CARVEOUT else "devmem", "device_address": device,
                            "physical": physical, "length": length, "flags": flags, "name": name})
        else:
            entries.append({"type": f"vendor-{kind:#x}"})
    return entries


def inspect(path: Path, core: dict[str, Any], require_trace: bool = False, runtime: str | None = None) -> Report:
    data = path.read_bytes()
    if data[:4] != b"\x7fELF":
        raise ElfError(f"{path.name} is not an ELF file")
    is64 = data[4] == 2
    if data[5] != 1:
        raise ElfError(f"{path.name} is not little-endian")
    if is64:
        _t, machine, _v, entry, phoff, shoff, _f, _eh, phentsize, phnum, shentsize, shnum, shstrndx = _unpack(
            "<HHIQQQIHHHHHH", data, 16)
    else:
        _t, machine, _v, entry, phoff, shoff, _f, _eh, phentsize, phnum, shentsize, shnum, shstrndx = _unpack(
            "<HHIIIIIHHHHHH", data, 16)
    report = Report(str(path), core["name"], machine, entry)
    if machine != MACHINE[core["family"]]:
        report.problems.append(f"machine {machine} is not {core['family']} ({MACHINE[core['family']]})")

    for i in range(phnum):
        if is64:
            kind, flags, _off, vaddr, paddr, filesz, memsz, _align = _unpack("<IIQQQQQQ", data, phoff + i * phentsize)
        else:
            kind, _off, vaddr, paddr, filesz, memsz, flags, _align = _unpack("<IIIIIIII", data, phoff + i * phentsize)
        if kind != PT_LOAD or memsz == 0:
            continue
        perms = ("R" if flags & 4 else "-") + ("W" if flags & 2 else "-") + ("X" if flags & 1 else "-")
        report.segments.append(Segment(paddr, filesz, memsz, perms))
    report.problems += placement_problems(report.segments, core, runtime)
    alignment = _hex(core.get("boot_alignment", "0x1"))
    if entry % alignment:
        report.problems.append(f"entry point {entry:#x} is not {alignment:#x}-aligned, so remoteproc cannot boot "
                               f"{core['name']} from it")
    sections = _sections(data, is64, shoff, shentsize, shnum, shstrndx) if shoff else {}
    if ".resource_table" not in sections:
        report.problems.append("no .resource_table section; remoteproc cannot set up RPMsg or trace")
        return report
    _address, offset, size = sections[".resource_table"]
    try:
        report.resources = resource_table(data[offset : offset + size])
    except ElfError as exc:
        report.problems.append(f"resource table: {exc}")
        return report
    vdevs = [r for r in report.resources if r["type"] == "vdev" and r["id"] == VIRTIO_ID_RPMSG]
    if not vdevs or len(vdevs[0]["vrings"]) != 2:
        report.problems.append("resource table has no RPMsg vdev with two vrings")
    traces = [r for r in report.resources if r["type"] == "trace"]
    if require_trace and not traces:
        report.problems.append("resource table has no trace buffer")
    ranges = load_ranges(core, runtime)
    for trace in traces:
        if not any(lo <= trace["address"] and trace["address"] + trace["length"] <= hi for _n, lo, hi in ranges):
            report.problems.append(f"trace buffer {trace['address']:#x}+{trace['length']:#x} is not in memory Linux "
                                   f"can read through remoteproc")
    return report


def _hex(value: Any) -> int:
    return int(str(value), 16)


def load_ranges(core: dict[str, Any], runtime: str | None = None) -> list[tuple[str, int, int]]:
    """(name, start, end) where remoteproc may load segments under this runtime.

    Without a runtime, the largest cache reserve of any runtime applies."""
    ranges = []
    for region in core["regions"]:
        if not region["loadable"] or region["runtime_use"] != "any":
            continue
        reserves = region.get("cache_reserve", {})
        reserve = _hex(reserves[runtime]) if runtime in reserves else max([_hex(v) for v in reserves.values()] or [0])
        start, end = _hex(region["base"]), _hex(region["base"]) + _hex(region["size"]) - reserve
        if end > start:
            ranges.append((region["name"], start, end))
    return ranges


def _why_not(core: dict[str, Any], start: int, end: int, runtime: str | None) -> str:
    for region in core["regions"]:
        base, top = _hex(region["base"]), _hex(region["base"]) + _hex(region["size"])
        if start < top and base < end:
            if not region["loadable"] or region["runtime_use"] != "any":
                owner = {"none": "is reserved", "audit": "is not confirmed free (Task 012)"}.get(region["runtime_use"],
                                                                                              "cannot be loaded")
                return f"overlaps {region['name']}, which {owner}: {region['reason'].split('. ')[0]}"
            return (f"reaches into the part of {region['name']} that the {runtime or 'any'} runtime uses as cache "
                    f"or that lies past its end")
    return f"is outside every memory region of {core['name']}"


def placement_problems(segments: list[Segment], core: dict[str, Any], runtime: str | None) -> list[str]:
    """Every loadable segment inside one load range, and no two overlapping.

    Linker overlays share run addresses, never load addresses, so overlapping
    PT_LOAD segments are always an error."""
    problems = []
    ranges = load_ranges(core, runtime)
    for seg in segments:
        end = seg.address + seg.memory_size
        if not any(lo <= seg.address and end <= hi for _n, lo, hi in ranges):
            problems.append(f"segment {seg.address:#x}..{end:#x} {_why_not(core, seg.address, end, runtime)}")
    ordered = sorted(segments, key=lambda seg: seg.address)
    for first, second in zip(ordered, ordered[1:]):
        if first.address + first.memory_size > second.address:
            problems.append(f"segments {first.address:#x}+{first.memory_size:#x} and {second.address:#x} overlap")
    return problems


def load_core(name: str, manifest: Path = CORES) -> dict[str, Any]:
    cores = {c["name"]: c for c in json.loads(manifest.read_text(encoding="utf-8"))["cores"]}
    if name not in cores:
        raise ValueError(f"unknown core {name}; known: {', '.join(cores)}")
    return cores[name]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("core")
    parser.add_argument("elf", type=Path)
    parser.add_argument("--require-trace", action="store_true", help="course firmware must expose a trace buffer")
    parser.add_argument("--runtime", choices=("freertos", "baremetal"),
                        help="the firmware's runtime, which decides how much local SRAM is cache")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        report = inspect(args.elf, load_core(args.core), args.require_trace, args.runtime)
    except (ElfError, ValueError, OSError) as exc:
        print(f"firmware check: {exc}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps({**asdict(report), "ok": report.ok}, indent=2))
    else:
        for segment in report.segments:
            print(f"  load {segment.address:#010x} +{segment.memory_size:#x} {segment.flags}")
        for resource in report.resources:
            print(f"  resource {resource}")
        print(f"{args.elf.name}: {'fits ' + args.core if report.ok else '; '.join(report.problems)}")
    return 0 if report.ok else 1


if __name__ == "__main__":
    sys.exit(main())
