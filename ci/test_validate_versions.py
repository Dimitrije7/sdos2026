#!/usr/bin/env python3
"""Portable tests for the version-manifest contract."""

from __future__ import annotations

import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from typing import Any

from ci import validate_versions as subject


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
FIXTURES = REPOSITORY_ROOT / "ci" / "fixtures" / "versions"


def load_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


class VersionManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.schema = load_json(REPOSITORY_ROOT / "manifest/schemas/versions.schema.json")
        cls.matrix = load_json(REPOSITORY_ROOT / "manifest/license-matrix.json")
        cls.manifest = load_json(REPOSITORY_ROOT / "manifest/versions.json")
        cls.minimal = load_json(FIXTURES / "valid-minimal.json")

    def validate(self, manifest: dict[str, Any]) -> None:
        subject.validate_against_schema(manifest, self.schema, Path("manifest.json"))
        subject.validate_manifest(manifest, self.matrix)

    def entry(self, manifest: dict[str, Any], identity: str) -> dict[str, Any]:
        return next(item for item in manifest["inputs"] if item["id"] == identity)

    def test_repository_manifest_is_valid(self) -> None:
        self.validate(self.manifest)

    def test_valid_fixture_passes(self) -> None:
        self.validate(self.minimal)

    def test_fixture_with_missing_hash_is_rejected(self) -> None:
        with self.assertRaisesRegex(subject.ValidationFailure, "'sha256' is a required"):
            self.validate(load_json(FIXTURES / "invalid-missing-hash.json"))

    def test_fixture_following_latest_is_rejected(self) -> None:
        with self.assertRaisesRegex(subject.ValidationFailure, "follows latest"):
            self.validate(load_json(FIXTURES / "invalid-latest.json"))

    def test_fixture_with_unknown_state_is_rejected(self) -> None:
        with self.assertRaisesRegex(subject.ValidationFailure, "'beta' is not one of"):
            self.validate(load_json(FIXTURES / "invalid-state.json"))

    def test_moving_version_token_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        self.entry(manifest, "ti.c6000-cgt-8.3.7")["version"] = "stable"
        with self.assertRaisesRegex(subject.ValidationFailure, "moving reference"):
            self.validate(manifest)

    def test_raw_github_branch_url_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.manifest)
        entry = self.entry(manifest, "beagle.bbai64-iot-2026-10-09-bmap")
        entry["source"]["url"] = entry["source"]["url"].replace(
            "42d2a0a0283f033db04bff9ccb8204a57426cdb0", "refs/heads/trunk"
        )
        with self.assertRaisesRegex(subject.ValidationFailure, "must name a commit"):
            self.validate(manifest)

    def test_oci_image_requires_digest(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        del self.entry(manifest, "ubuntu.jammy-20260924.1")["integrity"]["index_digest"]
        with self.assertRaisesRegex(subject.ValidationFailure, "'index_digest' is a required"):
            self.validate(manifest)

    def test_git_input_requires_full_commit(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        self.entry(manifest, "action.actions.checkout.v7.0.1")["integrity"]["commit"] = "3d3c42e"
        with self.assertRaisesRegex(subject.ValidationFailure, "does not match"):
            self.validate(manifest)

    def test_missing_section4_component_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        del manifest["section4"]["C71x compiler"]
        with self.assertRaisesRegex(subject.ValidationFailure, "'C71x compiler' is a required"):
            self.validate(manifest)

    def test_section4_unknown_input_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        manifest["section4"]["Linux"] = ["linux.not-pinned"]
        with self.assertRaisesRegex(subject.ValidationFailure, "names unknown inputs"):
            self.validate(manifest)

    def test_unknown_license_artifact_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        self.entry(manifest, "buildkit.v0.33.1")["license_matrix"]["artifact"] = "unknown.tool"
        with self.assertRaisesRegex(subject.ValidationFailure, "unknown license-matrix artifact"):
            self.validate(manifest)

    def test_unknown_license_component_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        self.entry(manifest, "buildkit.v0.33.1")["license_matrix"]["component"] = "Moby"
        with self.assertRaisesRegex(subject.ValidationFailure, "has no component 'Moby'"):
            self.validate(manifest)

    def test_ungated_acquisition_of_controlled_input_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        self.entry(manifest, "ti.c7000-cgt-5.0.0.lts")["acquisition"] = {"status": "allowed"}
        with self.assertRaisesRegex(subject.ValidationFailure, "license-matrix download"):
            self.validate(manifest)

    def test_gated_acquisition_requires_known_blocker(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        self.entry(manifest, "ti.c7000-cgt-5.0.0.lts")["acquisition"]["blocker_ids"] = ["LIC-999"]
        with self.assertRaisesRegex(subject.ValidationFailure, "unknown license blocker IDs"):
            self.validate(manifest)

    def test_child_of_gated_input_must_be_gated(self) -> None:
        manifest = copy.deepcopy(self.manifest)
        self.entry(manifest, "ti.c66x-dsplib-3.4.0.0")["acquisition"] = {"status": "gated", "blocker_ids": []}
        with self.assertRaisesRegex(subject.ValidationFailure, "should be non-empty|is too short"):
            self.validate(manifest)
        entry = self.entry(manifest, "ti.mmalib-11.02.00.11")
        entry["license_matrix"] = {"artifact": "beagle.debian-image"}
        entry["acquisition"] = {"status": "allowed"}
        self.entry(manifest, "ti.c66x-dsplib-3.4.0.0")["acquisition"] = {
            "status": "gated",
            "blocker_ids": ["LIC-001"],
        }
        with self.assertRaisesRegex(subject.ValidationFailure, "not gated itself"):
            self.validate(manifest)

    def test_containment_cycle_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        kernel = self.entry(manifest, "beagle.linux-image-6.18.55-arm64-k3-r59")
        kernel["integrity"]["contained_in"] = kernel["id"]
        with self.assertRaisesRegex(subject.ValidationFailure, "containment cycle"):
            self.validate(manifest)

    def test_qualified_state_requires_existing_evidence(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        entry = self.entry(manifest, "ti.c6000-cgt-8.3.7")
        entry["state"] = "qualified"
        with self.assertRaisesRegex(subject.ValidationFailure, "'qualification_evidence' is a required"):
            self.validate(manifest)
        entry["qualification_evidence"] = "release/evidence/missing.json"
        with self.assertRaisesRegex(subject.ValidationFailure, "does not exist"):
            self.validate(manifest)

    def test_superseded_state_requires_live_successor(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        entry = self.entry(manifest, "buildkit.v0.33.1")
        entry["state"] = "superseded"
        entry["superseded_by"] = "buildkit.v0.99.0"
        with self.assertRaisesRegex(subject.ValidationFailure, "superseded_by names unknown"):
            self.validate(manifest)

    def test_duplicate_ids_are_rejected(self) -> None:
        manifest = copy.deepcopy(self.minimal)
        manifest["deferred"].append(
            {"id": "buildkit.v0.33.1", "name": "dup", "reason": "dup", "owner": "Task 017"}
        )
        with self.assertRaisesRegex(subject.ValidationFailure, "duplicate"):
            self.validate(manifest)

    def test_cache_verification_detects_matches_absence_and_tampering(self) -> None:
        payload = b"sdos cache verification payload\n"
        manifest = copy.deepcopy(self.minimal)
        entry = self.entry(manifest, "ti.c6000-cgt-8.3.7")
        entry["integrity"]["sha256"] = hashlib.sha256(payload).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            self.assertEqual(subject.verify_cache(manifest, cache)[0], 0)
            (cache / entry["source"]["file_name"]).write_bytes(payload)
            verified, absent = subject.verify_cache(manifest, cache)
            self.assertEqual(verified, 1)
            self.assertGreater(absent, 0)
            (cache / entry["source"]["file_name"]).write_bytes(payload + b"x")
            with self.assertRaisesRegex(subject.ValidationFailure, "sha256 mismatch"):
                subject.verify_cache(manifest, cache)

    def test_cache_verification_checks_npm_sha512(self) -> None:
        manifest = copy.deepcopy(self.manifest)
        entry = self.entry(manifest, "npm.markdownlint-cli2.0.23.3")
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / entry["source"]["file_name"]).write_bytes(b"not the package")
            with self.assertRaisesRegex(subject.ValidationFailure, "sha512 mismatch"):
                subject.verify_cache(manifest, cache)


if __name__ == "__main__":
    unittest.main()
