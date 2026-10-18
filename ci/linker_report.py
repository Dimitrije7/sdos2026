#!/usr/bin/env python3
"""Summarize a TI linker map: memory regions, utilization, and every output section.

Usage: ``python3 ci/linker_report.py <file.map> <out-stem>`` writes
``<out-stem>.layout.json`` and ``<out-stem>.layout.txt``. Every firmware build
runs it after linking (cmake/SdosTiFirmware.cmake). Works for the C6000 and
C7000 linkers, whose maps differ only in address width.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from typing import Any

HEX = r"[0-9a-fA-F]+"
REGION = re.compile(rf"^\s{{2}}(\S+)\s+({HEX})\s+({HEX})\s+({HEX})\s+({HEX})\s+(\S+)")
SECTION_ONE_LINE = re.compile(rf"^(\S+)\s+\d+\s+({HEX})\s+({HEX})\s*(.*)$")
SECTION_SECOND_LINE = re.compile(rf"^\*\s+\d+\s+({HEX})\s+({HEX})\s*(.*)$")
RUN = re.compile(rf"RUN ADDR = ({HEX})")


def parse(text: str) -> dict[str, Any]:
    lines = text.splitlines()
    regions = []
    in_memory = False
    for line in lines:
        if line.startswith("MEMORY CONFIGURATION"):
            in_memory = True
            continue
        if in_memory and line.startswith(("SEGMENT ALLOCATION MAP", "SECTION ALLOCATION MAP")):
            break
        match = REGION.match(line) if in_memory else None
        if match:
            name, origin, length, used, unused, attr = match.groups()
            regions.append({"name": name, "origin": int(origin, 16), "length": int(length, 16),
                            "used": int(used, 16), "unused": int(unused, 16), "attributes": attr})

    sections = []
    start = next((i for i, line in enumerate(lines) if line.startswith("SECTION ALLOCATION MAP")), len(lines))
    pending = None
    for line in lines[start + 1:]:
        if line.startswith(("LINKER GENERATED COPY TABLES", "GLOBAL SYMBOLS", "LINKER GENERATED HANDLER")):
            break
        one = SECTION_ONE_LINE.match(line)
        second = SECTION_SECOND_LINE.match(line)
        if pending and second:
            origin, length, attrs = second.groups()
            sections.append(_section(pending, origin, length, attrs))
            pending = None
        elif one and not line.startswith(" "):
            name, origin, length, attrs = one.groups()
            sections.append(_section(name, origin, length, attrs))
            pending = None
        elif line and not line.startswith((" ", "*", "-", "output", "section")):
            pending = line.strip()
    for section in sections:
        section["region"] = next((r["name"] for r in regions
                                  if r["origin"] <= section["run"] < r["origin"] + r["length"]), None)
        if section["load"] != section["run"]:
            section["load_region"] = next((r["name"] for r in regions
                                           if r["origin"] <= section["load"] < r["origin"] + r["length"]), None)
    entry = re.search(r'ENTRY POINT SYMBOL: "(\S+)"\s+address: (' + HEX + ")", text)
    return {
        "schema": "sdos-linker-report/1",
        "entry": {"symbol": entry.group(1), "address": int(entry.group(2), 16)} if entry else None,
        "regions": [{**r, "percent": round(100.0 * r["used"] / r["length"], 1) if r["length"] else 0.0}
                    for r in regions],
        "sections": [s for s in sections if s["length"] > 0 or s["attributes"]],
    }


def _section(name: str, origin: str, length: str, attrs: str) -> dict[str, Any]:
    load = int(origin, 16)
    run = RUN.search(attrs)
    return {"name": name, "load": load, "run": int(run.group(1), 16) if run else load, "length": int(length, 16),
            "attributes": RUN.sub("", attrs).strip()}


def text_report(report: dict[str, Any]) -> str:
    out = []
    if report["entry"]:
        out.append(f"entry {report['entry']['symbol']} at {report['entry']['address']:#x}")
    out.append(f"{'region':12} {'origin':>12} {'end':>12} {'length':>10} {'used':>10} {'use':>6}")
    for r in report["regions"]:
        end = r["origin"] + r["length"] - 1
        out.append(f"{r['name']:12} {r['origin']:#12x} {end:#12x} {r['length']:#10x} {r['used']:#10x} {r['percent']:5.1f}%")
    out.append("")
    out.append(f"{'section':32} {'run':>12} {'load':>12} {'length':>10}  region  attributes")
    for s in sorted(report["sections"], key=lambda s: s["run"]):
        load = f"{s['load']:#12x}" if s["load"] != s["run"] else " " * 12
        out.append(f"{s['name'][:32]:32} {s['run']:#12x} {load} {s['length']:#10x}  {s['region'] or '-'}  "
                   f"{s['attributes']}")
    return "\n".join(out) + "\n"


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    report = parse(Path(argv[0]).read_text(encoding="utf-8", errors="replace"))
    if not report["regions"]:
        print(f"linker report: no MEMORY CONFIGURATION in {argv[0]}", file=sys.stderr)
        return 1
    stem = Path(argv[1])
    stem.with_suffix(".layout.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    stem.with_suffix(".layout.txt").write_text(text_report(report), encoding="utf-8")
    for r in report["regions"]:
        print(f"  {r['name']:10} {r['used']:#10x} of {r['length']:#10x} used ({r['percent']}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
