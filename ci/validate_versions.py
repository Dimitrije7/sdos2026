#!/usr/bin/env python3
"""Validate the SDOS platform version manifest and verify cached inputs."""

from __future__ import annotations

import argparse
import base64
import hashlib
import re
import sys
from pathlib import Path
from typing import Any, Iterable
from urllib.parse import urlsplit

try:
    from ci.validate_hardware import (
        ValidationFailure,
        check_url,
        load_json,
        require_unique,
        validate_against_schema,
    )
except ModuleNotFoundError:  # Direct execution puts ci/ first on sys.path.
    from validate_hardware import (  # type: ignore[no-redef]
        ValidationFailure,
        check_url,
        load_json,
        require_unique,
        validate_against_schema,
    )


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = REPOSITORY_ROOT / "manifest" / "versions.json"
DEFAULT_SCHEMA = REPOSITORY_ROOT / "manifest" / "schemas" / "versions.schema.json"
DEFAULT_LICENSE_MATRIX = REPOSITORY_ROOT / "manifest" / "license-matrix.json"

MUTABLE_TOKENS = {"latest", "stable", "nightly", "edge", "head", "main", "master"}
HEX_COMMIT = re.compile(r"^[0-9a-f]{40}$")
FILE_KINDS = {"file", "pypi", "npm"}


def _mutable_reason(value: str) -> str | None:
    lowered = value.lower()
    if lowered in MUTABLE_TOKENS:
        return f"'{value}' is a moving reference"
    if re.search(r"(^|[/:@_-])latest($|[/._-])", lowered):
        return f"'{value}' follows latest"
    return None


def _raw_github_ref(url: str) -> str | None:
    parts = urlsplit(url)
    if parts.netloc != "raw.githubusercontent.com":
        return None
    segments = parts.path.strip("/").split("/")
    if len(segments) < 3:
        return None
    if segments[2] == "refs" and len(segments) > 4:
        return "/".join(segments[2:5])
    return segments[2]


def check_immutability(entry: dict[str, Any]) -> None:
    identity = entry["id"]
    candidates = [entry["version"], entry["source"]["url"]]
    candidates.extend(
        entry["source"][key] for key in ("ref", "file_name") if key in entry["source"]
    )
    for value in candidates:
        reason = _mutable_reason(value)
        if reason:
            raise ValidationFailure(f"{identity}: mutable input: {reason}")
        for segment in re.split(r"[/:@]", value):
            reason = _mutable_reason(segment)
            if reason and entry["kind"] != "runner-label":
                raise ValidationFailure(f"{identity}: mutable input: {reason}")

    if entry["kind"] in FILE_KINDS:
        raw_ref = _raw_github_ref(entry["source"]["url"])
        if raw_ref is not None and not HEX_COMMIT.match(raw_ref):
            raise ValidationFailure(
                f"{identity}: raw GitHub URL must name a commit, not '{raw_ref}'"
            )
        last_segment = urlsplit(entry["source"]["url"]).path.rsplit("/", 1)[-1]
        if last_segment != entry["source"]["file_name"]:
            raise ValidationFailure(
                f"{identity}: file_name does not match the URL's final path segment"
            )


def _matrix_components(artifact: dict[str, Any]) -> set[str]:
    return {component["name"] for component in artifact["components"]}


def check_license_links(
    entry: dict[str, Any],
    artifacts: dict[str, dict[str, Any]],
    all_components: set[str],
    blocker_ids: set[str],
) -> None:
    identity = entry["id"]
    reference = entry["license_matrix"]
    artifact = artifacts.get(reference["artifact"])
    if artifact is None:
        raise ValidationFailure(
            f"{identity}: unknown license-matrix artifact {reference['artifact']}"
        )
    component = reference.get("component")
    if component is not None and component not in _matrix_components(artifact):
        raise ValidationFailure(
            f"{identity}: license-matrix artifact {artifact['id']} has no component "
            f"'{component}'"
        )
    if entry["kind"] == "apt-snapshot":
        for package in entry["integrity"]["packages"]:
            name = package["matrix_component"]
            if name is not None and name not in all_components:
                raise ValidationFailure(
                    f"{identity}: package {package['name']} names unknown license-matrix "
                    f"component '{name}'"
                )

    acquisition = entry["acquisition"]
    unknown = set(acquisition.get("blocker_ids", [])) - blocker_ids
    if unknown:
        raise ValidationFailure(
            f"{identity}: unknown license blocker IDs: {', '.join(sorted(unknown))}"
        )
    download = artifact["operations"]["download"]["status"]
    if acquisition["status"] == "allowed" and download != "allowed":
        raise ValidationFailure(
            f"{identity}: acquisition is allowed but license-matrix download for "
            f"{artifact['id']} is {download}"
        )


def check_containment(inputs: dict[str, dict[str, Any]]) -> None:
    for identity, entry in inputs.items():
        seen = [identity]
        current = entry
        while current["kind"] == "contained":
            parent_id = current["integrity"]["contained_in"]
            parent = inputs.get(parent_id)
            if parent is None:
                raise ValidationFailure(f"{identity}: contained_in names unknown input {parent_id}")
            if parent_id in seen:
                raise ValidationFailure(f"{identity}: containment cycle through {parent_id}")
            if parent["acquisition"]["status"] == "gated" and entry["acquisition"]["status"] != "gated":
                raise ValidationFailure(
                    f"{identity}: contained in gated input {parent_id} but not gated itself"
                )
            seen.append(parent_id)
            current = parent


