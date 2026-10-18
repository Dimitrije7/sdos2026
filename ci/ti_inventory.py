#!/usr/bin/env python3
"""Inventory the extracted TI SDK components against TI's shipped manifests.

Run inside the TI image: ``python3 ci/ti_inventory.py --out <file.json>``. For
each extracted component it records the file count, size, and a tree hash, then
parses the component's TI software manifest and assigns every file to the
manifest row whose location covers it. Files that no row covers are listed by
directory, so a reviewer can reconcile them. No TI file content is copied into
the output: only paths, counts, and hashes.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import os
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(os.environ.get("SDOS_TI_PSDK_ROOT", "/opt/ti/psdk/current"))
MANIFESTS = {
    "pdk_jacinto_11_02_01_08": "docs/manifest/pdk_jacinto_manifest.html",
    # docs/DSPLIB_Software_Manifest.html only redirects to this file.
    "dsplib_c66x_3_4_0_0": "packages/ti/dsplib/docs/manifest/Software_Manifest.html",
    "dsplib": "docs/DSPLIB_manifest.html",
    "fftlib": "docs/FFTLIB_manifest.html",
    "mmalib_11_02_00_11": "docs/manifest/MMALIB_11.02.00_manifest.html",
    "psdk_rtos": "PROCESSOR_SDK_RTOS_J721E_manifest.html",
}
FIELDS = ("Software Name", "Version", "License Type", "Delivered As", "Modified by TI")


def _cells(row: str) -> list[str]:
    raw = re.findall(r"<t[dh][^>]*>(.*?)</t[dh]>", row, re.S | re.I)
    return [html.unescape(re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", c))).strip() for c in raw]


def parse_manifest(path: Path) -> list[dict[str, str]]:
    """Return one entry per software row: name, version, license, location."""
    text = path.read_text(encoding="utf-8", errors="replace")
    entries: list[dict[str, str]] = []
    current: dict[str, str] | None = None
    for row in re.findall(r"<tr[^>]*>(.*?)</tr>", text, re.S | re.I):
        cells = _cells(row)
        if len(cells) >= 5 and cells[0] not in FIELDS and current is None and "Location" in cells:
            current = dict(zip(FIELDS, cells[:5]))
            current["Location"] = cells[cells.index("Location") + 1] if cells.index("Location") + 1 < len(cells) else ""
            entries.append(current)
            current = None
        elif len(cells) >= 2 and cells[0] == "Obtained from" and entries:
            entries[-1].setdefault("Obtained from", cells[1])
    return entries


def location_prefixes(location: str, component: str) -> list[str] | None:
    """Turn a manifest location into relative path prefixes.

    '[as installed]/packages/ti/drv/*' gives 'packages/ti/drv'. A location that
    names the whole component, such as '~/DSPLIB' or '[as installed]/*', gives
    ''. A row with no path, such as 'as installed', gives None: it is recorded
    but covers no files. Comma-separated lists give several prefixes.
    """
    prefixes = []
    for part in location.split(","):
        value = part.strip().split(" except")[0].strip()
        if not re.search(r"[/~]", value):
            continue
        value = re.sub(r"^(\[as[ _]installed\]|~|\$\w+|<[^>]+>)/?", "", value, flags=re.I)
        value = value.rstrip("*").strip().rstrip("/")
        # '~/DSPLIB' or '~/ti/dsplib' name the install root of the component.
        if value.lower().split("/")[-1] in (component.lower(), component.split("_")[0].lower()):
            value = ""
        prefixes.append(value.lower())
    return prefixes or None


def inventory_component(component: str) -> dict[str, object]:
    base = ROOT / component
    files = sorted(p for p in base.rglob("*") if p.is_file() and not p.is_symlink())
    tree = hashlib.sha256()
    size = 0
    for path in files:
        rel = path.relative_to(base).as_posix()
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        tree.update(f"{digest}  {rel}\n".encode())
        size += path.stat().st_size
    manifest = base / MANIFESTS[component]
    entries = parse_manifest(manifest) if manifest.exists() else []
    prefixes = []
    for entry in entries:
        for prefix in location_prefixes(entry.get("Location", ""), component) or []:
            prefixes.append((prefix, entry))
    prefixes.sort(key=lambda item: len(item[0]), reverse=True)
    # 'Vx' in a manifest path stands for any IP version directory (V0, V1, ...).
    patterns = [(re.compile("^" + re.escape(p).replace("/vx", r"/v\w*")), e) for p, e in prefixes]
    by_license: Counter[str] = Counter()
    uncovered: Counter[str] = Counter()
    for path in files:
        rel = path.relative_to(base).as_posix().lower()
        match = next((e for p, e in patterns if p.match(rel)), None)
        if match is None:
            uncovered["/".join(rel.split("/")[:2])] += 1
        else:
            by_license[match.get("License Type", "?")] += 1
    return {
        "component": component,
        "files": len(files),
        "bytes": size,
        "tree_sha256": tree.hexdigest(),
        "manifest": MANIFESTS[component] if manifest.exists() else None,
        "manifest_sha256": hashlib.sha256(manifest.read_bytes()).hexdigest() if manifest.exists() else None,
        "license_files": {
            p.relative_to(base).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in files
            if re.search(r"licen[cs]e|copying|tspa", p.name, re.I) and p.suffix.lower() in (".pdf", ".txt", ".htm", ".html", "")
        },
        "manifest_entries": [
            {k: e.get(k, "") for k in ("Software Name", "Version", "License Type", "Delivered As", "Location", "Obtained from")}
            for e in entries
        ],
        "files_by_license": dict(by_license.most_common()),
        "uncovered_files_by_directory": dict(uncovered.most_common()),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    archive_sha = Path("/opt/ti/psdk.archive.sha256")
    report = {
        "psdk_root": str(ROOT.resolve()),
        "archive_sha256": archive_sha.read_text().strip() if archive_sha.exists() else None,
        "components": [inventory_component(c) for c in MANIFESTS if (ROOT / c).is_dir()],
    }
    args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for item in report["components"]:
        print(f"{item['component']:28} {item['files']:6} files  manifest rows {len(item['manifest_entries']):3}  "
              f"uncovered {sum(item['uncovered_files_by_directory'].values())}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
