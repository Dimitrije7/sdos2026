#!/usr/bin/env python3
"""Portable tests for hardware BOM and inventory safety invariants."""

from __future__ import annotations

import copy
import json
import unittest
from pathlib import Path
from typing import Any

from ci import validate_hardware as subject


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]


def load_json(relative_path: str) -> dict[str, Any]:
    with (REPOSITORY_ROOT / relative_path).open(encoding="utf-8") as handle:
        return json.load(handle)


class HardwareValidationTests(unittest.TestCase):
    """Use synthetic records; these tests make no physical-hardware claims."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.bom = load_json("manifest/hardware-bom.json")
        cls.bom_schema = load_json("manifest/schemas/hardware-bom.schema.json")
        cls.inventory_schema = load_json(
            "manifest/schemas/hardware-inventory.schema.json"
        )
        subject.validate_against_schema(
            cls.bom, cls.bom_schema, Path("manifest/hardware-bom.json")
        )
        cls.bom_items = subject.validate_bom(cls.bom)

    def make_ready_inventory(self) -> dict[str, Any]:
        assets = []
        for index, item in enumerate(self.bom["items"], start=1):
            identity_method = sorted(
                subject.READINESS_IDENTITY_METHODS[item["category"]]
            )[0]
            asset = {
                "asset_id": f"synthetic-{index:02d}",
                "bom_item_id": item["id"],
                "category": item["category"],
                "status": "present",
                "manufacturer": "Synthetic test fixture",
                "model": item["id"],
                "condition": "serviceable",
                "observed_at": "2026-10-18T00:00:00Z",
                "identification": {
                    "availability": "recorded",
                    "method": identity_method,
                    "value": f"synthetic-{item['category']}-{index}",
                },
                "observed_capabilities": copy.deepcopy(item["recommended"]),
                "acceptance_results": {
                    check["id"]: True for check in item["acceptance_checks"]
                },
                "notes": "Synthetic validator fixture; not observed hardware.",
            }
            if item["quantity"]["technical_minimum"]["per_station"]:
                asset["assigned_station"] = "station-01"
            assets.append(asset)

        return {
            "$schema": "./schemas/hardware-inventory.schema.json",
            "schema_version": "1.0.0",
            "inventory_id": "synthetic-ready-fixture",
            "recorded_at": "2026-10-18T00:00:00Z",
            "location": {"site": "synthetic", "room": "synthetic"},
            "planned_stations": 1,
            "stations": [{"station_id": "station-01", "status": "ready"}],
            "assets": assets,
            "unresolved": [],
        }

    def validate_inventory(self, inventory: dict[str, Any]) -> None:
        subject.validate_against_schema(
            inventory, self.inventory_schema, Path("synthetic-inventory.json")
        )
        subject.validate_inventory(inventory, self.bom_items)

    def test_complete_synthetic_inventory_meets_readiness(self) -> None:
        inventory = self.make_ready_inventory()
        self.validate_inventory(inventory)
        subject.validate_readiness(inventory, self.bom_items)

    def test_readiness_rejects_unidentified_asset(self) -> None:
        inventory = self.make_ready_inventory()
        asset = inventory["assets"][0]
        asset["identification"] = {
            "availability": "unavailable",
            "method": "board-eeprom",
            "reason": "Synthetic missing identity",
        }
        self.validate_inventory(inventory)

        with self.assertRaisesRegex(
            subject.ValidationFailure, "identified, accepted assets"
        ):
            subject.validate_readiness(inventory, self.bom_items)

    def test_readiness_rejects_unsafe_reader_identity_method(self) -> None:
        inventory = self.make_ready_inventory()
        reader = next(
            asset
            for asset in inventory["assets"]
            if asset["category"] == "card_reader"
        )
        reader["identification"] = {
            "availability": "recorded",
            "method": "manual-label",
            "value": "synthetic-reader-label",
        }
        self.validate_inventory(inventory)

        with self.assertRaisesRegex(
            subject.ValidationFailure, "reader.microsd.*identified, accepted"
        ):
            subject.validate_readiness(inventory, self.bom_items)

    def test_readiness_rejects_failed_acceptance_check(self) -> None:
        inventory = self.make_ready_inventory()
        board = inventory["assets"][0]
        board["acceptance_results"]["board.condition"] = False
        self.validate_inventory(inventory)

        with self.assertRaisesRegex(
            subject.ValidationFailure, "board.ai64.*identified, accepted"
        ):
            subject.validate_readiness(inventory, self.bom_items)

    def test_inventory_rejects_unknown_acceptance_result(self) -> None:
        inventory = self.make_ready_inventory()
        inventory["assets"][0]["acceptance_results"]["board.unknown"] = True

        with self.assertRaisesRegex(
            subject.ValidationFailure, "unknown acceptance results"
        ):
            self.validate_inventory(inventory)

    def test_bom_rejects_missing_category(self) -> None:
        bom = copy.deepcopy(self.bom)
        bom["items"].pop()

        with self.assertRaisesRegex(subject.ValidationFailure, "missing categories"):
            subject.validate_bom(bom)


if __name__ == "__main__":
    unittest.main()
