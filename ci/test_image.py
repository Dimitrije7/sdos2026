#!/usr/bin/env python3
"""Image builder tests on a 16 MiB synthetic image with an MBR and an ext4 root."""

from __future__ import annotations

import hashlib
import json
import lzma
import shutil
import struct
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

from image import build as image

TOOLS = all(shutil.which(tool) for tool in ("debugfs", "mke2fs", "e2fsck", "xz"))
MiB = 1 << 20


def make_base(directory: Path) -> tuple[Path, dict]:
    raw = directory / "base.img"
    with raw.open("wb") as handle:
        handle.truncate(16 * MiB)
        mbr = bytearray(512)
        mbr[446:462] = bytes([0x80, 0, 0, 0, 0x0C, 0, 0, 0]) + struct.pack("<II", 2048, 2048)  # 1 MiB "boot"
        mbr[478:494] = bytes([0, 0, 0, 0, 0x83, 0, 0, 0]) + struct.pack("<II", 4096, 28672)  # 14 MiB root
        mbr[510:512] = b"\x55\xaa"
        handle.write(mbr)
        handle.seek(2048 * 512)
        handle.write(b"BOOT" * 1024)
    subprocess.run(["mke2fs", "-q", "-F", "-t", "ext4", "-U", "8c4b8b6a-3e0e-4f3e-9d7e-0a6a1b2c3d4e",
                    "-E", f"offset={4096 * 512},hash_seed=1d4c2b2e-6a9f-4c1e-8f0e-0b1c2d3e4f50",
                    str(raw), "14M"], check=True, capture_output=True)
    fs = image.Filesystem(raw, 4096 * 512)
    fs.run(["set_current_time @1700000000", "mkdir /etc", "mkdir /usr", "mkdir /usr/lib", "mkdir /usr/lib/firmware",
            "mkdir /usr/local", "mkdir /usr/local/bin"], write=True)
    expanded = hashlib.sha256(raw.read_bytes()).hexdigest()
    compressed = directory / "base.img.xz"
    compressed.write_bytes(lzma.compress(raw.read_bytes()))
    raw.unlink()
    info = {"id": "test.base", "version": "test-base",
            "integrity": {"sha256": hashlib.sha256(compressed.read_bytes()).hexdigest(), "expanded_sha256": expanded}}
    return compressed, info


def recipe(directory: Path, entries: list[dict]) -> Path:
    path = directory / "recipe.json"
    path.write_text(json.dumps({
        "schema": "sdos-image-recipe/1", "name": "test-image", "version": "0.0.1", "base": "test.base",
        "root_partition": 3, "kernel": "beagleboard-package", "source_date_epoch": 1792281600, "entries": entries,
    }), encoding="utf-8")
    return path


ENTRIES = [
    {"type": "dir", "path": "/usr/share", "mode": "0755"},
    {"type": "dir", "path": "/usr/share/sdos", "mode": "0755"},
    {"type": "file", "path": "/etc/sdos-release", "source": "generated:release", "mode": "0644"},
    {"type": "file", "path": "/usr/local/bin/dspctl", "source": "build:fake/dspctl", "mode": "0755"},
    {"type": "file", "path": "/usr/share/sdos/README", "source": "repo:image/files/README", "mode": "0644"},
    {"type": "file", "path": "/usr/lib/firmware/absent.xe66", "source": "build:missing.xe66", "optional": True},
]


