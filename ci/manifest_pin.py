#!/usr/bin/env python3
"""Print one field of a pinned input from manifest/versions.json.

Shell scripts use this instead of repeating URLs and hashes, so the manifest
stays the single source of truth. Fields: url, file_name, sha256, version.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

MANIFEST = Path(__file__).resolve().parents[1] / "manifest" / "versions.json"
FIELDS = {
    "url": lambda entry: entry["source"]["url"],
    "file_name": lambda entry: entry["source"]["file_name"],
    "sha256": lambda entry: entry["integrity"]["sha256"],
    "version": lambda entry: entry["version"],
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_id")
    parser.add_argument("field", choices=sorted(FIELDS))
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    args = parser.parse_args()

    with args.manifest.open(encoding="utf-8") as handle:
        inputs = {entry["id"]: entry for entry in json.load(handle)["inputs"]}
    entry = inputs.get(args.input_id)
    if entry is None:
        print(f"unknown manifest input: {args.input_id}", file=sys.stderr)
        return 2
    try:
        print(FIELDS[args.field](entry))
    except KeyError:
        print(f"{args.input_id} has no {args.field}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
