"""Read-only discovery of UART adapters and removable readers.

Discovery reads a ``HostView``: the live implementation queries sysfs, udev
links, and ``lsblk``; the fixture implementation reads a recorded
JSON listing so every decision can be tested without hardware. Nothing here
opens a device for writing.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Protocol

SYSTEM_MOUNTPOINTS = {"/", "/boot", "/boot/efi", "/home", "/usr", "/var", "[SWAP]"}
PLACEHOLDER_SERIALS = {
    "",
    "0",
    "000000000000",
    "000000000001",
    "0123456789",
    "0123456789abcdef",
    "123456789012",
    "generic",
    "none",
    "serial",
}


def is_placeholder_serial(serial: str | None) -> bool:
    """Return True when a serial cannot distinguish two units."""
    if serial is None:
        return True
    value = serial.strip().lower()
    if value in PLACEHOLDER_SERIALS or len(value) < 4:
        return True
    return len(set(value)) == 1


@dataclass(frozen=True)
class Diagnostic:
    severity: str  # "error" blocks selection; "warning" is informational
    code: str
    subject: str
    message: str
    action: str


@dataclass(frozen=True)
class Uart:
    by_id: str
    device: str
    vendor_id: str
    product_id: str
    serial: str
    manufacturer: str
    product: str
    unique_serial: bool


@dataclass(frozen=True)
class Reader:
    by_id: str
    device: str
    model: str
    vendor: str
    serial: str
    size_bytes: int
    removable: bool
    transport: str
    mountpoints: tuple[str, ...]
    unique_serial: bool


@dataclass
class Discovery:
    uarts: list[Uart] = field(default_factory=list)
    readers: list[Reader] = field(default_factory=list)
    diagnostics: list[Diagnostic] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        return {
            "uarts": [asdict(item) for item in self.uarts],
            "readers": [asdict(item) for item in self.readers],
            "diagnostics": [asdict(item) for item in self.diagnostics],
        }


class HostView(Protocol):
    def serial_links(self) -> dict[str, str]:
        """Map /dev/serial/by-id link names to the device they resolve to."""

    def usb_serial_devices(self) -> dict[str, dict[str, str]]:
        """Map every USB tty device path to its USB attributes."""

    def block_devices(self) -> list[dict[str, Any]]:
        """Return ``lsblk --json --bytes`` block devices with children."""

    def disk_links(self) -> dict[str, str]:
        """Map /dev/disk/by-id link names to the device they resolve to."""


class FixtureHost:
    """A recorded host listing, used by tests and dry runs."""

    def __init__(self, data: dict[str, Any]) -> None:
        self._data = data

    @classmethod
    def load(cls, path: Path) -> "FixtureHost":
        with path.open(encoding="utf-8") as handle:
            return cls(json.load(handle))

    def serial_links(self) -> dict[str, str]:
        return dict(self._data.get("serial_by_id", {}))

    def usb_serial_devices(self) -> dict[str, dict[str, str]]:
        return dict(self._data.get("usb_serial_devices", {}))

    def block_devices(self) -> list[dict[str, Any]]:
        return list(self._data.get("lsblk", {}).get("blockdevices", []))

    def disk_links(self) -> dict[str, str]:
        return dict(self._data.get("disk_by_id", {}))


def _links(directory: Path) -> dict[str, str]:
    if not directory.is_dir():
        return {}
    return {
        entry.name: os.path.realpath(entry)
        for entry in sorted(directory.iterdir())
        if entry.is_symlink()
    }


def _read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace").strip()
    except OSError:
        return ""


class LiveHost:
    """The workstation this process runs on, queried read-only."""

    def serial_links(self) -> dict[str, str]:
        return _links(Path("/dev/serial/by-id"))

    def usb_serial_devices(self) -> dict[str, dict[str, str]]:
        devices: dict[str, dict[str, str]] = {}
        for tty in sorted(Path("/sys/class/tty").glob("tty[UA]*")):
            if not re.fullmatch(r"tty(USB|ACM)\d+", tty.name):
                continue
            usb = Path(os.path.realpath(tty / "device"))
            while usb != usb.parent and not (usb / "idVendor").exists():
                usb = usb.parent
            devices[f"/dev/{tty.name}"] = {
                key: _read(usb / key)
                for key in ("idVendor", "idProduct", "serial", "manufacturer", "product")
            }
        return devices

    def block_devices(self) -> list[dict[str, Any]]:
        columns = "NAME,PATH,TYPE,TRAN,RM,HOTPLUG,SIZE,MODEL,SERIAL,VENDOR,MOUNTPOINTS"
        result = subprocess.run(
            ["lsblk", "--json", "--bytes", "--output", columns],
            capture_output=True,
            text=True,
            check=True,
        )
        return json.loads(result.stdout)["blockdevices"]

    def disk_links(self) -> dict[str, str]:
        return _links(Path("/dev/disk/by-id"))


def _flag(value: Any) -> bool:
    """lsblk reports flags as booleans or, on older util-linux, as "0"/"1"."""
    return value in (True, 1, "1", "true")


def _mountpoints(device: dict[str, Any]) -> tuple[str, ...]:
    points = [point for point in device.get("mountpoints") or [] if point]
    for child in device.get("children") or []:
        points.extend(_mountpoints(child))
    return tuple(points)


def discover_uarts(host: HostView, result: Discovery) -> None:
    devices = host.usb_serial_devices()
    linked = {}
    for link, device in sorted(host.serial_links().items()):
        linked[device] = link
        attributes = devices.get(device, {})
        serial = attributes.get("serial", "")
        unique = not is_placeholder_serial(serial)
        result.uarts.append(
            Uart(
                by_id=link,
                device=device,
                vendor_id=attributes.get("idVendor", ""),
                product_id=attributes.get("idProduct", ""),
                serial=serial,
                manufacturer=attributes.get("manufacturer", ""),
                product=attributes.get("product", ""),
                unique_serial=unique,
            )
        )
        if not unique:
            result.diagnostics.append(
                Diagnostic(
                    "warning",
                    "uart-placeholder-serial",
                    link,
                    "This adapter reports no unique USB serial, so its by-id name "
                    "is shared by every identical adapter.",
                    "Keep only one adapter of this model connected, or use an "
                    "adapter with a factory serial such as the FTDI TTL-232R-3V3.",
                )
            )
    for device, attributes in sorted(devices.items()):
        if device not in linked:
            result.diagnostics.append(
                Diagnostic(
                    "error",
                    "uart-without-stable-identity",
                    device,
                    "A USB serial device has no /dev/serial/by-id link; udev hides "
                    "one of two identical serial-less adapters this way.",
                    "Disconnect all but one adapter of model "
                    f"{attributes.get('idVendor', '?')}:{attributes.get('idProduct', '?')} "
                    "and rerun discovery.",
                )
            )


def discover_readers(host: HostView, result: Discovery) -> None:
    by_device: dict[str, list[str]] = {}
    for link, device in host.disk_links().items():
        if re.search(r"-part\d+$", link):
            continue
        by_device.setdefault(device, []).append(link)

    for device in host.block_devices():
        if device.get("type") != "disk":
            continue
        mountpoints = _mountpoints(device)
        removable = _flag(device.get("rm")) or _flag(device.get("hotplug"))
        transport = device.get("tran") or ""
        if transport != "usb" or not removable:
            continue
        if SYSTEM_MOUNTPOINTS.intersection(mountpoints):
            result.diagnostics.append(
                Diagnostic(
                    "warning",
                    "reader-hosts-system-mount",
                    device["path"],
                    "A removable USB disk backs a system mount and is never a target.",
                    "None; it is excluded from reader selection.",
                )
            )
            continue
        links = sorted(by_device.get(device["path"], []))
        usb_links = [link for link in links if link.startswith("usb-")]
        serial = (device.get("serial") or "").strip()
        unique = not is_placeholder_serial(serial)
        if not usb_links:
            result.diagnostics.append(
                Diagnostic(
                    "error",
                    "reader-without-by-id",
                    device["path"],
                    "A removable USB disk has no /dev/disk/by-id usb-* link.",
                    "Reconnect the reader directly to the workstation and rerun "
                    "discovery; do not select it by /dev/sdX.",
                )
            )
            continue
        result.readers.append(
            Reader(
                by_id=usb_links[0],
                device=device["path"],
                model=(device.get("model") or "").strip(),
                vendor=(device.get("vendor") or "").strip(),
                serial=serial,
                size_bytes=int(device.get("size") or 0),
                removable=removable,
                transport=transport,
                mountpoints=mountpoints,
                unique_serial=unique,
            )
        )
        if not unique:
            result.diagnostics.append(
                Diagnostic(
                    "error",
                    "reader-placeholder-serial",
                    usb_links[0],
                    "The reader reports a placeholder serial, so it fails the BOM "
                    "stable-identity acceptance check.",
                    "Use a reader that exposes a unique serial before any automated "
                    "flashing.",
                )
            )


def discover(host: HostView) -> Discovery:
    result = Discovery()
    discover_uarts(host, result)
    discover_readers(host, result)
    if not result.uarts:
        result.diagnostics.append(
            Diagnostic(
                "warning",
                "no-uart",
                "/dev/serial/by-id",
                "No USB serial adapter is connected.",
                "Connect the 3.3 V USB-TTL adapter to the workstation and the "
                "board's debug UART header.",
            )
        )
    if not result.readers:
        result.diagnostics.append(
            Diagnostic(
                "warning",
                "no-reader",
                "/dev/disk/by-id",
                "No removable USB reader is connected.",
                "Connect the microSD reader with a card inserted.",
            )
        )
    return result
