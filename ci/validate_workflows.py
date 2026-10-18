#!/usr/bin/env python3
"""Check GitHub workflows against the repository's supply-chain rules.

Every ``uses:`` must name a full 40-character commit that manifest/versions.json
pins for that Action, with the matching version in a trailing comment. Every
workflow must declare top-level ``permissions``. The required ``ci`` workflow
must keep its triggers and an always-reporting ``ci-gate`` job.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
USES = re.compile(r"^\s*-?\s*uses:\s*(\S+?)(?:\s*#\s*(\S+))?\s*$")
PIN = re.compile(r"^([\w.-]+/[\w.-]+)(?:/[\w./-]+)?@([0-9a-f]{40})$")


def pinned_actions(manifest: Path) -> dict[str, tuple[str, str]]:
    data = json.loads(manifest.read_text(encoding="utf-8"))
    pins = {}
    for item in data["inputs"]:
        if item["kind"] == "github-action":
            repo = item["source"]["url"].removeprefix("https://github.com/")
            pins[repo] = (item["integrity"]["commit"], item["version"])
    return pins


def check_workflow(path: Path, pins: dict[str, tuple[str, str]]) -> list[str]:
    problems = []
    text = path.read_text(encoding="utf-8")
    for number, line in enumerate(text.splitlines(), 1):
        match = USES.match(line)
        if not match:
            continue
        ref, comment = match.groups()
        if ref.startswith("./"):
            continue
        pin = PIN.match(ref)
        if not pin:
            problems.append(f"{path}:{number}: {ref} is not pinned to a full commit SHA")
            continue
        repo, commit = pin.groups()
        if repo not in pins:
            problems.append(f"{path}:{number}: {repo} is not an Action input in the manifest")
        elif pins[repo][0] != commit:
            problems.append(f"{path}:{number}: {repo}@{commit} differs from the manifest pin {pins[repo][0]}")
        elif comment != pins[repo][1]:
            problems.append(f"{path}:{number}: comment {comment} should name {pins[repo][1]}")
    if not re.search(r"^permissions:", text, re.M):
        problems.append(f"{path}: no top-level permissions block")
    return problems


def check_gateway(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    problems = []
    for required in ("types: [opened, synchronize, reopened, ready_for_review]", "merge_group:"):
        if required not in text:
            problems.append(f"{path}: the gateway must keep `{required}`")
    gate = re.search(r"^  ci-gate:\n((?:    .*\n|\n)*)", text, re.M)
    if not gate or "if: always()" not in gate.group(1):
        problems.append(f"{path}: ci-gate must exist and run with `if: always()`")
    if "pull_request_target" in text:
        problems.append(f"{path}: pull_request_target must not run pull-request code")
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("workflows", nargs="*", type=Path)
    parser.add_argument("--manifest", type=Path, default=ROOT / "manifest" / "versions.json")
    args = parser.parse_args(argv)
    paths = args.workflows or sorted((ROOT / ".github" / "workflows").glob("*.yml"))
    pins = pinned_actions(args.manifest)
    problems = [p for path in paths for p in check_workflow(path, pins)]
    gateway = ROOT / ".github" / "workflows" / "ci.yml"
    if not args.workflows:
        problems += check_gateway(gateway)
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        return 1
    print(f"validated {len(paths)} workflow(s) against {len(pins)} pinned Actions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