@unittest.skipUnless(TOOLS, "needs debugfs, mke2fs, e2fsck, and xz (run in the container)")
class ImageBuilderTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp())
        self.base, self.info = make_base(self.tmp)
        self.build_dir = self.tmp / "build"
        (self.build_dir / "fake").mkdir(parents=True)
        (self.build_dir / "fake" / "dspctl").write_bytes(b"\x7fELF fake dspctl")

    def tearDown(self) -> None:
        shutil.rmtree(self.tmp)

    def run_build(self, name: str, entries: list[dict] = ENTRIES, keep_raw: bool = True) -> dict:
        return image.build(recipe(self.tmp, entries), self.base, self.tmp / name, self.build_dir,
                           base_info=self.info, keep_raw=keep_raw)

    def test_recipe_is_applied_with_root_owner_modes_and_content(self) -> None:
        manifest = self.run_build("out")
        raw = self.tmp / "out" / "test-image.img"
        fs = image.Filesystem(raw, manifest["root_partition"]["offset"])
        self.assertEqual(fs.read("/usr/local/bin/dspctl"), b"\x7fELF fake dspctl")
        self.assertEqual(fs.stat("/usr/local/bin/dspctl")["mode"], "0755")
        self.assertIn(b"SDOS_IMAGE_VERSION=0.0.1", fs.read("/etc/sdos-release"))
        self.assertEqual(fs.stat("/usr/share/sdos")["type"], "directory")
        self.assertIsNone(fs.stat("/usr/lib/firmware/absent.xe66"))  # optional and not built
        self.assertEqual([e["path"] for e in manifest["entries"]][-1], "/usr/share/sdos/README")
        with (self.tmp / "out" / "test-image.img").open("rb") as handle:
            handle.seek(2048 * 512)
            self.assertEqual(handle.read(4), b"BOOT")

    def test_two_builds_are_identical(self) -> None:
        first = self.run_build("one", keep_raw=False)
        time.sleep(1.1)  # a different wall clock must not leak into the image
        second = self.run_build("two", keep_raw=False)
        self.assertEqual(first["outputs"]["raw_sha256"], second["outputs"]["raw_sha256"])
        self.assertEqual(first["outputs"]["xz_sha256"], second["outputs"]["xz_sha256"])

    def test_rebuilding_over_existing_files_replaces_them(self) -> None:
        self.run_build("out")
        (self.build_dir / "fake" / "dspctl").write_bytes(b"\x7fELF newer")
        manifest = self.run_build("out")
        fs = image.Filesystem(self.tmp / "out" / "test-image.img", manifest["root_partition"]["offset"])
        self.assertEqual(fs.read("/usr/local/bin/dspctl"), b"\x7fELF newer")

    def test_a_base_with_another_hash_is_refused(self) -> None:
        self.info["integrity"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(image.BuildError, "does not match the pinned SHA-256"):
            self.run_build("out")

    def test_missing_sources_and_bad_paths_are_refused(self) -> None:
        with self.assertRaisesRegex(image.BuildError, "build it first"):
            self.run_build("out", [{"type": "file", "path": "/x", "source": "build:nope"}])
        with self.assertRaisesRegex(image.BuildError, "absolute and normalized"):
            self.run_build("out", [{"type": "dir", "path": "/usr/../etc"}])
        with self.assertRaisesRegex(image.BuildError, "not a regular file"):
            self.run_build("out", [{"type": "file", "path": "/etc", "source": "generated:release"}])

    def test_unimplemented_kernel_source_is_explicit(self) -> None:
        path = recipe(self.tmp, ENTRIES)
        data = json.loads(path.read_text())
        data["kernel"] = "source"
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(image.BuildError, "from-source kernel"):
            image.build(path, self.base, self.tmp / "out", self.build_dir, base_info=self.info)


class RecipeTests(unittest.TestCase):
    def test_course_recipe_names_a_pinned_base_and_valid_entries(self) -> None:
        data = json.loads(image.RECIPE.read_text())
        inputs = {item["id"] for item in json.loads(image.MANIFEST.read_text())["inputs"]}
        self.assertIn(data["base"], inputs)
        for entry in data["entries"]:
            image._check_path(entry["path"])
            if entry["type"] == "file" and entry["source"].startswith("repo:"):
                self.assertTrue((image.ROOT / entry["source"][5:]).is_file(), entry["source"])


if __name__ == "__main__":
    unittest.main()
