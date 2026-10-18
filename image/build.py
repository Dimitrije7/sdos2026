#!/usr/bin/env python3
"""Build the course image from the pinned upstream image, without privileges.

The builder never mounts anything. It decompresses the verified upstream
image, edits the root filesystem in place with ``debugfs`` from e2fsprogs, and
recompresses it. Every change comes from ``image/recipe.json``. Timestamps
come from the recipe's ``source_date_epoch``, so two builds from the same
inputs produce the same bytes.

After applying the recipe it checks the result: each entry's type, mode,
owner, and content hash; ``e2fsck -fn``; and that the partition table and
every other partition are unchanged. The image manifest records all of it.

Usage: ``python3 image/build.py --base <upstream.img.xz> --out build/image``
(``make image`` runs it in the container after building the inputs).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import lzma
import os
import posixpath
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ROOT / "image" / "recipe.json"
MANIFEST = ROOT / "manifest" / "versions.json"
SECTOR = 512
CHUNK = 1 << 20


class BuildError(RuntimeError):
    pass


@dataclass(frozen=True)
class Partition:
    number: int
    kind: int
    start: int  # bytes
    size: int  # bytes


def partitions(image: Path) -> list[Partition]:
    """Primary partitions from the MBR."""
    with image.open("rb") as handle:
        mbr = handle.read(SECTOR)
    if mbr[510:512] != b"\x55\xaa":
        raise BuildError(f"{image.name} has no MBR partition table")
    result = []
    for index in range(4):
        entry = mbr[446 + 16 * index : 462 + 16 * index]
        kind = entry[4]
        first, count = struct.unpack("<II", entry[8:16])
        if kind and count:
            result.append(Partition(index + 1, kind, first * SECTOR, count * SECTOR))
    return result


def sha256_file(path: Path, start: int = 0, length: int | None = None) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        handle.seek(start)
        remaining = length
        while remaining is None or remaining > 0:
            chunk = handle.read(CHUNK if remaining is None else min(CHUNK, remaining))
            if not chunk:
                break
            digest.update(chunk)
            if remaining is not None:
                remaining -= len(chunk)
    return digest.hexdigest()


class Filesystem:
    """An ext4 filesystem at a byte offset inside an image file, via debugfs."""

    def __init__(self, image: Path, offset: int, debugfs: str = "debugfs") -> None:
        self.device = f"{image}?offset={offset}"
        self.debugfs = debugfs

    def run(self, commands: list[str], write: bool = False) -> str:
        with tempfile.NamedTemporaryFile("w", suffix=".debugfs", delete=False) as script:
            script.write("\n".join(commands) + "\n")
        try:
            argv = [self.debugfs, *(["-w"] if write else []), "-f", script.name, self.device]
            result = subprocess.run(argv, capture_output=True, text=True, check=False)
        finally:
            os.unlink(script.name)
        if result.returncode != 0:
            raise BuildError(f"debugfs failed: {result.stderr.strip()}")
        return result.stdout + result.stderr

    def stat(self, path: str) -> dict[str, Any] | None:
        out = self.run([f'stat "{path}"'])
        if "not found by ext2_lookup" in out or "File not found" in out:
            return None
        info: dict[str, Any] = {}
        for key in ("Type", "Mode", "User", "Group", "Size"):
            match = re.search(rf"\b{key}:\s*(\S+)", out)
            if match:
                info[key.lower()] = match.group(1)
        return info

    def read(self, path: str) -> bytes:
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "out"
            self.run([f'dump "{path}" "{target}"'])
            return target.read_bytes() if target.exists() else b""


def _check_path(path: str) -> str:
    if not path.startswith("/") or posixpath.normpath(path) != path or path == "/":
        raise BuildError(f"recipe path {path!r} must be absolute and normalized")
    return path


def resolve(recipe: dict[str, Any], build_dir: Path, generated: dict[str, bytes]) -> list[dict[str, Any]]:
    """Turn recipe entries into concrete operations with content and hashes."""
    operations = []
    for entry in recipe["entries"]:
        path = _check_path(entry["path"])
        mode = int(entry.get("mode", "0644"), 8)
        if entry["type"] == "dir":
            operations.append({"type": "dir", "path": path, "mode": mode})
            continue
        source = entry["source"]
        scheme, _, location = source.partition(":")
        if scheme == "build":
            file = build_dir / location
        elif scheme == "repo":
            file = ROOT / location
        elif scheme == "generated":
            file = None
        else:
            raise BuildError(f"{path}: unknown source {source!r}")
        if file is not None:
            if not file.is_file():
                if entry.get("optional"):
                    continue
                raise BuildError(f"{path}: source {source} does not exist; build it first")
            content = file.read_bytes()
        elif location in generated:
            content = generated[location]
        else:
            raise BuildError(f"{path}: no generated content named {location!r}")
        operations.append({
            "type": "file", "path": path, "mode": mode, "source": source, "content": content,
            "sha256": hashlib.sha256(content).hexdigest(), "size": len(content),
        })
    return operations


def apply(fs: Filesystem, operations: list[dict[str, Any]], epoch: int) -> None:
    stamp = f"@{epoch}"
    commands = [f"set_current_time {stamp}"]
    touched_dirs: set[str] = set()
    with tempfile.TemporaryDirectory() as tmp:
        for index, op in enumerate(operations):
            path = op["path"]
            parent = posixpath.dirname(path)
            existing = fs.stat(path)
            if op["type"] == "dir":
                if existing is None:
                    commands.append(f'mkdir "{path}"')
                elif existing.get("type") != "directory":
                    raise BuildError(f"{path} exists in the base image and is not a directory")
                mode = 0o040000 | op["mode"]
            else:
                if existing is not None:
                    if existing.get("type") != "regular":
                        raise BuildError(f"{path} exists in the base image and is not a regular file")
                    commands.append(f'rm "{path}"')
                local = Path(tmp) / f"{index}"
                local.write_bytes(op["content"])
                commands.append(f'write "{local}" "{path}"')
                mode = 0o100000 | op["mode"]
            commands += [f'sif "{path}" mode 0{mode:o}', f'sif "{path}" uid 0', f'sif "{path}" gid 0']
            commands += [f'sif "{path}" {field} {stamp}' for field in ("atime", "mtime", "ctime", "crtime")]
            touched_dirs.add(parent)
        for directory in sorted(touched_dirs):
            commands += [f'sif "{directory}" {field} {stamp}' for field in ("mtime", "ctime")]
        commands.append(f"ssv wtime {stamp}")
        fs.run(commands, write=True)


def verify(fs: Filesystem, operations: list[dict[str, Any]]) -> list[str]:
    problems = []
    for op in operations:
        info = fs.stat(op["path"])
        wanted_type = "directory" if op["type"] == "dir" else "regular"
        if info is None or info.get("type") != wanted_type:
            problems.append(f"{op['path']}: expected a {wanted_type}, found {info}")
            continue
        if int(info.get("mode", "0"), 8) & 0o7777 != op["mode"]:
            problems.append(f"{op['path']}: mode {info.get('mode')} is not {op['mode']:04o}")
        if info.get("user") != "0" or info.get("group") != "0":
            problems.append(f"{op['path']}: owner {info.get('user')}:{info.get('group')} is not root")
        if op["type"] == "file" and hashlib.sha256(fs.read(op["path"])).hexdigest() != op["sha256"]:
            problems.append(f"{op['path']}: content differs from its source")
    return problems


def release_file(recipe: dict[str, Any], base: dict[str, Any], commit: str) -> bytes:
    lines = [
        f"SDOS_IMAGE_VERSION={recipe['version']}",
        f"SDOS_BASE_IMAGE={base['version']}",
        f"SDOS_BASE_SHA256={base['integrity']['sha256']}",
        f"SDOS_COMMIT={commit}",
        f"SDOS_SOURCE_DATE_EPOCH={recipe['source_date_epoch']}",
        f"SDOS_KERNEL_SOURCE={recipe['kernel']}",
    ]
    return ("\n".join(lines) + "\n").encode()


def git_commit() -> str:
    try:
        out = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True, check=False)
        dirty = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain"], capture_output=True, text=True,
                               check=False)
    except OSError:
        return "unknown"
    if out.returncode != 0:
        return "unknown"
    return out.stdout.strip() + ("-dirty" if dirty.stdout.strip() else "")


def decompress(source: Path, target: Path, expected_sha256: str | None) -> None:
    """Decompress sparsely and verify the expanded hash while writing."""
    digest = hashlib.sha256()
    zero = bytes(CHUNK)
    with lzma.open(source) as reader, target.open("wb") as writer:
        while chunk := reader.read(CHUNK):
            digest.update(chunk)
            if chunk == zero[: len(chunk)]:
                writer.seek(len(chunk), os.SEEK_CUR)
            else:
                writer.write(chunk)
        writer.truncate()
    if expected_sha256 and digest.hexdigest() != expected_sha256:
        raise BuildError(f"expanded base image hash {digest.hexdigest()} is not the pinned {expected_sha256}")


def compress(source: Path, target: Path) -> None:
    # A fixed block size makes xz's multi-threaded output identical for any
    # thread count of 2 or more; single-threaded mode would differ.
    threads = str(max(2, os.cpu_count() or 2))
    with target.open("wb") as out:
        subprocess.run(["xz", "-6", "-T", threads, "--block-size=64MiB", "-c", str(source)], stdout=out, check=True)


def build(recipe_path: Path, base_xz: Path, out: Path, build_dir: Path, base_id: str | None = None,
          base_info: dict[str, Any] | None = None, keep_raw: bool = False) -> dict[str, Any]:
    recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
    if recipe.get("kernel") != "beagleboard-package":
        raise BuildError(f"kernel source {recipe.get('kernel')!r} is not implemented yet; "
                         "the from-source kernel variant is the next Task 035 step")
    if base_info is None:
        inputs = {item["id"]: item for item in json.loads(MANIFEST.read_text(encoding="utf-8"))["inputs"]}
        base_info = inputs[base_id or recipe["base"]]
    if sha256_file(base_xz) != base_info["integrity"]["sha256"]:
        raise BuildError(f"{base_xz.name} does not match the pinned SHA-256 of {base_info['id']}")
    out.mkdir(parents=True, exist_ok=True)
    raw = out / f"{recipe['name']}.img"
    decompress(base_xz, raw, base_info["integrity"].get("expanded_sha256"))

    parts = partitions(raw)
    root = next((p for p in parts if p.number == recipe["root_partition"]), None)
    if root is None or root.kind != 0x83:
        raise BuildError(f"partition {recipe['root_partition']} is not a Linux filesystem: {parts}")
    others = {p.number: sha256_file(raw, p.start, p.size) for p in parts if p != root}
    table = sha256_file(raw, 0, SECTOR)

    generated = {"release": release_file(recipe, base_info, git_commit())}
    operations = resolve(recipe, build_dir, generated)
    fs = Filesystem(raw, root.start)
    apply(fs, operations, int(recipe["source_date_epoch"]))

    problems = verify(fs, operations)
    check = subprocess.run(["e2fsck", "-fn", f"{raw}?offset={root.start}"], capture_output=True, text=True,
                           check=False)
    if check.returncode != 0:
        problems.append(f"e2fsck found problems: {check.stdout[-400:]}")
    if sha256_file(raw, 0, SECTOR) != table:
        problems.append("the partition table changed")
    for p in parts:
        if p != root and sha256_file(raw, p.start, p.size) != others[p.number]:
            problems.append(f"partition {p.number} changed")
    if problems:
        raise BuildError("image checks failed:\n  " + "\n  ".join(problems))

    image_xz = out / f"{recipe['name']}.img.xz"
    compress(raw, image_xz)
    manifest = {
        "schema": "sdos-image-manifest/1",
        "name": recipe["name"],
        "version": recipe["version"],
        "recipe_sha256": sha256_file(recipe_path),
        "base": {"id": base_info["id"], "sha256": base_info["integrity"]["sha256"]},
        "source_date_epoch": recipe["source_date_epoch"],
        "kernel": recipe["kernel"],
        "root_partition": {"number": root.number, "offset": root.start, "size": root.size},
        "entries": [{k: v for k, v in op.items() if k != "content"} | {"mode": f"{op['mode']:04o}"}
                    for op in operations],
        "checks": ["entry type, mode, owner, content", "e2fsck -fn", "partition table unchanged",
                   "other partitions unchanged"],
        "outputs": {
            "raw_sha256": sha256_file(raw),
            "raw_size": raw.stat().st_size,
            "xz_sha256": sha256_file(image_xz),
            "xz_size": image_xz.stat().st_size,
        },
        "publication": "local only: the image carries TI-derived firmware and the upstream image, "
                       "whose redistribution waits for LIC-001 and LIC-003",
    }
    (out / f"{recipe['name']}.manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (out / f"{recipe['name']}.img.xz.sha256").write_text(
        f"{manifest['outputs']['xz_sha256']}  {image_xz.name}\n", encoding="utf-8")
    if not keep_raw:
        raw.unlink()
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--recipe", type=Path, default=RECIPE)
    parser.add_argument("--base", type=Path, required=True, help="pinned upstream .img.xz")
    parser.add_argument("--out", type=Path, default=ROOT / "build" / "image")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build", help="where build: sources live")
    parser.add_argument("--keep-raw", action="store_true", help="keep the uncompressed image for inspection")
    args = parser.parse_args(argv)
    if not shutil.which("debugfs") or not shutil.which("e2fsck"):
        print("image: debugfs and e2fsck are needed; run `make image` in the container", file=sys.stderr)
        return 1
    try:
        manifest = build(args.recipe, args.base, args.out, args.build_dir, keep_raw=args.keep_raw)
    except BuildError as exc:
        print(f"image: {exc}", file=sys.stderr)
        return 1
    print(f"image: {args.out / (manifest['name'] + '.img.xz')} {manifest['outputs']['xz_sha256']}")
    for entry in manifest["entries"]:
        print(f"  {entry['mode']} {entry['path']}" + (f"  {entry['sha256'][:16]}" if "sha256" in entry else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
