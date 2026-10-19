#!/usr/bin/env python3
"""Device-tree memory audit (Task 012).

Two steps keep the board out of CI:

``extract``  reads a flattened device tree (the board's ``live.dtb``, captured
             by ``board-report``) and writes the memory facts as JSON: DDR
             banks, ``/memreserve/`` entries, every reserved-memory node with
             the nodes that use it, every remote core with its local memories
             and memory regions, and every on-chip SRAM with its carve-outs.
``check``    compares such an extraction with ``manifest/memory-map.json``
             (owner and source of every reserved region) and
             ``manifest/dsp-cores.json`` (each DSP's regions), and checks the
             map for overlap, range, and alignment. With no ``--dt`` it uses the
             extraction the memory map names as its evidence, so CI runs it
             without a board.

Usage::

    python3 ci/dt_memory.py extract live.dtb --out dt-memory.json
    python3 ci/dt_memory.py [check [--dt dt-memory.json]]

Portable: standard library only.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
MEMORY_MAP = ROOT / "manifest" / "memory-map.json"
CORES = ROOT / "manifest" / "dsp-cores.json"

FDT_MAGIC = 0xD00DFEED
FDT_BEGIN_NODE, FDT_END_NODE, FDT_PROP, FDT_NOP, FDT_END = 1, 2, 3, 4, 9
PAGE = 0x1000
REMOTE_CORES = ("ti,j721e-c66-dsp", "ti,j721e-c71-dsp", "ti,j721e-r5f")


class Node:
    def __init__(self, name: str, parent: "Node | None") -> None:
        self.name = name
        self.parent = parent
        self.props: dict[str, bytes] = {}
        self.children: list[Node] = []

    @property
    def path(self) -> str:
        if self.parent is None:
            return "/"
        base = self.parent.path
        return (base if base.endswith("/") else base + "/") + self.name

    def cells(self, name: str) -> list[int]:
        raw = self.props.get(name, b"")
        return list(struct.unpack(f">{len(raw) // 4}I", raw))

    def strings(self, name: str) -> list[str]:
        raw = self.props.get(name)
        return [] if raw is None else [s.decode() for s in raw.rstrip(b"\0").split(b"\0")]

    def cell_count(self, name: str, default: int) -> int:
        raw = self.props.get(name)
        return struct.unpack(">I", raw)[0] if raw else default

    def walk(self):
        yield self
        for child in self.children:
            yield from child.walk()


def parse(blob: bytes) -> tuple[Node, list[tuple[int, int]]]:
    """Parse a flattened device tree; return the root and /memreserve/ entries."""
    if len(blob) < 40:
        raise ValueError("not a device tree: too short")
    magic, total, off_struct, off_strings, off_rsv = struct.unpack_from(">5I", blob, 0)
    if magic != FDT_MAGIC or total > len(blob):
        raise ValueError("not a device tree: bad magic or size")
    reserved = []
    at = off_rsv
    while True:
        address, size = struct.unpack_from(">QQ", blob, at)
        at += 16
        if address == 0 and size == 0:
            break
        reserved.append((address, size))

    def string(offset: int) -> str:
        end = blob.index(b"\0", off_strings + offset)
        return blob[off_strings + offset:end].decode()

    root: Node | None = None
    node: Node | None = None
    at = off_struct
    while True:
        (token,) = struct.unpack_from(">I", blob, at)
        at += 4
        if token == FDT_BEGIN_NODE:
            end = blob.index(b"\0", at)
            child = Node(blob[at:end].decode(), node)
            at = (end + 4) & ~3
            if node is None:
                root = child
            else:
                node.children.append(child)
            node = child
        elif token == FDT_END_NODE:
            if node is None:
                raise ValueError("unbalanced device tree")
            node = node.parent
        elif token == FDT_PROP:
            length, name_offset = struct.unpack_from(">II", blob, at)
            at += 8
            if node is None:
                raise ValueError("property outside a node")
            node.props[string(name_offset)] = blob[at:at + length]
            at = (at + length + 3) & ~3
        elif token == FDT_NOP:
            continue
        elif token == FDT_END:
            break
        else:
            raise ValueError(f"bad device-tree token {token:#x}")
    if root is None:
        raise ValueError("empty device tree")
    return root, reserved


def _number(cells: list[int]) -> int:
    value = 0
    for cell in cells:
        value = (value << 32) | cell
    return value


def reg(node: Node) -> list[tuple[int, int]]:
    """``reg`` as (address, size) pairs, using the parent's cell counts."""
    parent = node.parent
    address_cells = parent.cell_count("#address-cells", 2) if parent else 2
    size_cells = parent.cell_count("#size-cells", 1) if parent else 1
    cells = node.cells("reg")
    step = address_cells + size_cells
    return [(_number(cells[i:i + address_cells]), _number(cells[i + address_cells:i + step]))
            for i in range(0, len(cells) - step + 1, step)]


