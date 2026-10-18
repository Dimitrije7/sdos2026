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
import time
from pathlib import Path
from typing import Any, Sequence

ROOT = Path(__file__).resolve().parents[1]
DOCKERFILE = ROOT / "toolchain" / "Dockerfile"
MANIFEST = ROOT / "manifest" / "versions.json"
BASE_INPUT = "ubuntu.jammy-20260924.1"
APT_INPUT = "ubuntu.jammy-apt-snapshot-20261004"
FRONTEND_INPUT = "dockerfile-frontend.1.27.1"
# Container name for each single-file tool input, in install order.
FILE_TOOLS = {
    "hadolint": "hadolint.v2.15.1",
    "syft": "syft.v1.54.0",
    "trivy": "trivy.v0.75.0",
    "node": "nodejs.v22.23.3",
    "actionlint": "actionlint.v1.7.12",
}
# TI compiler family for each TI input installed by the ti-toolchain stage.
TI_TOOLS = {
    "c6000": "ti.c6000-cgt-8.3.7",
    "c7000": "ti.c7000-cgt-5.0.0.lts",
}
TI_ACCEPTANCE = Path.home() / ".config" / "sdos" / "ti-terms-accepted.json"
IMAGE_NAME = "sdos-dev"
CURRENT_TAG = f"{IMAGE_NAME}:current"  # what .devcontainer opens; moved by each bootstrap


def _inputs(manifest: Path = MANIFEST) -> dict[str, dict[str, Any]]:
    data = json.loads(manifest.read_text(encoding="utf-8"))
    return {item["id"]: item for item in data["inputs"]}


def apt_cache_name(package: dict[str, Any]) -> str:
    """Name apt gives a downloaded package: the version keeps its epoch, escaped."""
    architecture = package["file_name"].rsplit("_", 1)[1]
    return f"{package['name']}_{package['version'].replace(':', '%3a')}_{architecture}"


def ti_terms(manifest: Path = MANIFEST) -> list[dict[str, str]]:
    """The TI inputs a user accepts, with the pages that present their terms."""
    inputs = _inputs(manifest)
    return [
        {
            "family": family,
            "id": item_id,
            "version": inputs[item_id]["version"],
            "sha256": inputs[item_id]["integrity"]["sha256"],
            "terms_page": inputs[item_id]["evidence"],
        }
        for family, item_id in TI_TOOLS.items()
    ]


def ti_accepted(manifest: Path = MANIFEST, record: Path | None = None) -> bool:
    """True when the user accepted exactly the TI installers the manifest pins."""
    if os.environ.get("SDOS_TI_TERMS") == "accept":
        return True
    record = record or TI_ACCEPTANCE
    try:
        accepted = json.loads(record.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return False
    pinned = {item["sha256"] for item in ti_terms(manifest)}
    return pinned <= set(accepted.get("installer_sha256", []))


def record_ti_acceptance(manifest: Path = MANIFEST, record: Path | None = None) -> Path:
    record = record or TI_ACCEPTANCE
    record.parent.mkdir(parents=True, exist_ok=True)
    terms = ti_terms(manifest)
    record.write_text(
        json.dumps(
            {
                "accepted_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "installer_sha256": [item["sha256"] for item in terms],
                "inputs": terms,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return record


def build_args(manifest: Path = MANIFEST, ti: bool = False) -> dict[str, str]:
    inputs = _inputs(manifest)
    base = inputs[BASE_INPUT]
    repository = base["source"]["ref"].split(":")[0]
    apt = inputs[APT_INPUT]["integrity"]
    packages = apt["packages"]
    args = {
        "BASE_IMAGE": f"{repository}@{base['integrity']['index_digest']}",
        "APT_SNAPSHOT": apt["snapshot"],
        "APT_INRELEASE_SHA256": " ".join(f"{s}={h}" for s, h in sorted(apt["inrelease_sha256"].items())),
        "APT_PACKAGES": " ".join(f"{p['name']}={p['version']}" for p in packages),
        "APT_DEB_SHA256": " ".join(f"{apt_cache_name(p)}={p['sha256']}" for p in packages),
        "FILE_TOOLS": " ".join(
            f"{name}|{inputs[i]['source']['url']}|{inputs[i]['integrity']['sha256']}"
            for name, i in FILE_TOOLS.items()
        ),
    }
    if ti:
        args["TOOLCHAIN_BASE"] = "ti-toolchain"
        args["TI_TOOLS"] = " ".join(
            f"{family}|{inputs[i]['source']['url']}|{inputs[i]['integrity']['sha256']}"
            for family, i in TI_TOOLS.items()
        )
    return args


def frontend_reference(manifest: Path = MANIFEST) -> str:
    item = _inputs(manifest)[FRONTEND_INPUT]
    return f"{item['source']['ref']}@{item['integrity']['index_digest']}"


def image_tag(
    target: str = "dev", manifest: Path = MANIFEST, dockerfile: Path = DOCKERFILE, ti: bool = False
) -> str:
    digest = hashlib.sha256()
    for path in sorted(dockerfile.parent.rglob("*")):  # the whole build context
        if path.is_file():
            digest.update(path.relative_to(dockerfile.parent).as_posix().encode())
            digest.update(path.read_bytes())
    digest.update(json.dumps(build_args(manifest, ti), sort_keys=True).encode())
    digest.update(target.encode())
    flavor = f"{target}-ti" if ti else target
    return f"{IMAGE_NAME}:{flavor}-{digest.hexdigest()[:12]}"


def build_command(target: str = "dev", ti: bool = False) -> list[str]:
    argv = ["docker", "build", "--target", target, "--tag", image_tag(target, ti=ti)]
    if target == "dev":
        argv += ["--tag", CURRENT_TAG]
    argv += ["--file", str(DOCKERFILE)]
    for key, value in build_args(ti=ti).items():
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
    command += ["--env", f"SDOS_IMAGE={tag}"]
    for variable in ("CI",):
        if variable in os.environ:
            command += ["--env", variable]
    return command + [tag, *argv]
