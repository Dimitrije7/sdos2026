#!/usr/bin/env python3
"""Validate the SDOS hardware BOM and an observed inventory file."""

from __future__ import annotations

import argparse
import json
import math
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Iterable

try:
    from jsonschema import Draft202012Validator, FormatChecker
    from jsonschema.exceptions import SchemaError
except ImportError as exc:  # pragma: no cover - environmental failure path
    raise SystemExit(
        "jsonschema is required; use the pinned development environment once "
        "Task 017 provides it"
    ) from exc


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BOM = REPOSITORY_ROOT / "manifest" / "hardware-bom.json"
DEFAULT_INVENTORY = (
    REPOSITORY_ROOT / "manifest" / "hardware-inventory.example.json"
)
DEFAULT_BOM_SCHEMA = (
    REPOSITORY_ROOT / "manifest" / "schemas" / "hardware-bom.schema.json"
)
DEFAULT_INVENTORY_SCHEMA = (
    REPOSITORY_ROOT / "manifest" / "schemas" / "hardware-inventory.schema.json"
)
REQUIRED_CATEGORIES = {
    "board",
    "power_supply",
    "cooling",
    "storage",
    "card_reader",
    "uart",
    "ethernet",
    "recovery_media",
}
READINESS_IDENTITY_METHODS = {
    "board": {"asset-tag", "board-eeprom"},
    "power_supply": {"asset-tag", "manufacturer-serial", "manual-label"},
    "cooling": {"asset-tag", "manufacturer-serial", "manual-label"},
    "storage": {"block-by-id", "manufacturer-serial", "manual-label"},
    "card_reader": {"usb-serial"},
    "uart": {"usb-serial"},
    "ethernet": {"asset-tag", "manufacturer-serial", "manual-label"},
    "recovery_media": {"block-by-id", "manufacturer-serial", "manual-label"},
}


class ValidationFailure(Exception):
    """A deterministic validation error suitable for command-line output."""


def load_json(path: Path) -> Any:
    try:
        with path.open(encoding="utf-8") as handle:
            return json.load(handle)
    except FileNotFoundError as exc:
        raise ValidationFailure(f"missing file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ValidationFailure(
            f"invalid JSON in {path}:{exc.lineno}:{exc.colno}: {exc.msg}"
        ) from exc


def json_path(parts: Iterable[Any]) -> str:
    rendered = "$"
    for part in parts:
        rendered += f"[{part}]" if isinstance(part, int) else f".{part}"
    return rendered


def validate_against_schema(
    instance: Any, schema: Any, instance_path: Path
) -> None:
    try:
        Draft202012Validator.check_schema(schema)
    except SchemaError as exc:
        raise ValidationFailure(f"invalid schema for {instance_path}: {exc.message}")

    validator = Draft202012Validator(schema, format_checker=FormatChecker())
    errors = sorted(validator.iter_errors(instance), key=lambda error: list(error.path))
    if errors:
        rendered = "\n".join(
            f"  {json_path(error.path)}: {error.message}" for error in errors
        )
        raise ValidationFailure(
            f"schema validation failed for {instance_path}:\n{rendered}"
        )


def require_unique(values: Iterable[str], label: str) -> None:
    seen: set[str] = set()
    duplicates: set[str] = set()
    for value in values:
        if value in seen:
            duplicates.add(value)
        seen.add(value)
    if duplicates:
        raise ValidationFailure(f"duplicate {label}: {', '.join(sorted(duplicates))}")


def quantity_total(quantity: dict[str, Any], station_count: int) -> int:
    calculated = quantity["per_lab"] + quantity["per_station"] * station_count
    return max(quantity["minimum_total"], math.ceil(calculated))


def quantity_plan(item: dict[str, Any], station_count: int) -> tuple[int, int, int]:
    technical = quantity_total(item["quantity"]["technical_minimum"], station_count)
    recommended = quantity_total(item["quantity"]["recommended"], station_count)
    spare_policy = item["quantity"]["spares"]
    spares = max(
        spare_policy["minimum_total"],
        math.ceil(recommended * spare_policy["fraction_of_recommended"]),
    )
    return technical, recommended, spares


