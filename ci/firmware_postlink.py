#!/usr/bin/env python3
"""Post-link step for course firmware: protection rules and the integrity record.

Every course firmware build runs this on the linked, unstripped ELF before
stripping it (cmake/SdosTiFirmware.cmake). It refuses a layout that would
weaken the run-time protection (docs/safety.md), then writes the CRC record of
the read-only image into the ``.sdos.integrity`` section so the firmware can
check its own code and constants at start and in the background.

Rules:

- The linker files define the protection zones ``sdos_zone_code_start/end``
  and ``sdos_zone_rodata_start/end``; each is 4 KiB-aligned and inside memory
  the core may load.
- The L2 code zone ``sdos_zone_l2code_start/end`` is aligned to the L2
  protection page (16 KiB on the C66x) and inside usable L2, which ends at
  ``sdos_zone_l2_end``.
- Executable code is loaded and runs only in the code zones.
- Nothing writable lies in a code or read-only data zone.
- No loaded segment is both writable and executable.
- No C heap: ``malloc`` and its relatives are not linked.
- C66x: the device windows from manifest/dsp-cores.json and the zones fit the
  14 MPAX segments the firmware programs.
- C71x: guarded stacks start on a 4 KiB page, whose first page becomes their
  read-only guard, and the MMU map fits TI's 16 page tables. TI's Mmu_map
  dereferences a null table pointer when the tables run out, so this limit is
  checked here, at build time.

Usage: ``python3 ci/firmware_postlink.py <core> <runtime> <debug.elf>``.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from ci import firmware_elf as fe
except ImportError:  # run as a script from ci/
    import firmware_elf as fe  # type: ignore[no-redef]

PAGE = 0x1000
SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR = 1, 2, 4
HEAP_SYMBOLS = ("malloc", "calloc", "realloc", "free", "memalign", "aligned_alloc")
# MPAX segments 0 and 1 deny everything below and above 2 GiB; the firmware
# programs its grants into segments 2 to 15 (firmware/c66x/src/c66x.c).
MPAX_SEGMENTS = 14
# The C71x MMU: 4 KiB pages, 2 MiB blocks, 1 GiB tables; TI's pool holds 16.
BLOCK, GIGABYTE, MMU_TABLES = 0x200000, 0x40000000, 16


def mpax_split(start: int, end: int) -> int:
    """Segments sdos_mpax_split() needs for [start, end): the C planner's twin."""
    count = 0
    while start < end:
        log2 = 12
        while log2 < 32 and start % (1 << (log2 + 1)) == 0 and start + (1 << (log2 + 1)) <= end:
            log2 += 1
        start += 1 << log2
        count += 1
    return count


def _region(core: dict, name: str) -> tuple[int, int]:
    r = next(r for r in core["regions"] if r["name"] == name)
    return int(r["base"], 16), int(r["base"], 16) + int(r["size"], 16)


L2_PAGE = {"c6000": 0x4000, "c7000": PAGE}  # C66x L2 protection page; C71x MMU page


def zones(elf: fe.ElfFile, family: str = "c7000") -> tuple[dict[str, tuple[int, int]], list[str]]:
    found, problems = {}, []
    for zone in ("code", "rodata", "l2code"):
        start = elf.symbols.get(f"sdos_zone_{zone}_start")
        end = elf.symbols.get(f"sdos_zone_{zone}_end")
        align = L2_PAGE[family] if zone == "l2code" else PAGE
        if start is None or end is None:
            problems.append(f"the linker files do not define sdos_zone_{zone}_start and _end (docs/safety.md)")
            continue
        if start % align or end % align or end < start:
            problems.append(f"the {zone} zone {start:#x}..{end:#x} is not a {align // 1024} KiB-aligned range")
            continue
        found[zone] = (start, end)
    if "sdos_zone_l2_end" not in elf.symbols:
        problems.append("the linker files do not define sdos_zone_l2_end, the end of usable L2 (docs/safety.md)")
    return found, problems


def windows(core: dict) -> list[tuple[int, int]]:
    return [(int(w["base"], 16), int(w["base"], 16) + int(w["size"], 16)) for w in core.get("mmio_windows", [])]


def mpax_segments(core: dict, found: dict[str, tuple[int, int]]) -> int:
    """Segments 2 to 15 the C66x firmware programs (c66x.c): device windows,
    the carve-out, the vring pool, and the DDR parts of the zones."""
    ddr, pool = _region(core, "ddr"), _region(core, "dma_pool")
    needed = sum(mpax_split(lo, hi) for lo, hi in windows(core)) + mpax_split(*ddr) + mpax_split(*pool)
    for zone in ("code", "rodata"):
        lo, hi = found.get(zone, (0, 0))
        lo, hi = max(lo, ddr[0]), min(hi, ddr[1])
        if hi > lo:
            needed += mpax_split(lo, hi)
    return needed


