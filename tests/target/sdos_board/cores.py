"""Logical DSP identities from manifest/dsp-cores.json, the single source."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

MANIFEST = Path(__file__).resolve().parents[3] / "manifest" / "dsp-cores.json"


@dataclass(frozen=True)
class Core:
    name: str
    label: str
    family: str
    device: str
    ti_core: str
    kernel_firmware: str
    ti_echo_input: str
    regions: tuple[dict[str, object], ...]
    boot_alignment: str
    boot_alignment_reason: str

    def region(self, name: str) -> dict[str, object]:
        return next(r for r in self.regions if r["name"] == name)


def _core(item: dict[str, object]) -> Core:
    return Core(**{**item, "regions": tuple(item["regions"])})  # type: ignore[arg-type]


def load(manifest: Path = MANIFEST) -> dict[str, Core]:
    data = json.loads(manifest.read_text(encoding="utf-8"))
    return {item["name"]: _core(item) for item in data["cores"]}


CORES = load()
