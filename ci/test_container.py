#!/usr/bin/env python3
"""Tests that the container is built only from manifest pins and runs commands safely."""

from __future__ import annotations

import contextlib
import io
import json
import os
import re
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


if __name__ == "__main__":
    unittest.main()
