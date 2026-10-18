#!/usr/bin/env python3
"""Portable tests for license-matrix default-deny invariants."""

from __future__ import annotations

import copy
import json
import unittest
from pathlib import Path
from typing import Any

from ci import validate_licenses as subject


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]


def load_json(relative_path: str) -> dict[str, Any]:
    with (REPOSITORY_ROOT / relative_path).open(encoding="utf-8") as handle:
        return json.load(handle)


class LicenseMatrixValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.matrix = load_json("manifest/license-matrix.json")
        cls.schema = load_json("manifest/schemas/license-matrix.schema.json")

    def artifact(self, matrix: dict[str, Any], artifact_id: str) -> dict[str, Any]:
        return next(
            artifact
            for artifact in matrix["artifacts"]
            if artifact["id"] == artifact_id
        )

    def test_repository_matrix_is_valid(self) -> None:
        subject.validate_against_schema(
            self.matrix, self.schema, Path("manifest/license-matrix.json")
        )
        subject.validate_matrix(self.matrix)

    def test_public_pr_cannot_require_credentials(self) -> None:
        matrix = copy.deepcopy(self.matrix)
        artifact = self.artifact(matrix, "cppcheck.open-source")
        artifact["handling"]["credentials_required"] = True

        with self.assertRaisesRegex(
            subject.ValidationFailure, "public PR handling cannot require credentials"
        ):
            subject.validate_matrix(matrix)

    def test_public_pr_cannot_accept_license_terms(self) -> None:
        matrix = copy.deepcopy(self.matrix)
        artifact = self.artifact(matrix, "cppcheck.open-source")
        artifact["handling"]["license_acceptance_required"] = True

        with self.assertRaisesRegex(
            subject.ValidationFailure, "public PR handling cannot perform license"
        ):
            subject.validate_matrix(matrix)

    def test_proprietary_payload_cannot_enter_public_container(self) -> None:
        matrix = copy.deepcopy(self.matrix)
        artifact = self.artifact(matrix, "misra.c2012-materials")
        artifact["operations"]["public_container"]["status"] = "conditional"

        with self.assertRaisesRegex(
            subject.ValidationFailure,
            "proprietary or unresolved payload cannot enter the public container",
        ):
            subject.validate_matrix(matrix)

    def test_unresolved_conclusion_requires_blocker(self) -> None:
        matrix = copy.deepcopy(self.matrix)
        artifact = self.artifact(matrix, "beagle.debian-image")
        artifact["blocker_ids"] = []

        with self.assertRaisesRegex(
            subject.ValidationFailure, "unresolved conclusions require a blocker"
        ):
            subject.validate_matrix(matrix)

    def test_unknown_blocker_reference_is_rejected(self) -> None:
        matrix = copy.deepcopy(self.matrix)
        artifact = self.artifact(matrix, "cppcheck.open-source")
        artifact["blocker_ids"] = ["LIC-999"]

        with self.assertRaisesRegex(subject.ValidationFailure, "unknown blocker IDs"):
            subject.validate_matrix(matrix)

    def test_non_open_source_cannot_be_committed_as_source(self) -> None:
        matrix = copy.deepcopy(self.matrix)
        artifact = self.artifact(matrix, "ti.c7000-cgt")
        artifact["handling"]["repository_payload"] = "source"

        with self.assertRaisesRegex(
            subject.ValidationFailure,
            "repository source requires an open-source license conclusion",
        ):
            subject.validate_matrix(matrix)


if __name__ == "__main__":
    unittest.main()