def check(elf: fe.ElfFile, core: dict, runtime: str) -> list[str]:
    found, problems = zones(elf, core["family"])
    loadable = fe.load_ranges(core, runtime)
    for zone, (start, end) in found.items():
        if end > start and not any(lo <= start and end <= hi for _n, lo, hi in loadable):
            problems.append(f"the {zone} zone {start:#x}..{end:#x} is not inside memory {core['name']} may load")
    l2 = _region(core, "l2sram")
    l2_end = elf.symbols.get("sdos_zone_l2_end", l2[1])
    l2code = found.get("l2code", (0, 0))
    if l2code[1] > l2code[0] and not (l2[0] <= l2code[0] and l2code[1] <= l2_end):
        problems.append(f"the l2code zone {l2code[0]:#x}..{l2code[1]:#x} is not inside usable L2")

    def inside(start: int, end: int, *spans: tuple[int, int]) -> bool:
        return any(lo <= start and end <= hi for lo, hi in spans)

    code = found.get("code", (0, 0))
    for s in elf.sections:
        if not s.flags & SHF_ALLOC or not s.size:
            continue
        end = s.address + s.size
        if s.flags & SHF_EXECINSTR and found and not inside(s.address, end, code, l2code):
            problems.append(f"code section {s.name} at {s.address:#x} is outside the code zones")
        if s.flags & SHF_WRITE:
            for zone, (lo, hi) in found.items():
                if s.address < hi and lo < end:
                    problems.append(f"writable section {s.name} at {s.address:#x} lies in the {zone} zone, "
                                    f"which the firmware maps read-only")
    for seg in elf.segments:
        if seg.kind != fe.PT_LOAD or not seg.memory_size:
            continue
        if seg.flags & 3 == 3:
            problems.append(f"segment {seg.paddr:#x} is both writable and executable")
        if seg.flags & 1 and found:
            for address in {seg.paddr, seg.vaddr}:
                if not inside(address, address + seg.memory_size, code, l2code):
                    problems.append(f"code is loaded or runs at {address:#x}, outside the code zones")
    heap = [name for name in HEAP_SYMBOLS if name in elf.symbols]
    if heap:
        problems.append(f"the C heap is linked ({', '.join(heap)}); course firmware allocates statically")
    if core["family"] == "c6000" and found:
        needed = mpax_segments(core, found)
        if needed > MPAX_SEGMENTS:
            problems.append(f"the device windows and zones need {needed} MPAX segments, but the firmware has "
                            f"{MPAX_SEGMENTS}: align zone boundaries to larger powers of two")
    if core["family"] == "c7000":
        guarded = [(s.name, s.address) for s in elf.sections if s.name.startswith(".sdos.stack.") and s.size]
        top, size = elf.symbols.get("__TI_STACK_END"), elf.symbols.get("__TI_STACK_SIZE")
        if top is not None and size is not None:
            guarded.append(("the C stack", top - size))
        for name, address in guarded:
            if address % PAGE:
                problems.append(f"{name} at {address:#x} is not on a 4 KiB page; the MMU cannot guard it")
        tables = mmu_tables(elf, core, found, [a for _n, a in guarded])
        if tables > MMU_TABLES:
            problems.append(f"the MMU map needs about {tables} page tables, but TI's pool has {MMU_TABLES}: put "
                            f"zone and region boundaries on 2 MiB boundaries")
    return problems


def mmu_tables(elf: fe.ElfFile, core: dict, found: dict[str, tuple[int, int]], guards: list[int]) -> int:
    """Upper bound of the tables firmware/c71x/src/c71x.c needs, per security state.

    One level-1 table, one level-2 table per GiB touched, and one level-3 table
    per 2 MiB block that a mapping boundary splits."""
    spans = windows(core) + [_region(core, "l2sram"), _region(core, "ddr"), _region(core, "dma_pool")]
    ddr, l2 = _region(core, "ddr"), _region(core, "l2sram")
    for zone, (lo, hi) in found.items():
        bound = l2 if zone == "l2code" else ddr
        lo, hi = max(lo, bound[0]), min(hi, bound[1])
        if hi > lo:
            spans.append((lo, hi))
    start, end = elf.symbols.get("sdos_mmu_tables_start"), elf.symbols.get("sdos_mmu_tables_end")
    if start is not None and end is not None:
        spans.append((start, -(-end // PAGE) * PAGE))
    spans += [(g, g + PAGE) for g in guards]
    gigabytes = {g for lo, hi in spans for g in range(lo // GIGABYTE, (hi - 1) // GIGABYTE + 1)}
    split = {b // BLOCK for lo, hi in spans for b in (lo, hi) if b % BLOCK}
    return 1 + len(gigabytes) + len(split)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("core")
    parser.add_argument("runtime", choices=("freertos", "baremetal"))
    parser.add_argument("elf", type=Path)
    args = parser.parse_args(argv)
    try:
        core = fe.load_core(args.core)
        data = bytearray(args.elf.read_bytes())
        elf = fe.read_elf(bytes(data))
        problems = check(elf, core, args.runtime)
        section = elf.section(fe.INTEGRITY_SECTION)
        if section is None or section.kind == 8 or section.size < fe.INTEGRITY_SIZE:
            problems.append(f"no {fe.INTEGRITY_SECTION} section of {fe.INTEGRITY_SIZE} bytes to hold the record")
        if problems:
            for problem in problems:
                print(f"post-link: {problem}", file=sys.stderr)
            return 1
        assert section is not None
        if int.from_bytes(data[section.offset : section.offset + 4], "little") != fe.INTEGRITY_MAGIC:
            raise fe.ElfError(f"{fe.INTEGRITY_SECTION} does not start with the record marker")
        ranges = fe.integrity_ranges(elf)
        data[section.offset : section.offset + fe.INTEGRITY_SIZE] = fe.integrity_record(ranges)
        args.elf.write_bytes(bytes(data))
    except (fe.ElfError, ValueError, OSError, StopIteration) as exc:
        print(f"post-link: {exc}", file=sys.stderr)
        return 1
    total = sum(n for _a, n, _c in ranges)
    print(f"  integrity record: {len(ranges)} read-only ranges, {total} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