def _hex(value: int) -> str:
    return f"{value:#x}"


def extract(blob: bytes) -> dict[str, Any]:
    """The memory facts of a device tree, as plain JSON data."""
    root, memreserve = parse(blob)
    by_phandle = {n.cell_count("phandle", 0): n for n in root.walk() if "phandle" in n.props}
    model = root.strings("model")

    banks = []
    for node in root.children:
        if node.strings("device_type") == ["memory"]:
            banks += [{"base": _hex(a), "size": _hex(s)} for a, s in reg(node)]

    users: dict[str, list[str]] = {}
    for node in root.walk():
        if "memory-region" in node.props:
            for handle in node.cells("memory-region"):
                target = by_phandle.get(handle)
                if target is not None:
                    users.setdefault(target.path, []).append(node.path)

    reserved = []
    container = next((n for n in root.children if n.name == "reserved-memory"), None)
    for node in container.children if container else []:
        entry: dict[str, Any] = {"node": node.name, "reg": [{"base": _hex(a), "size": _hex(s)} for a, s in reg(node)]}
        if "compatible" in node.props:
            entry["compatible"] = node.strings("compatible")
        entry["no_map"] = "no-map" in node.props
        entry["used_by"] = users.get(node.path, [])
        reserved.append(entry)

    cores = []
    srams = []
    for node in root.walk():
        compatible = node.strings("compatible")
        if any(c in REMOTE_CORES for c in compatible):
            names = node.strings("reg-names")
            regions = [by_phandle[h].name for h in node.cells("memory-region") if h in by_phandle]
            cores.append({
                "device": node.name.split("@", 1)[1].lower() + "." + node.name.split("@", 1)[0]
                if "@" in node.name else node.name,
                "path": node.path,
                "compatible": compatible,
                "firmware": (node.strings("firmware-name") or [None])[0],
                "status": (node.strings("status") or ["okay"])[0],
                "local": {names[i] if i < len(names) else str(i): {"base": _hex(a), "size": _hex(s)}
                          for i, (a, s) in enumerate(reg(node))},
                "memory_region": regions,
            })
        if "mmio-sram" in compatible:
            ((base, size),) = reg(node)
            srams.append({
                "node": node.path, "base": _hex(base), "size": _hex(size),
                "carveouts": [{"node": c.name, "base": _hex(base + a), "size": _hex(s)}
                              for c in node.children for a, s in reg(c)],
            })
    return {
        "schema": "sdos-dt-memory/1",
        "model": model[0] if model else None,
        "memory": banks,
        "memreserve": [{"base": _hex(a), "size": _hex(s)} for a, s in memreserve],
        "reserved_memory": reserved,
        "remote_cores": sorted(cores, key=lambda c: c["path"]),
        "srams": srams,
    }


def _int(text: str) -> int:
    return int(text, 0)


def _span(entry: dict[str, Any]) -> tuple[int, int]:
    return _int(entry["base"]), _int(entry["base"]) + _int(entry["size"])


