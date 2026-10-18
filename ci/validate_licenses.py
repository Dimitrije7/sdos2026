#!/usr/bin/env python3
"""Validate the SDOS third-party license and distribution matrix."""

from __future__ import annotations

import argparse
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

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
DEFAULT_MATRIX = REPOSITORY_ROOT / "manifest" / "license-matrix.json"
DEFAULT_SCHEMA = (
    REPOSITORY_ROOT / "manifest" / "schemas" / "license-matrix.schema.json"
)


def validate_matrix(matrix: dict[str, Any]) -> None:
    artifacts = matrix["artifacts"]
    blockers = matrix["blockers"]
    require_unique((artifact["id"] for artifact in artifacts), "artifact IDs")
    require_unique((blocker["id"] for blocker in blockers), "license blocker IDs")

    blocker_ids = {blocker["id"] for blocker in blockers}
    referenced_blockers: set[str] = set()
    for artifact in artifacts:
        require_unique(
            (component["name"] for component in artifact["components"]),
            f"component names for {artifact['id']}",
        )
        artifact_blockers = set(artifact["blocker_ids"])
        unknown_blockers = artifact_blockers - blocker_ids
        if unknown_blockers:
            raise ValidationFailure(
                f"{artifact['id']}: unknown blocker IDs: "
                f"{', '.join(sorted(unknown_blockers))}"
            )
        referenced_blockers.update(artifact_blockers)

        unresolved_operations = [
            operation
            for operation, decision in artifact["operations"].items()
            if decision["status"] == "unresolved"
        ]
        if (
            unresolved_operations or artifact["license"]["class"] == "unresolved"
        ) and not artifact_blockers:
            raise ValidationFailure(
                f"{artifact['id']}: unresolved conclusions require a blocker"
            )

        handling = artifact["handling"]
        if handling["lane"] == "public-pr":
            if handling["credentials_required"]:
                raise ValidationFailure(
                    f"{artifact['id']}: public PR handling cannot require credentials"
                )
            if handling["license_acceptance_required"]:
                raise ValidationFailure(
                    f"{artifact['id']}: public PR handling cannot perform license "
                    "acceptance"
                )
            if artifact["operations"]["ci_use"]["status"] not in {
                "allowed",
                "conditional",
            }:
                raise ValidationFailure(
                    f"{artifact['id']}: public PR lane requires permitted CI use"
                )

        if handling["repository_payload"] == "source" and artifact["license"][
            "class"
        ] != "open-source":
            raise ValidationFailure(
                f"{artifact['id']}: repository source requires an open-source "
                "license conclusion"
            )

        public_container = artifact["operations"]["public_container"]["status"]
        if artifact["license"]["class"] in {"proprietary", "unresolved"} and (
            public_container in {"allowed", "conditional"}
        ):
            raise ValidationFailure(
                f"{artifact['id']}: proprietary or unresolved payload cannot enter "
                "the public container"
            )

    unreferenced_blockers = blocker_ids - referenced_blockers
    if unreferenced_blockers:
        raise ValidationFailure(
            "unreferenced license blockers: "
            f"{', '.join(sorted(unreferenced_blockers))}"
        )


def iter_urls(matrix: dict[str, Any]) -> Iterable[str]:
    for artifact in matrix["artifacts"]:
        yield artifact["source"]["canonical_url"]
        yield from artifact["source"]["license_urls"]
        for component in artifact["components"]:
            yield component["source_url"]
            yield component["license_url"]


def print_summary(matrix: dict[str, Any]) -> None:
    license_classes = Counter(
        artifact["license"]["class"] for artifact in matrix["artifacts"]
    )
    lanes = Counter(
        artifact["handling"]["lane"] for artifact in matrix["artifacts"]
    )
    print(f"artifacts: {len(matrix['artifacts'])}")
    print(
        "license classes: "
        + ", ".join(
            f"{name}={count}" for name, count in sorted(license_classes.items())
        )
    )
    print(
        "handling lanes: "
        + ", ".join(f"{name}={count}" for name, count in sorted(lanes.items()))
    )
    print(f"blockers: {len(matrix['blockers'])}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", type=Path, default=DEFAULT_MATRIX)
    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA)
    parser.add_argument(
        "--check-links",
        action="store_true",
        help="perform network checks for canonical and license URLs",
    )
    parser.add_argument("--link-timeout", type=float, default=20.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    matrix = load_json(args.matrix)
    schema = load_json(args.schema)
    validate_against_schema(matrix, schema, args.matrix)
    validate_matrix(matrix)
    if args.check_links:
        for url in sorted(set(iter_urls(matrix))):
            check_url(url, args.link_timeout)
    print_summary(matrix)
    print(
        f"validated license matrix {matrix['matrix_id']} revision "
        f"{matrix['revision']} ({matrix['decision_status']})"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValidationFailure as exc:
        print(f"license validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
