"""Exact-identity selection that refuses ambiguous or unsafe devices.

Selection accepts only a stable ``/dev/serial/by-id`` or ``/dev/disk/by-id``
name, never an enumeration path such as ``/dev/ttyUSB0`` or ``/dev/sdb``.
``check_write_target`` performs no write: it returns the record a later task
must show the maintainer before requesting approval for a destructive operation.
"""

from __future__ import annotations

from dataclasses import asdict
from typing import Any

from .discovery import Discovery, Reader, Uart

MIN_CARD_BYTES = 30 * 1000**3  # the BOM requires 32 GB cards; allow formatting loss


class BoardAccessError(RuntimeError):
    """A selection was refused; the message says what to change."""


def _normalize(identity: str, prefix: str) -> str:
    identity = identity.strip()
    if identity.startswith(prefix):
        identity = identity[len(prefix):]
    if "/" in identity or not identity:
        raise BoardAccessError(
            f"'{identity}' is not a stable identity; pass a name from {prefix}"
        )
    return identity


def _blocking(discovery: Discovery, subject: str) -> list[str]:
    return [
        f"{item.code}: {item.message} Action: {item.action}"
        for item in discovery.diagnostics
        if item.severity == "error" and subject in (item.subject, "*")
    ]


def select_uart(discovery: Discovery, identity: str) -> Uart:
    name = _normalize(identity, "/dev/serial/by-id/")
    matches = [uart for uart in discovery.uarts if uart.by_id == name]
    if not matches:
        available = ", ".join(uart.by_id for uart in discovery.uarts) or "none"
        raise BoardAccessError(
            f"no UART with identity {name}; connected adapters: {available}"
        )
    uart = matches[0]
    hidden = [item for item in discovery.diagnostics if item.code == "uart-without-stable-identity"]
    if hidden and not uart.unique_serial:
        raise BoardAccessError(
            f"{name} has no unique serial and another adapter is hidden behind the "
            "same identity; disconnect the duplicate adapter"
        )
    return uart


def select_reader(discovery: Discovery, identity: str) -> Reader:
    name = _normalize(identity, "/dev/disk/by-id/")
    matches = [reader for reader in discovery.readers if reader.by_id == name]
    if not matches:
        available = ", ".join(reader.by_id for reader in discovery.readers) or "none"
        raise BoardAccessError(
            f"no removable reader with identity {name}; connected readers: {available}"
        )
    reader = matches[0]
    problems = _blocking(discovery, name)
    if problems:
        raise BoardAccessError("; ".join(problems))
    return reader


def check_write_target(discovery: Discovery, identity: str) -> dict[str, Any]:
    """Return the approval record for a card, or refuse. Never writes."""
    reader = select_reader(discovery, identity)
    refusals = []
    if not reader.unique_serial:
        refusals.append("the reader has no unique serial")
    if reader.transport not in {"usb", "mmc"} or not reader.removable:
        refusals.append("the device is not a removable USB disk or SD card")
    if reader.size_bytes == 0:
        refusals.append("no medium is inserted")
    elif reader.size_bytes < MIN_CARD_BYTES:
        refusals.append(f"the medium holds {reader.size_bytes} bytes, below 32 GB")
    if reader.mountpoints:
        refusals.append("partitions are mounted at " + ", ".join(reader.mountpoints))
    if refusals:
        raise BoardAccessError(f"{reader.by_id} is not a safe write target: " + "; ".join(refusals))
    record = asdict(reader)
    record["existing_content_erased_by_a_write"] = list(reader.partitions) or ["no partitions"]
    record["approval_required"] = (
        "Explicit maintainer approval naming this by_id, model, serial, and size "
        "is required before any write."
    )
    return record
