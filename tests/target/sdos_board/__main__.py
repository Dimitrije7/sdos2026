"""Command interface: ``python3 -m tests.target.sdos_board <command>``."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from dataclasses import asdict
from pathlib import Path
from typing import Any

from . import guards, network, serial_capture
from .discovery import Discovery, FixtureHost, HostView, LiveHost, discover


def redact_value(value: str) -> str:
    return "redacted-" + hashlib.sha256(value.encode("utf-8")).hexdigest()[:12]


def redact(report: dict[str, Any]) -> dict[str, Any]:
    """Replace USB serials, including inside by-id names, with stable hashes."""
    text = json.dumps(report)
    serials = {
        item["serial"]
        for key in ("uarts", "readers")
        for item in report.get(key, [])
        if len(item.get("serial", "")) >= 4
    }
    for serial in sorted(serials, key=len, reverse=True):
        text = text.replace(serial, redact_value(serial))
    return json.loads(text)


def host_from(args: argparse.Namespace) -> HostView:
    return FixtureHost.load(args.fixture) if args.fixture else LiveHost()


def run_discovery(args: argparse.Namespace) -> Discovery:
    return discover(host_from(args))


def emit(data: dict[str, Any], args: argparse.Namespace) -> None:
    if getattr(args, "redact", False):
        data = redact(data)
    text = json.dumps(data, indent=2) + "\n"
    if getattr(args, "json", None):
        args.json.write_text(text, encoding="utf-8")
    sys.stdout.write(text)


def cmd_discover(args: argparse.Namespace) -> int:
    result = run_discovery(args)
    report = result.to_dict()
    report["network"] = [network.probe(host).to_dict() for host in args.host]
    emit(report, args)
    return 1 if any(item.severity == "error" for item in result.diagnostics) else 0


def cmd_select_uart(args: argparse.Namespace) -> int:
    emit(asdict(guards.select_uart(run_discovery(args), args.id)), args)
    return 0


def cmd_check_write_target(args: argparse.Namespace) -> int:
    emit(guards.check_write_target(run_discovery(args), args.id), args)
    return 0


def cmd_ssh_identity(args: argparse.Namespace) -> int:
    target = network.probe(args.host)
    problems = network.check_known_identity(target, args.expect) if args.expect else []
    report = target.to_dict()
    report["problems"] = problems
    emit(report, args)
    return 1 if problems or not target.ssh_reachable else 0


def cmd_capture(args: argparse.Namespace) -> int:
    uart = guards.select_uart(run_discovery(args), args.uart)
    metadata = serial_capture.capture(
        f"/dev/serial/by-id/{uart.by_id}",
        args.out,
        identity=uart.by_id,
        baud=args.baud,
        seconds=args.seconds,
        until=args.until,
    )
    emit(metadata, args)
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="python3 -m tests.target.sdos_board", description=__doc__)
    parser.add_argument("--fixture", type=Path, help="read a recorded host listing instead of this machine")
    parser.add_argument("--redact", action="store_true", help="hash USB serials in the output")
    parser.add_argument("--json", type=Path, help="also write the output to this file")
    commands = parser.add_subparsers(dest="command", required=True)

    sub = commands.add_parser("discover", help="list UARTs, readers, and network targets")
    sub.add_argument("--host", action="append", default=[], help="board hostname or address to probe")
    sub.set_defaults(handler=cmd_discover)

    sub = commands.add_parser("select-uart", help="resolve one UART by exact by-id name")
    sub.add_argument("--id", required=True)
    sub.set_defaults(handler=cmd_select_uart)

    sub = commands.add_parser("check-write-target", help="print the approval record for a card; never writes")
    sub.add_argument("--id", required=True)
    sub.set_defaults(handler=cmd_check_write_target)

    sub = commands.add_parser("ssh-identity", help="scan SSH host keys and compare with recorded fingerprints")
    sub.add_argument("--host", required=True)
    sub.add_argument("--expect", action="append", default=[], help="recorded SHA256 fingerprint")
    sub.set_defaults(handler=cmd_ssh_identity)

    sub = commands.add_parser("capture", help="receive-only serial capture")
    sub.add_argument("--uart", required=True, help="exact /dev/serial/by-id name")
    sub.add_argument("--out", type=Path, required=True)
    sub.add_argument("--baud", type=int, default=115200)
    sub.add_argument("--seconds", type=float)
    sub.add_argument("--until", help="stop after a line matching this regular expression")
    sub.set_defaults(handler=cmd_capture)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        return args.handler(args)
    except guards.BoardAccessError as exc:
        print(f"refused: {exc}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
