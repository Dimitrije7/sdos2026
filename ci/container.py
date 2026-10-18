"""Pinned container image for the command facade.

The image tag is derived from the build context in toolchain/ and the build
arguments, which all come from manifest/versions.json, so any pin change produces a new tag and
``make bootstrap`` rebuilds. Commands then run inside the image with the
repository mounted at /work.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import Any, Sequence

ROOT = Path(__file__).resolve().parents[1]
DOCKERFILE = ROOT / "toolchain" / "Dockerfile"
MANIFEST = ROOT / "manifest" / "versions.json"
BASE_INPUT = "ubuntu.jammy-20260924.1"
APT_INPUT = "ubuntu.jammy-apt-snapshot-20261004"
FRONTEND_INPUT = "dockerfile-frontend.1.27.1"
IMAGE_NAME = "sdos-dev"
CURRENT_TAG = f"{IMAGE_NAME}:current"  # what .devcontainer opens; moved by each bootstrap


def _inputs(manifest: Path = MANIFEST) -> dict[str, dict[str, Any]]:
    data = json.loads(manifest.read_text(encoding="utf-8"))
    return {item["id"]: item for item in data["inputs"]}


def apt_cache_name(package: dict[str, Any]) -> str:
    """Name apt gives a downloaded package: the version keeps its epoch, escaped."""
    architecture = package["file_name"].rsplit("_", 1)[1]
    return f"{package['name']}_{package['version'].replace(':', '%3a')}_{architecture}"


def build_args(manifest: Path = MANIFEST) -> dict[str, str]:
    inputs = _inputs(manifest)
    base = inputs[BASE_INPUT]
    repository = base["source"]["ref"].split(":")[0]
    apt = inputs[APT_INPUT]["integrity"]
    packages = apt["packages"]
    return {
        "BASE_IMAGE": f"{repository}@{base['integrity']['index_digest']}",
        "APT_SNAPSHOT": apt["snapshot"],
        "APT_INRELEASE_SHA256": " ".join(f"{s}={h}" for s, h in sorted(apt["inrelease_sha256"].items())),
        "APT_PACKAGES": " ".join(f"{p['name']}={p['version']}" for p in packages),
        "APT_DEB_SHA256": " ".join(f"{apt_cache_name(p)}={p['sha256']}" for p in packages),
    }


def frontend_reference(manifest: Path = MANIFEST) -> str:
    item = _inputs(manifest)[FRONTEND_INPUT]
    return f"{item['source']['ref']}@{item['integrity']['index_digest']}"


def image_tag(target: str = "dev", manifest: Path = MANIFEST, dockerfile: Path = DOCKERFILE) -> str:
    digest = hashlib.sha256()
    for path in sorted(dockerfile.parent.rglob("*")):  # the whole build context
        if path.is_file():
            digest.update(path.relative_to(dockerfile.parent).as_posix().encode())
            digest.update(path.read_bytes())
    digest.update(json.dumps(build_args(manifest), sort_keys=True).encode())
    digest.update(target.encode())
    return f"{IMAGE_NAME}:{target}-{digest.hexdigest()[:12]}"


def build_command(target: str = "dev") -> list[str]:
    argv = ["docker", "build", "--target", target, "--tag", image_tag(target)]
    if target == "dev":
        argv += ["--tag", CURRENT_TAG]
    argv += ["--file", str(DOCKERFILE)]
    for key, value in build_args().items():
        argv += ["--build-arg", f"{key}={value}"]
    return argv + [str(DOCKERFILE.parent)]


def docker_available() -> bool:
    if not shutil.which("docker"):
        return False
    result = subprocess.run(["docker", "info"], capture_output=True, check=False)
    return result.returncode == 0


def image_present(tag: str) -> bool:
    result = subprocess.run(["docker", "image", "inspect", tag], capture_output=True, check=False)
    return result.returncode == 0


def _rootless() -> bool:
    result = subprocess.run(
        ["docker", "info", "--format", "{{.SecurityOptions}}"], capture_output=True, text=True, check=False
    )
    return "rootless" in result.stdout


def run_command(tag: str, argv: Sequence[str], rootless: bool | None = None) -> list[str]:
    """Run ``argv`` in the image with the repository at /work.

    Rootless Docker maps container root to the invoking user, so files created
    in /work belong to that user. With rootful Docker the container runs as the
    invoking user's UID and GID for the same result.
    """
    command = ["docker", "run", "--rm", "--init", "--volume", f"{ROOT}:/work", "--workdir", "/work"]
    if os.isatty(0) and os.isatty(1):
        command.append("--tty")
    if not (_rootless() if rootless is None else rootless):
        command += ["--user", f"{os.getuid()}:{os.getgid()}", "--env", "HOME=/tmp"]
    for variable in ("SDOS_TI_C6000_ROOT", "SDOS_TI_C7000_ROOT", "CI"):
        if variable in os.environ:
            command += ["--env", variable]
    return command + [tag, *argv]
