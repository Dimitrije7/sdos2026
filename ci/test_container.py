#!/usr/bin/env python3
"""Tests that the container is built only from manifest pins and runs commands safely."""

from __future__ import annotations

import contextlib
import io
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from ci import container, facade

LOCK = container.ROOT / "toolchain" / "python-tools.lock"


def run(*argv: str) -> tuple[int, str]:
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
        code = facade.main(list(argv))
    return code, out.getvalue()


@mock.patch.dict(os.environ, {"SDOS_NATIVE": "", "SDOS_IN_CONTAINER": ""})
class ContainerTests(unittest.TestCase):
    def test_build_commands_rerun_inside_the_pinned_image(self) -> None:
        code, text = run("test", "--preset", "host", "--dry-run")
        self.assertEqual(code, facade.EXIT_OK)
        self.assertIn("docker run --rm --init", text)
        self.assertIn(container.image_tag() + " python3 ci/facade.py test --preset host", text)

    def test_bootstrap_builds_from_manifest_pins(self) -> None:
        code, text = run("bootstrap", "--dry-run")
        self.assertEqual(code, facade.EXIT_OK)
        self.assertIn("APT_PACKAGES=ca-certificates=20260601~22.04.1", text)
        self.assertIn("cmake=3.22.1-1ubuntu1.22.04.2", text)
        self.assertIn("BASE_IMAGE=docker.io/library/ubuntu@sha256:5ec03bb3", text)

    def test_deb_checks_use_apt_cache_names_with_escaped_epochs(self) -> None:
        checks = container.build_args()["APT_DEB_SHA256"]
        self.assertIn("git_1%3a2.34.1-1ubuntu1.17_amd64.deb=8794fcf2", checks)
        self.assertIn("ca-certificates_20260601~22.04.1_all.deb=6e8cdcc8", checks)

    def test_dockerfile_frontend_matches_the_manifest(self) -> None:
        first = container.DOCKERFILE.read_text(encoding="utf-8").splitlines()[0]
        self.assertEqual(first, "# syntax=" + container.frontend_reference())

    def test_tag_changes_with_any_pin(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            manifest = Path(tmp) / "versions.json"
            text = container.MANIFEST.read_text(encoding="utf-8")
            manifest.write_text(text.replace("3.22.1-1ubuntu1.22.04.2", "3.22.1-1ubuntu1.22.04.1"), encoding="utf-8")
            self.assertNotEqual(container.image_tag(manifest=manifest), container.image_tag())

    def test_rootful_docker_runs_as_the_invoking_user(self) -> None:
        argv = container.run_command("img", ["true"], rootless=False)
        self.assertIn(f"{os.getuid()}:{os.getgid()}", argv)
        self.assertNotIn("--user", container.run_command("img", ["true"], rootless=True))



    def test_bootstrap_untags_only_images_from_earlier_inputs(self) -> None:
        listed = ["sdos-dev:current", "sdos-dev:dev-aaa", "sdos-dev:dev-ti-bbb", "sdos-dev:dev-ti-ccc", "other:dev-x"]
        keep = {"sdos-dev:dev-aaa", "sdos-dev:dev-ti-ccc"}
        self.assertEqual(container.stale_tags(keep, listed), ["sdos-dev:dev-ti-bbb"])


class PythonLockTests(unittest.TestCase):
    def test_every_locked_wheel_has_a_hash(self) -> None:
        entries = re.findall(r"^([a-z0-9-]+)==(\S+) \\\n    --hash=sha256:([0-9a-f]{64})$", LOCK.read_text(), re.M)
        requirements = [line for line in LOCK.read_text().splitlines() if "==" in line]
        self.assertEqual(len(entries), len(requirements))
        self.assertGreaterEqual(len(entries), 5)

    def test_top_level_versions_match_the_manifest(self) -> None:
        locked = dict(re.findall(r"^([a-z0-9-]+)==(\S+) ", LOCK.read_text(), re.M))
        manifest = json.loads(container.MANIFEST.read_text())
        for item in manifest["inputs"]:
            if item["kind"] != "pypi" or item["id"] == "pypi.reuse.6.2.0":
                continue
            name = item["source"]["file_name"].split("-")[0].replace("_", "-").lower()
            self.assertEqual(locked.get(name), item["version"], name)


class NodeLockTests(unittest.TestCase):
    def test_markdownlint_matches_manifest_and_every_package_has_integrity(self) -> None:
        lock = json.loads((container.ROOT / "toolchain" / "node" / "package-lock.json").read_text())
        manifest = json.loads(container.MANIFEST.read_text())
        pin = next(i for i in manifest["inputs"] if i["id"] == "npm.markdownlint-cli2.0.23.3")
        entry = lock["packages"]["node_modules/markdownlint-cli2"]
        self.assertEqual(entry["version"], pin["version"])
        self.assertEqual(entry["integrity"], pin["integrity"]["sha512_integrity"])
        missing = [name for name, item in lock["packages"].items() if name and "integrity" not in item]
        self.assertEqual(missing, [])

    def test_every_file_tool_is_a_hashed_manifest_input(self) -> None:
        entries = container.build_args()["FILE_TOOLS"].split()
        self.assertEqual([e.split("|")[0] for e in entries], list(container.FILE_TOOLS))
        for entry in entries:
            self.assertRegex(entry.split("|")[2], r"^[0-9a-f]{64}$")


@unittest.skipUnless(shutil.which("hadolint"), "hadolint runs in the pinned container")
class LintFixtureTests(unittest.TestCase):
    def test_hadolint_rejects_an_unpinned_dockerfile(self) -> None:
        bad = container.ROOT / "ci" / "fixtures" / "lint" / "unpinned.Dockerfile"
        result = subprocess.run(["hadolint", str(bad)], capture_output=True, text=True, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("DL3007", result.stdout)  # the latest tag
        self.assertIn("DL3008", result.stdout)  # unpinned apt package


class TiTermsTests(unittest.TestCase):
    def test_ti_layer_is_built_only_after_acceptance(self) -> None:
        with tempfile.TemporaryDirectory() as tmp, mock.patch.dict(os.environ, {"SDOS_TI_TERMS": ""}):
            record = Path(tmp) / "accepted.json"
            self.assertFalse(container.ti_accepted(record=record))
            self.assertNotIn("TI_TOOLS", container.build_args())
            container.record_ti_acceptance(record=record)
            self.assertTrue(container.ti_accepted(record=record))
            args = container.build_args(ti=True)
            self.assertEqual(args["TOOLCHAIN_BASE"], "ti-sdk")
            self.assertIn("c6000|https://dr-download.ti.com/", args["TI_TOOLS"])
            self.assertRegex(args["PSDK_SHA256"], r"^[0-9a-f]{64}$")
            self.assertIn("ti-inputs=", " ".join(container.build_command(ti=True)))
            self.assertNotIn("ti-inputs=", " ".join(container.build_command(ti=False)))

    def test_acceptance_of_other_installers_does_not_count(self) -> None:
        with tempfile.TemporaryDirectory() as tmp, mock.patch.dict(os.environ, {"SDOS_TI_TERMS": ""}):
            record = Path(tmp) / "accepted.json"
            record.write_text(json.dumps({"installer_sha256": ["0" * 64]}))
            self.assertFalse(container.ti_accepted(record=record))

    def test_ti_and_open_images_have_different_tags(self) -> None:
        self.assertNotEqual(container.image_tag(ti=True), container.image_tag(ti=False))
        self.assertIn(":dev-ti-", container.image_tag(ti=True))


class FetchInputTests(unittest.TestCase):
    def test_cached_file_with_matching_hash_is_reused_without_network(self) -> None:
        payload = b"pinned payload"
        with tempfile.TemporaryDirectory() as tmp:
            manifest = Path(tmp) / "versions.json"
            manifest.write_text(json.dumps({"inputs": [{
                "id": "x", "kind": "file",
                "source": {"url": "https://invalid.example/x.bin", "file_name": "x.bin"},
                "integrity": {"sha256": __import__("hashlib").sha256(payload).hexdigest()},
            }]}))
            cache = Path(tmp) / "cache"
            cache.mkdir()
            (cache / "x.bin").write_bytes(payload)
            self.assertEqual(container.fetch_input("x", cache, manifest), cache / "x.bin")

    def test_unreachable_download_fails_with_a_clear_message(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            manifest = Path(tmp) / "versions.json"
            manifest.write_text(json.dumps({"inputs": [{
                "id": "x", "kind": "file",
                "source": {"url": "https://invalid.example/x.bin", "file_name": "x.bin"},
                "integrity": {"sha256": "0" * 64},
            }]}))
            denied = OSError("HTTP Error 403: Forbidden")
            with mock.patch("urllib.request.urlopen", side_effect=denied), \
                    contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(RuntimeError, "could not download"):
                    container.fetch_input("x", Path(tmp) / "cache", manifest)


if __name__ == "__main__":
    unittest.main()