def validate_bom(bom: dict[str, Any]) -> dict[str, dict[str, Any]]:
    items = bom["items"]
    require_unique((item["id"] for item in items), "BOM item IDs")
    require_unique((item["category"] for item in items), "BOM categories")

    categories = {item["category"] for item in items}
    missing = REQUIRED_CATEGORIES - categories
    unexpected = categories - REQUIRED_CATEGORIES
    if missing or unexpected:
        messages = []
        if missing:
            messages.append(f"missing categories: {', '.join(sorted(missing))}")
        if unexpected:
            messages.append(f"unexpected categories: {', '.join(sorted(unexpected))}")
        raise ValidationFailure("; ".join(messages))

    for item in items:
        require_unique(
            (check["id"] for check in item["acceptance_checks"]),
            f"acceptance-check IDs for {item['id']}",
        )
        missing_capabilities = set(item["technical_minimum"]) - set(
            item["recommended"]
        )
        if missing_capabilities:
            raise ValidationFailure(
                f"{item['id']}: recommended capabilities omit technical "
                f"minimum keys: {', '.join(sorted(missing_capabilities))}"
            )
        technical, recommended, _ = quantity_plan(item, 1)
        if recommended < technical:
            raise ValidationFailure(
                f"{item['id']}: recommended quantity {recommended} is below "
                f"technical minimum {technical} for one station"
            )

    return {item["id"]: item for item in items}


def validate_inventory(
    inventory: dict[str, Any], bom_items: dict[str, dict[str, Any]]
) -> None:
    stations = {station["station_id"]: station for station in inventory["stations"]}
    assets = {asset["asset_id"]: asset for asset in inventory["assets"]}
    require_unique(
        (station["station_id"] for station in inventory["stations"]),
        "station IDs",
    )
    require_unique(
        (asset["asset_id"] for asset in inventory["assets"]),
        "asset IDs",
    )
    require_unique(
        (item["id"] for item in inventory["unresolved"]),
        "unresolved-item IDs",
    )

    stable_ids: list[str] = []
    for asset in assets.values():
        bom_item = bom_items.get(asset["bom_item_id"])
        if bom_item is None:
            raise ValidationFailure(
                f"{asset['asset_id']}: unknown BOM item {asset['bom_item_id']}"
            )
        if asset["category"] != bom_item["category"]:
            raise ValidationFailure(
                f"{asset['asset_id']}: category {asset['category']} does not match "
                f"BOM category {bom_item['category']}"
            )
        station_id = asset.get("assigned_station")
        if station_id is not None and station_id not in stations:
            raise ValidationFailure(
                f"{asset['asset_id']}: unknown assigned station {station_id}"
            )
        identification = asset.get("identification")
        if identification and identification["availability"] == "recorded":
            stable_ids.append(
                f"{identification['method']}:{identification['value']}"
            )
        result_ids = set(asset.get("acceptance_results", {}))
        expected_result_ids = {
            check["id"] for check in bom_item["acceptance_checks"]
        }
        unknown_result_ids = result_ids - expected_result_ids
        if unknown_result_ids:
            raise ValidationFailure(
                f"{asset['asset_id']}: unknown acceptance results for "
                f"{bom_item['id']}: {', '.join(sorted(unknown_result_ids))}"
            )
    require_unique(stable_ids, "recorded stable asset identities")


def validate_readiness(
    inventory: dict[str, Any], bom_items: dict[str, dict[str, Any]]
) -> None:
    planned = inventory["planned_stations"]
    if planned < 1:
        raise ValidationFailure("readiness requires planned_stations to be at least 1")

    ready_stations = {
        station["station_id"]
        for station in inventory["stations"]
        if station["status"] == "ready"
    }
    if len(ready_stations) != planned:
        raise ValidationFailure(
            f"readiness requires {planned} ready stations; found {len(ready_stations)}"
        )
    if inventory["unresolved"]:
        raise ValidationFailure("readiness requires an empty unresolved list")

    present_assets = [
        asset for asset in inventory["assets"] if asset["status"] == "present"
    ]
    for item in bom_items.values():
        technical, _, _ = quantity_plan(item, planned)
        accepted_assets = []
        expected_results = {
            check["id"] for check in item["acceptance_checks"]
        }
        for asset in present_assets:
            if asset["bom_item_id"] != item["id"]:
                continue
            identification = asset["identification"]
            if identification["availability"] != "recorded":
                continue
            if identification["method"] not in READINESS_IDENTITY_METHODS[
                item["category"]
            ]:
                continue
            results = asset["acceptance_results"]
            if expected_results and all(
                results.get(check_id) is True for check_id in expected_results
            ):
                accepted_assets.append(asset)

        present_count = len(accepted_assets)
        if present_count < technical:
            raise ValidationFailure(
                f"{item['id']}: readiness needs {technical} identified, accepted "
                f"assets; found {present_count}"
            )

        per_station = math.ceil(
            item["quantity"]["technical_minimum"]["per_station"]
        )
        if per_station:
            for station_id in sorted(ready_stations):
                assigned_count = sum(
                    1
                    for asset in accepted_assets
                    if asset.get("assigned_station") == station_id
                )
                if assigned_count < per_station:
                    raise ValidationFailure(
                        f"{item['id']}: station {station_id} needs {per_station} "
                        f"assigned assets; found {assigned_count}"
                    )