def check_map(memory_map: dict[str, Any]) -> list[str]:
    """Checks on the memory map alone: owners, sources, alignment, range, overlap."""
    problems = []
    banks = [_span(b) for b in memory_map.get("memory", [])]
    spans = []
    for region in memory_map.get("reserved_memory", []):
        name = region.get("node", "?")
        for key in ("owner", "source", "reason"):
            if not region.get(key):
                problems.append(f"{name}: no {key}")
        start, end = _span(region)
        if start % PAGE or (end - start) % PAGE or end <= start:
            problems.append(f"{name}: {region['base']}+{region['size']} is not a whole number of 4 KiB pages")
        if not any(lo <= start and end <= hi for lo, hi in banks):
            problems.append(f"{name}: {region['base']}+{region['size']} is outside every DDR bank")
        spans.append((start, end, name))
    for entry in memory_map.get("memreserve", []):
        start, end = _span(entry)
        spans.append((start, end, f"/memreserve/ {entry['base']}"))
    spans.sort()
    for (a_start, a_end, a_name), (b_start, b_end, b_name) in zip(spans, spans[1:]):
        if b_start < a_end:
            problems.append(f"{a_name} and {b_name} overlap at {b_start:#x}")
    for sram in memory_map.get("srams", []):
        start, end = _span(sram)
        for usable in sram.get("usable", []):
            u_start, u_end = _span(usable)
            if not (start <= u_start and u_end <= end):
                problems.append(f"{sram['node']}: usable {usable['base']}+{usable['size']} is outside the SRAM")
            for carve in sram.get("carveouts", []):
                c_start, c_end = _span(carve)
                if u_start < c_end and c_start < u_end:
                    problems.append(f"{sram['node']}: usable {usable['base']} overlaps carve-out {carve['node']}")
            if not usable.get("owner") or not usable.get("source"):
                problems.append(f"{sram['node']}: usable {usable['base']} has no owner or source")
    return problems


def check_against_dt(memory_map: dict[str, Any], dt: dict[str, Any]) -> list[str]:
    """The memory map must state exactly what the device tree reserves."""
    problems = []
    if [_span(b) for b in memory_map.get("memory", [])] != [_span(b) for b in dt["memory"]]:
        problems.append("DDR banks differ from the device tree's /memory node")
    if sorted(map(_span, memory_map.get("memreserve", []))) != sorted(map(_span, dt["memreserve"])):
        problems.append("/memreserve/ entries differ from the device tree")
    listed = {r["node"]: r for r in memory_map.get("reserved_memory", [])}
    live = {r["node"]: r for r in dt["reserved_memory"]}
    for name in sorted(set(live) - set(listed)):
        problems.append(f"{name}: reserved in the device tree but not in the memory map (no owner)")
    for name in sorted(set(listed) - set(live)):
        problems.append(f"{name}: in the memory map but not reserved in the device tree")
    for name in sorted(set(listed) & set(live)):
        mine, theirs = listed[name], live[name]
        if [_span(mine)] != [_span(r) for r in theirs["reg"]]:
            problems.append(f"{name}: memory map says {mine['base']}+{mine['size']}, device tree differs")
        if mine.get("no_map") != theirs["no_map"]:
            problems.append(f"{name}: no-map differs from the device tree")
        if sorted(mine.get("used_by", [])) != sorted(theirs["used_by"]):
            problems.append(f"{name}: users differ from the device tree: {theirs['used_by']}")
    live_srams = {s["node"]: s for s in dt["srams"]}
    for sram in memory_map.get("srams", []):
        theirs = live_srams.get(sram["node"])
        if theirs is None:
            problems.append(f"{sram['node']}: not in the device tree")
            continue
        if _span(sram) != _span(theirs):
            problems.append(f"{sram['node']}: range differs from the device tree")
        if sorted(map(_span, sram.get("carveouts", []))) != sorted(map(_span, theirs["carveouts"])):
            problems.append(f"{sram['node']}: carve-outs differ from the device tree")
    return problems


