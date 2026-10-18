#!/usr/bin/env python3
"""Line and branch coverage of the portable C code, from gcov's JSON output.

Run after the tests of a coverage build: ``python3 ci/coverage.py
build/host-gcc-coverage``. It reads every ``.gcda`` file, keeps sources under
the measured directories (tests excluded), writes ``coverage.json`` into the
build directory, and prints a table. It reports; it does not enforce a
threshold until one is accepted (Task 023).
"""

from __future__ import annotations

import json
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MEASURED = ("firmware/common/", "host/libsdos_dsp/src/", "host/dspctl/", "algorithms/")
GCOV = "gcov-12"


def collect(build: Path) -> dict[str, dict[str, int]]:
    totals: dict[str, dict[str, int]] = defaultdict(lambda: {"lines": 0, "lines_hit": 0, "branches": 0,
                                                             "branches_hit": 0})
    seen: dict[str, dict[int, int]] = defaultdict(dict)
    branches: dict[str, dict[tuple[int, int], int]] = defaultdict(dict)
    for gcda in sorted(build.rglob("*.gcda")):
        result = subprocess.run([GCOV, "--json-format", "--stdout", "--branch-probabilities", "-o",
                                 str(gcda.parent), str(gcda)], capture_output=True, text=True, check=False,
                                cwd=build)
        for line in result.stdout.splitlines():
            if not line.startswith("{"):
                continue
            for record in json.loads(line).get("files", []):
                path = Path(record["file"])
                path = path if path.is_absolute() else (build / path)
                try:
                    relative = str(path.resolve().relative_to(ROOT))
                except ValueError:
                    continue
                if not relative.startswith(MEASURED) or "/tests/" in relative:
                    continue
                for entry in record.get("lines", []):
                    number = entry["line_number"]
                    seen[relative][number] = max(seen[relative].get(number, 0), entry["count"])
                    for index, branch in enumerate(entry.get("branches", [])):
                        key = (number, index)
                        branches[relative][key] = max(branches[relative].get(key, 0), branch["count"])
    for relative, lines in seen.items():
        totals[relative]["lines"] = len(lines)
        totals[relative]["lines_hit"] = sum(1 for count in lines.values() if count > 0)
        totals[relative]["branches"] = len(branches[relative])
        totals[relative]["branches_hit"] = sum(1 for count in branches[relative].values() if count > 0)
    return dict(totals)


def percent(hit: int, total: int) -> float:
    return round(100.0 * hit / total, 1) if total else 100.0


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    build = Path(argv[0]).resolve()
    totals = collect(build)
    if not totals:
        print(f"coverage: no .gcda data under {build}; run the tests of a coverage build first", file=sys.stderr)
        return 1
    summary = {key: sum(t[key] for t in totals.values()) for key in ("lines", "lines_hit", "branches", "branches_hit")}
    report = {
        "schema": "sdos-coverage/1",
        "line_percent": percent(summary["lines_hit"], summary["lines"]),
        "branch_percent": percent(summary["branches_hit"], summary["branches"]),
        **summary,
        "files": {path: {**t, "line_percent": percent(t["lines_hit"], t["lines"]),
                         "branch_percent": percent(t["branches_hit"], t["branches"])}
                  for path, t in sorted(totals.items())},
    }
    (build / "coverage.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{'file':52} {'lines':>8} {'branches':>9}")
    for path, t in report["files"].items():
        print(f"{path:52} {t['line_percent']:7.1f}% {t['branch_percent']:8.1f}%")
    print(f"{'total':52} {report['line_percent']:7.1f}% {report['branch_percent']:8.1f}%")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
