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
    memory: dict[str, dict[str, str]]

    def regions(self) -> list[tuple[str, int, int]]:
        """(name, base, end) of each carve-out the core may load into or share."""
        return [(name, int(r["base"], 16), int(r["base"], 16) + int(r["size"], 16)) for name, r in self.memory.items()]


def load(manifest: Path = MANIFEST) -> dict[str, Core]:
    data = json.loads(manifest.read_text(encoding="utf-8"))
    return {item["name"]: Core(**item) for item in data["cores"]}


CORES = load()
