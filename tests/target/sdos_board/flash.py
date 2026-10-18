"""Write a verified image to an approved card, then read it back.

This is the package's only destructive operation. ``authorize`` repeats the
write-target check and requires the maintainer's confirmation to name the same
by-id. ``write_image`` opens the device with ``O_EXCL``, so the kernel refuses a
mounted or busy device, writes the decompressed image, flushes it, drops cached
pages, and reads the written range back to compare its SHA-256 with the
manifest value.
"""

from __future__ import annotations

import fcntl
import hashlib
import json
import lzma
import os
import stat
import sys
import time
from pathlib import Path
from typing import IO, Any

from . import guards
from .discovery import Discovery

CHUNK = 4 * 1024 * 1024
BLKFLSBUF = 0x1261  # linux/fs.h: flush a block device's buffer cache
MANIFEST = Path(__file__).resolve().parents[3] / "manifest" / "versions.json"


class FlashError(RuntimeError):
    """The write or its verification failed; the card content is not trusted."""


def manifest_image(input_id: str, manifest: Path = MANIFEST) -> dict[str, Any]:
    """Return the integrity fields of a pinned image input."""
    data = json.loads(manifest.read_text(encoding="utf-8"))
    for item in data["inputs"]:
        if item["id"] == input_id:
            integrity = item["integrity"]
            missing = {"sha256", "expanded_sha256", "expanded_size"} - integrity.keys()
            if missing:
                raise FlashError(f"{input_id} lacks {', '.join(sorted(missing))} in the manifest")
            return {"id": input_id, "file_name": item["source"]["file_name"], **integrity}
    raise FlashError(f"{input_id} is not an input in {manifest}")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(CHUNK):
            digest.update(chunk)
    return digest.hexdigest()


def authorize(discovery: Discovery, identity: str, confirm: str) -> dict[str, Any]:
    """Return the approval record, or refuse unless ``confirm`` names the target."""
    record = guards.check_write_target(discovery, identity)
    if confirm.strip() != record["by_id"]:
        raise guards.BoardAccessError(
            f"--confirm must repeat the approved identity {record['by_id']} exactly"
        )
    return record


def _open_source(path: Path) -> IO[bytes]:
    return lzma.open(path, "rb") if path.suffix == ".xz" else path.open("rb")


def _progress(label: str, done: int, total: int) -> None:
    print(f"{label} {done / 1e9:.2f} of {total / 1e9:.2f} GB", file=sys.stderr, flush=True)


def _drop_cache(fd: int) -> None:
    os.fsync(fd)
    if stat.S_ISBLK(os.fstat(fd).st_mode):
        fcntl.ioctl(fd, BLKFLSBUF, 0)
    os.posix_fadvise(fd, 0, 0, os.POSIX_FADV_DONTNEED)


def write_image(
    source: Path,
    target: str,
    expected_sha256: str,
    expected_size: int,
    report_every: int = 256 * 1024 * 1024,
) -> dict[str, Any]:
    """Write ``source`` to ``target`` and verify the read-back; return timings."""
    written = 0
    digest = hashlib.sha256()
    started = time.monotonic()
    fd = os.open(target, os.O_WRONLY | os.O_EXCL | getattr(os, "O_CLOEXEC", 0))
    try:
        if stat.S_ISBLK(os.fstat(fd).st_mode):
            capacity = os.lseek(fd, 0, os.SEEK_END)
            os.lseek(fd, 0, os.SEEK_SET)
            if capacity < expected_size:
                raise FlashError(f"{target} holds {capacity} bytes; the image needs {expected_size}")
        with _open_source(source) as stream:
            while chunk := stream.read(CHUNK):
                view = memoryview(chunk)
                while view:
                    view = view[os.write(fd, view):]
                digest.update(chunk)
                written += len(chunk)
                if written % report_every < len(chunk):
                    _progress("written", written, expected_size)
        _drop_cache(fd)
    finally:
        os.close(fd)
    write_seconds = time.monotonic() - started
    if written != expected_size or digest.hexdigest() != expected_sha256:
        raise FlashError(
            f"the decompressed image is {written} bytes with SHA-256 {digest.hexdigest()}; "
            f"the manifest expects {expected_size} bytes with {expected_sha256}"
        )

    started = time.monotonic()
    readback = hashlib.sha256()
    remaining = expected_size
    fd = os.open(target, os.O_RDONLY | getattr(os, "O_CLOEXEC", 0))
    try:
        os.posix_fadvise(fd, 0, 0, os.POSIX_FADV_DONTNEED)
        while remaining:
            chunk = os.read(fd, min(CHUNK, remaining))
            if not chunk:
                break
            readback.update(chunk)
            remaining -= len(chunk)
            if (expected_size - remaining) % report_every < len(chunk):
                _progress("verified", expected_size - remaining, expected_size)
    finally:
        os.close(fd)
    if remaining or readback.hexdigest() != expected_sha256:
        raise FlashError(
            f"read-back of {expected_size - remaining} bytes has SHA-256 "
            f"{readback.hexdigest()}; expected {expected_sha256}"
        )
    return {
        "bytes_written": written,
        "written_sha256": digest.hexdigest(),
        "readback_sha256": readback.hexdigest(),
        "write_seconds": round(write_seconds, 1),
        "readback_seconds": round(time.monotonic() - started, 1),
    }
