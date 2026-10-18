#!/usr/bin/env python3
"""Rebuild TI's IPC echo firmware from the pinned PDK (Task 020).

The PDK has its own make system, so these builds are not CMake presets. The
facade exposes them as presets named ``ti-reference-<core>-<debug|release>``
(group ``ti-reference``) and calls ``build`` for each one inside the container.

Each build writes to ``build/<preset>/``: the PDK object tree, the firmware
ELF and map, and ``build-manifest.json`` with input and output hashes, the
firmware fit check, and a comparison with TI's prebuilt blob when it is cached.
TI's boot-loader image step needs ``mono`` and is not used by Linux
remoteproc, so it is switched off; its ``aarch64-none-elf-objcopy`` check is
satisfied with the container's AArch64 binutils.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
from pathlib import Path
from typing import Any, Callable, Sequence

try:
    from ci import container, firmware_elf
except ImportError:  # run as a script: ci/ is on sys.path
    import container  # type: ignore[no-redef]
    import firmware_elf  # type: ignore[no-redef]

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "ti-reference-"
EXAMPLE = "ipc_echo_test_freertos"  # examples/linux: built with -DA72_LINUX_OS, as TI's blobs are
PROFILES = ("debug", "release")
EXTENSION = {"c6000": "xe66", "c7000": "xe71"}

Run = Callable[[Sequence[str]], int]


def cores() -> dict[str, dict[str, Any]]:
    return {c["name"]: c for c in json.loads(firmware_elf.CORES.read_text(encoding="utf-8"))["cores"]}


def presets() -> list[str]:
    return [f"{PREFIX}{name}-{profile}" for name in cores() for profile in PROFILES]


def parse(preset: str) -> tuple[dict[str, Any], str]:
    name, profile = preset[len(PREFIX):].rsplit("-", 1)
    return cores()[name], profile


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8").strip() if path.exists() else ""


def paths() -> dict[str, Path]:
    psdk = Path(os.environ.get("SDOS_TI_PSDK_ROOT", "/nonexistent")).resolve()
    pdk = next(psdk.glob("pdk_jacinto_*/packages"), psdk / "pdk-missing")
    return {
        "psdk": psdk,
        "pdk": pdk,
        "c6000": Path(os.environ.get("SDOS_TI_C6000_ROOT", "/nonexistent")).resolve(),
        "c7000": Path(os.environ.get("SDOS_TI_C7000_ROOT", "/nonexistent")).resolve(),
    }


def make_command(core: dict[str, Any], profile: str, out: Path, where: dict[str, Path]) -> list[str]:
    return [
        "env",
        f"SDK_INSTALL_PATH={where['psdk']}",
        f"PDK_INSTALL_PATH={where['pdk']}",
        f"TOOLS_INSTALL_PATH={where['psdk'].parent}",
        f"C6X_GEN_INSTALL_PATH={where['c6000']}",
        f"C7X_GEN_INSTALL_PATH={where['c7000']}",
        f"TOOLCHAIN_PATH_GCC_ARCH64={out / 'shim'}",
        "make", "-s", "-C", str(where["pdk"] / "ti" / "build"), EXAMPLE,
        "BOARD=j721e_evm", f"CORE={core['ti_core']}", f"BUILD_PROFILE={profile}",
        f"DEST_ROOT={out / 'pdk'}", f"{EXAMPLE}_SBL_APPIMAGEGEN=no",
    ]


def compare(rebuilt: firmware_elf.Report, prebuilt: firmware_elf.Report) -> dict[str, Any]:
    """Compare the parts of the contract Linux depends on, not the bytes."""
    def layout(report: firmware_elf.Report) -> list[tuple[int, str]]:
        return [(s.address, s.flags) for s in report.segments]

    same_resources = rebuilt.resources == prebuilt.resources
    same_layout = layout(rebuilt) == layout(prebuilt)
    return {
        "resource_table_equal": same_resources,
        "segment_addresses_equal": same_layout,
        "segment_differences": sorted(set(layout(rebuilt)) ^ set(layout(prebuilt)))[:20],
    }


def build(preset: str, run: Run, dry_run: bool = False) -> int:
    core, profile = parse(preset)
    out = ROOT / "build" / preset
    where = paths()
    shim = out / "shim" / "bin" / "aarch64-none-elf-objcopy"
    if not dry_run:
        shim.parent.mkdir(parents=True, exist_ok=True)
        if not shim.exists():
            shim.symlink_to(shutil.which("aarch64-linux-gnu-objcopy") or "/usr/bin/aarch64-linux-gnu-objcopy")
    code = run(make_command(core, profile, out, where))
    if code or dry_run:
        return code
    stem = f"{EXAMPLE}_{core['ti_core']}_{profile}"
    binary = out / "pdk" / EXAMPLE / "bin" / "j721e_evm"
    elf = binary / f"{stem}.{EXTENSION[core['family']]}"
    report = firmware_elf.inspect(elf, core)
    manifest: dict[str, Any] = {
        "schema": "sdos-ti-reference/1",
        "preset": preset,
        "core": core["name"],
        "ti_core": core["ti_core"],
        "profile": profile,
        "example": EXAMPLE,
        "inputs": {
            "psdk_archive_sha256": _read(where["psdk"].parent.parent / "psdk.archive.sha256"),
            "compiler_installer_sha256": _read(Path(f"{where[core['family']].parent.parent}.installer.sha256"))
            or _read(Path(f"/opt/ti/{core['family']}.installer.sha256")),
            "pdk": where["pdk"].parent.name,
            "container_image": os.environ.get("SDOS_IMAGE", ""),
        },
        "command": make_command(core, profile, Path("build") / preset, where),
        "outputs": {
            path.name: {"sha256": _sha256(path), "size": path.stat().st_size}
            for path in sorted(binary.glob(f"{stem}*"))
            if path.suffix in (f".{EXTENSION[core['family']]}", ".map")
        },
        "fit": {"ok": report.ok, "problems": report.problems, "resources": report.resources},
    }
    prebuilt_id = core["ti_echo_input"]
    item = container._inputs()[prebuilt_id]
    cached = container.INPUT_CACHE / item["source"]["file_name"]
    if profile == "release" and cached.exists() and _sha256(cached) == item["integrity"]["sha256"]:
        manifest["prebuilt_comparison"] = {
            "input": prebuilt_id,
            "note": "TI built the prebuilt blob from REL.PSDK.11.02.01.05; this tree is PDK 11.02.01.08",
            **compare(firmware_elf.inspect(elf.with_name(f"{stem}_strip.{elf.suffix[1:]}"), core),
                      firmware_elf.inspect(cached, core)),
        }
    (out / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"{preset}: {elf.name} {manifest['outputs'][elf.name]['sha256'][:16]} "
          f"fit={'ok' if report.ok else '; '.join(report.problems)}")
    return 0 if report.ok else 1