def check_lifecycle(entry: dict[str, Any], inputs: dict[str, dict[str, Any]]) -> None:
    identity = entry["id"]
    if entry["state"] == "superseded":
        successor = inputs.get(entry["superseded_by"])
        if successor is None:
            raise ValidationFailure(f"{identity}: superseded_by names unknown input")
        if successor["state"] == "superseded":
            raise ValidationFailure(f"{identity}: successor is itself superseded")
    if entry["state"] in {"qualified", "semester-frozen"}:
        evidence = REPOSITORY_ROOT / entry["qualification_evidence"]
        if not evidence.exists():
            raise ValidationFailure(
                f"{identity}: qualification evidence {entry['qualification_evidence']} "
                "does not exist"
            )


def validate_manifest(manifest: dict[str, Any], matrix: dict[str, Any]) -> None:
    entries = manifest["inputs"]
    require_unique(
        [entry["id"] for entry in entries] + [item["id"] for item in manifest["deferred"]],
        "version manifest input IDs",
    )
    inputs = {entry["id"]: entry for entry in entries}
    artifacts = {artifact["id"]: artifact for artifact in matrix["artifacts"]}
    all_components = {
        name for artifact in matrix["artifacts"] for name in _matrix_components(artifact)
    }
    blocker_ids = {blocker["id"] for blocker in matrix["blockers"]}

    for component, ids in manifest["section4"].items():
        missing = [identity for identity in ids if identity not in inputs]
        if missing:
            raise ValidationFailure(
                f"Section 4 component '{component}' names unknown inputs: "
                f"{', '.join(missing)}"
            )

    for entry in entries:
        check_immutability(entry)
        check_license_links(entry, artifacts, all_components, blocker_ids)
        check_lifecycle(entry, inputs)
    check_containment(inputs)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def _sha512_integrity(path: Path) -> str:
    digest = hashlib.sha512()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return "sha512-" + base64.b64encode(digest.digest()).decode("ascii")


def iter_cacheable(manifest: dict[str, Any]) -> Iterable[tuple[str, str, str, str]]:
    """Yield (input id, file name, algorithm, expected value) for hashed files."""
    for entry in manifest["inputs"]:
        integrity = entry["integrity"]
        if entry["kind"] in {"file", "pypi"}:
            yield entry["id"], entry["source"]["file_name"], "sha256", integrity["sha256"]
        elif entry["kind"] == "npm":
            yield (
                entry["id"],
                entry["source"]["file_name"],
                "sha512",
                integrity["sha512_integrity"],
            )
        elif entry["kind"] == "apt-snapshot":
            for package in integrity["packages"]:
                yield entry["id"], package["file_name"], "sha256", package["sha256"]


def verify_cache(manifest: dict[str, Any], cache_dir: Path) -> tuple[int, int]:
    verified = missing = 0
    mismatches: list[str] = []
    for identity, file_name, algorithm, expected in iter_cacheable(manifest):
        path = cache_dir / file_name
        if not path.is_file():
            missing += 1
            continue
        actual = _sha256(path) if algorithm == "sha256" else _sha512_integrity(path)
        if actual != expected:
            mismatches.append(f"{identity}: {file_name} {algorithm} mismatch")
        else:
            verified += 1
    if mismatches:
        raise ValidationFailure("; ".join(mismatches))
    return verified, missing


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA)
    parser.add_argument("--license-matrix", type=Path, default=DEFAULT_LICENSE_MATRIX)
    parser.add_argument(
        "--verify-cache",
        type=Path,
        metavar="DIR",
        help="verify every manifest file present in DIR against its recorded hash",
    )
    parser.add_argument(
        "--check-links",
        action="store_true",
        help="perform network checks for evidence URLs",
    )
    parser.add_argument("--link-timeout", type=float, default=20.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = load_json(args.manifest)
    validate_against_schema(manifest, load_json(args.schema), args.manifest)
    validate_manifest(manifest, load_json(args.license_matrix))
    if args.check_links:
        for url in sorted({entry["evidence"] for entry in manifest["inputs"]}):
            check_url(url, args.link_timeout)
    states: dict[str, int] = {}
    for entry in manifest["inputs"]:
        states[entry["state"]] = states.get(entry["state"], 0) + 1
    print(f"inputs: {len(manifest['inputs'])}; deferred: {len(manifest['deferred'])}")
    print("states: " + ", ".join(f"{k}={v}" for k, v in sorted(states.items())))
    if args.verify_cache is not None:
        if not args.verify_cache.is_dir():
            raise ValidationFailure(f"cache directory {args.verify_cache} does not exist")
        verified, missing = verify_cache(manifest, args.verify_cache)
        print(f"cache {args.verify_cache}: verified={verified} absent={missing}")
    print(
        f"validated version manifest {manifest['manifest_id']} revision "
        f"{manifest['revision']}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValidationFailure as exc:
        print(f"version validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