def check_cores(cores: dict[str, Any], memory_map: dict[str, Any], dt: dict[str, Any]) -> list[str]:
    """Each DSP in the core contract against its device-tree node and the map."""
    problems = []
    live = {c["device"]: c for c in dt["remote_cores"]}
    reserved = {r["node"]: r for r in memory_map.get("reserved_memory", [])}
    srams = memory_map.get("srams", [])
    family = {"c6000": "ti,j721e-c66-dsp", "c7000": "ti,j721e-c71-dsp"}
    for core in cores["cores"]:
        name = core["name"]
        node = live.get(core["device"])
        if node is None:
            problems.append(f"{name}: no device-tree node for {core['device']}")
            continue
        if family[core["family"]] not in node["compatible"]:
            problems.append(f"{name}: device tree says {node['compatible']}")
        if node["firmware"] != core["kernel_firmware"]:
            problems.append(f"{name}: device tree firmware-name {node['firmware']}, contract {core['kernel_firmware']}")
        if node["status"] != "okay":
            problems.append(f"{name}: device-tree status {node['status']}")
        regions = {r["name"]: r for r in core["regions"]}
        # remoteproc's order: the first memory-region is the DMA pool for
        # vrings and buffers, the second the firmware carve-out.
        expected = [regions.get("dma_pool"), regions.get("ddr")]
        if len(node["memory_region"]) != 2:
            problems.append(f"{name}: device tree lists {len(node['memory_region'])} memory regions, expected 2")
        for region, ref in zip(expected, node["memory_region"]):
            target = reserved.get(ref)
            if region is None or target is None:
                problems.append(f"{name}: memory region {ref} is not in the memory map")
            elif _span(region) != _span(target):
                problems.append(f"{name}: {region['name']} {region['base']}+{region['size']} is not {ref}")
            elif name not in target.get("owner", "") and target.get("owner") != f"{name}":
                problems.append(f"{name}: {ref} is owned by {target.get('owner')} in the memory map")
        dt_local = {"l2sram": "l2sram", "l1dsram": "l1dram", "l1psram": "l1pram"}
        for region_name, dt_name in dt_local.items():
            region = regions.get(region_name)
            if region is None:
                continue
            local = node["local"].get(dt_name)
            if local is None:
                problems.append(f"{name}: device tree has no {dt_name}")
                continue
            global_base = _int(region.get("global_base", region["base"]))
            if dt_name == "l2sram" and global_base != _int(local["base"]):
                problems.append(f"{name}: l2sram at {local['base']} in the device tree, contract {region.get('global_base')}")
            if _int(region["size"]) > _int(local["size"]):
                problems.append(f"{name}: {region_name} {region['size']} exceeds the device tree's {local['size']}")
        shared = regions.get("shared_ddr")
        if shared is not None and not any(_span(r) == _span(shared) for r in reserved.values()):
            problems.append(f"{name}: shared_ddr {shared['base']} is not a reserved region")
        msmc = regions.get("msmc")
        if msmc is not None:
            m_start, m_end = _span(msmc)
            inside = [s for s in srams if _span(s)[0] <= m_start and m_end <= _span(s)[1]]
            if not inside:
                problems.append(f"{name}: msmc {msmc['base']} is outside every on-chip SRAM")
            for sram in inside:
                for carve in sram.get("carveouts", []):
                    c_start, c_end = _span(carve)
                    if m_start < c_end and c_start < m_end:
                        problems.append(f"{name}: msmc overlaps the SRAM carve-out {carve['node']}")
    return problems


def check(memory_map: dict[str, Any], cores: dict[str, Any], dt: dict[str, Any]) -> list[str]:
    return check_map(memory_map) + check_against_dt(memory_map, dt) + check_cores(cores, memory_map, dt)


def _load(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text())


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command")
    ex = commands.add_parser("extract", help="write the memory facts of a .dtb as JSON")
    ex.add_argument("dtb", type=Path)
    ex.add_argument("--out", type=Path)
    ck = commands.add_parser("check", help="check the memory map and core contract against a device tree")
    ck.add_argument("--dt", type=Path, help="an extraction; default: the memory map's evidence")
    ck.add_argument("--map", type=Path, default=MEMORY_MAP)
    ck.add_argument("--cores", type=Path, default=CORES)
    args = parser.parse_args(argv)
    if args.command is None:  # the quality gate runs every validator without arguments
        args = parser.parse_args(["check"])

    if args.command == "extract":
        text = json.dumps(extract(args.dtb.read_bytes()), indent=2) + "\n"
        if args.out:
            args.out.write_text(text)
        else:
            sys.stdout.write(text)
        return 0
    memory_map = _load(args.map)
    dt_path = args.dt or ROOT / memory_map["evidence"]
    problems = check(memory_map, _load(args.cores), _load(dt_path))
    for problem in problems:
        print(f"FAIL  {problem}")
    if not problems:
        count = len(memory_map.get("reserved_memory", []))
        print(f"PASS  {count} reserved regions, {len(memory_map.get('srams', []))} on-chip SRAMs, "
              f"and every DSP agree with {dt_path.relative_to(ROOT) if dt_path.is_relative_to(ROOT) else dt_path}")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