def iter_urls(bom: dict[str, Any]) -> Iterable[str]:
    for item in bom["items"]:
        for example in item["supported_examples"]:
            yield example["url"]
        for evidence in item["evidence"]:
            yield evidence["url"]


def check_url(url: str, timeout: float) -> None:
    headers = {"User-Agent": "sdos-hardware-validator/1.0"}
    request = urllib.request.Request(url, headers=headers, method="HEAD")
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            if response.status >= 400:
                raise ValidationFailure(f"link returned HTTP {response.status}: {url}")
    except urllib.error.HTTPError as exc:
        if exc.code not in {403, 405, 429}:
            raise ValidationFailure(f"link returned HTTP {exc.code}: {url}") from exc
        fallback = urllib.request.Request(url, headers=headers, method="GET")
        try:
            with urllib.request.urlopen(fallback, timeout=timeout) as response:
                if response.status >= 400:
                    raise ValidationFailure(
                        f"link returned HTTP {response.status}: {url}"
                    )
        except urllib.error.HTTPError as fallback_exc:
            if fallback_exc.code in {403, 429}:
                print(
                    "link endpoint rejects or rate-limits automated access with "
                    f"HTTP {fallback_exc.code}: {url}"
                )
                return
            raise ValidationFailure(
                f"link returned HTTP {fallback_exc.code}: {url}"
            ) from fallback_exc
        except urllib.error.URLError as fallback_exc:
            raise ValidationFailure(
                f"could not read link {url}: {fallback_exc}"
            ) from fallback_exc
        except TimeoutError as fallback_exc:
            raise ValidationFailure(f"timed out reading link: {url}") from fallback_exc
    except urllib.error.URLError as exc:
        raise ValidationFailure(f"could not read link {url}: {exc}") from exc
    except TimeoutError as exc:
        raise ValidationFailure(f"timed out reading link: {url}") from exc


def print_quantity_plan(bom: dict[str, Any], station_count: int) -> None:
    print(f"quantity plan for {station_count} station(s)")
    print("item\ttechnical\trecommended\tspares")
    for item in bom["items"]:
        technical, recommended, spares = quantity_plan(item, station_count)
        print(f"{item['id']}\t{technical}\t{recommended}\t{spares}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bom", type=Path, default=DEFAULT_BOM)
    parser.add_argument("--inventory", type=Path, default=DEFAULT_INVENTORY)
    parser.add_argument("--bom-schema", type=Path, default=DEFAULT_BOM_SCHEMA)
    parser.add_argument(
        "--inventory-schema", type=Path, default=DEFAULT_INVENTORY_SCHEMA
    )
    parser.add_argument(
        "--check-readiness",
        action="store_true",
        help="require a complete observed fleet that meets technical minima",
    )
    parser.add_argument(
        "--check-links",
        action="store_true",
        help="perform network checks for BOM example and evidence URLs",
    )
    parser.add_argument("--link-timeout", type=float, default=20.0)
    parser.add_argument(
        "--stations",
        type=int,
        help="print calculated technical, recommended, and spare quantities",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.stations is not None and args.stations < 1:
        raise ValidationFailure("--stations must be at least 1")

    bom = load_json(args.bom)
    inventory = load_json(args.inventory)
    validate_against_schema(bom, load_json(args.bom_schema), args.bom)
    validate_against_schema(
        inventory, load_json(args.inventory_schema), args.inventory
    )
    bom_items = validate_bom(bom)
    validate_inventory(inventory, bom_items)
    if args.check_readiness:
        validate_readiness(inventory, bom_items)
    if args.check_links:
        for url in sorted(set(iter_urls(bom))):
            check_url(url, args.link_timeout)
    if args.stations is not None:
        print_quantity_plan(bom, args.stations)
    print(
        f"validated BOM {bom['bom_id']} revision {bom['revision']} and "
        f"inventory {inventory['inventory_id']}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValidationFailure as exc:
        print(f"hardware validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
