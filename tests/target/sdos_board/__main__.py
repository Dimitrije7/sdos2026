"""Command interface: ``python3 -m tests.target.sdos_board <command>``."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
from dataclasses import asdict
from pathlib import Path
from typing import Any

from . import course_smoke, dsp_ipc, fault_injection, flash, guards, network, report, serial_capture
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


def cmd_flash(args: argparse.Namespace) -> int:
    if args.fixture:
        raise guards.BoardAccessError("flash acts on the live host only; remove --fixture")
    record = flash.authorize(run_discovery(args), args.id, args.confirm)
    image = flash.manifest_image(args.input)
    if args.image.name != image["file_name"]:
        raise guards.BoardAccessError(f"{args.image.name} is not the manifest file {image['file_name']}")
    print(f"checking {args.image.name}", file=sys.stderr, flush=True)
    actual = flash.sha256_file(args.image)
    if actual != image["sha256"]:
        raise guards.BoardAccessError(f"{args.image} has SHA-256 {actual}; the manifest pins {image['sha256']}")
    report: dict[str, Any] = {"target": record, "input": image, "result": "failed"}
    try:
        report.update(
            flash.write_image(
                args.image,
                f"/dev/disk/by-id/{record['by_id']}",
                image["expanded_sha256"],
                image["expanded_size"],
            )
        )
        report["result"] = "written-and-verified"
    except (OSError, flash.FlashError) as exc:
        report["error"] = str(exc)
    emit(report, args)
    owner = os.environ.get("SUDO_UID"), os.environ.get("SUDO_GID")
    if args.json and all(owner):
        os.chown(args.json, int(owner[0]), int(owner[1]))
    return 0 if report["result"] == "written-and-verified" else 1


def cmd_board_report(args: argparse.Namespace) -> int:
    if args.raw:
        text = args.raw.read_text(encoding="utf-8")
    else:
        text = report.collect(args.host, args.key)
    data = report.parse(text)
    results = report.evaluate(data)
    if args.out:
        report.write(data, results, args.out)
        if not args.raw:
            (args.out / "board-report.raw.txt").write_text(text, encoding="utf-8")
            data["identity"]["fdt_copy_sha256"] = report.fetch_fdt(args.host, args.out / "live.dtb", args.key)
            report.write(data, results, args.out)
    sys.stdout.write(report.summary(data, results))
    return 1 if any(r["status"] == "fail" for r in results) else 0


def cmd_dsp_ipc(args: argparse.Namespace) -> int:
    from ci import container  # the repository root is on sys.path under `python3 -m`

    try:
        selected = dsp_ipc.resolve(args.core or list(dsp_ipc.CORES))
    except ValueError as exc:
        print(f"dsp-ipc: {exc}", file=sys.stderr)
        return 2
    inputs = {item["id"]: item for item in json.loads(container.MANIFEST.read_text())["inputs"]}
    license_file = container.fetch_input("ti.linux-firmware-license-ti")
    dsp_ipc.upload(args.host, args.key, license_file, inputs["ti.linux-firmware-license-ti"]["integrity"]["sha256"])
    failed = []
    if args.firmware and len(selected) != 1:
        print("dsp-ipc: --firmware needs exactly one --core", file=sys.stderr)
        return 2
    for core in selected:  # one DSP at a time: Task 013 excludes concurrent use
        if args.firmware:  # e.g. Task 020's rebuild of TI's echo firmware
            firmware = args.firmware
            sha256, input_id = hashlib.sha256(firmware.read_bytes()).hexdigest(), f"local:{firmware.name}"
        else:
            firmware = container.fetch_input(core.ti_echo_input)
            sha256 = inputs[core.ti_echo_input]["integrity"]["sha256"]
            input_id = core.ti_echo_input
        result = dsp_ipc.run_core(
            core, args.host, args.key, firmware, sha256, input_id, args.out,
            count=args.count, cycles=args.cycles,
        )
        verdict = "pass" if result["passed"] else "FAIL"
        failing = [name for name, ok in result["checks"].items() if not ok]
        print(f"{core.label:8} {verdict}" + (f"  failed: {', '.join(failing)}" if failing else ""))
        if not result["passed"]:
            failed.append(core.name)
    return 1 if failed else 0


def cmd_course_smoke(args: argparse.Namespace) -> int:
    try:
        selected = dsp_ipc.resolve(args.core or list(dsp_ipc.CORES))
        results = course_smoke.run(args.host, args.key, args.out, selected)
    except (ValueError, RuntimeError) as exc:
        print(f"course-smoke: {exc}", file=sys.stderr)
        return 1
    for result in results:
        failing = [name for name, ok in result["checks"].items() if not ok]
        print(f"{result['core']:8} {result['runtime']:10} {'pass' if result['passed'] else 'FAIL'}"
              + (f"  failed: {', '.join(failing)}" if failing else ""))
    return 0 if all(r["passed"] for r in results) else 1


def cmd_fault_injection(args: argparse.Namespace) -> int:
    try:
        selected = dsp_ipc.resolve(args.core or list(dsp_ipc.CORES))
        results = fault_injection.run(args.host, args.key, args.out, selected)
    except (ValueError, RuntimeError) as exc:
        print(f"fault-injection: {exc}", file=sys.stderr)
        return 1
    for result in results:
        missed = [f"{name} ({case['outcome']})" for name, case in result["cases"].items()
                  if not (case["detected"] and case["recovered"])]
        print(f"{result['core']:8} {result['runtime']:10} {'pass' if result['passed'] else 'FAIL'}"
              + (f"  missed: {', '.join(missed)}" if missed else ""))
    return 0 if all(r["passed"] for r in results) else 1


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

    sub = commands.add_parser("board-report", help="read-only board health report over SSH")
    sub.add_argument("--host", default="192.168.7.2")
    sub.add_argument("--key", type=Path, help="SSH private key for root on the board")
    sub.add_argument("--out", type=Path, help="directory for board-report.json, .txt, raw text, and live.dtb")
    sub.add_argument("--raw", type=Path, help="evaluate a saved raw report instead of connecting")
    sub.set_defaults(handler=cmd_board_report)

    sub = commands.add_parser("dsp-ipc", help="Task 013: load, start, echo, stop, and restart TI echo firmware")
    sub.add_argument("--host", default="192.168.7.2")
    sub.add_argument("--key", type=Path, help="SSH private key for root on the board")
    sub.add_argument("--core", action="append", help="c66x_0, c66x_1, or c71x_0; repeat; default all, in turn")
    sub.add_argument("--out", type=Path, required=True, help="directory for per-core results and logs")
    sub.add_argument("--count", type=int, default=10, help="ping messages per cycle")
    sub.add_argument("--cycles", type=int, default=2, help="start/echo/stop cycles; 2 proves restart")
    sub.add_argument("--firmware", type=Path, help="a local TI-echo-compatible ELF instead of the pinned blob")
    sub.set_defaults(handler=cmd_dsp_ipc)

    sub = commands.add_parser("course-smoke", help="Task 021: course firmware on every core and runtime via dspctl")
    sub.add_argument("--host", default="192.168.7.2")
    sub.add_argument("--key", type=Path, help="SSH private key for root on the board")
    sub.add_argument("--core", action="append", help="c66x_0, c66x_1, or c71x_0; repeat; default all")
    sub.add_argument("--out", type=Path, required=True, help="directory for per-core, per-runtime results")
    sub.set_defaults(handler=cmd_course_smoke)

    sub = commands.add_parser("fault-injection", help="Task 042: every safety mechanism catches its fault")
    sub.add_argument("--host", default="192.168.7.2")
    sub.add_argument("--key", type=Path, help="SSH private key for root on the board")
    sub.add_argument("--core", action="append", help="c66x_0, c66x_1, or c71x_0; repeat; default all")
    sub.add_argument("--out", type=Path, required=True, help="directory for per-core, per-runtime results")
    sub.set_defaults(handler=cmd_fault_injection)

    sub = commands.add_parser("flash", help="write a pinned image to an approved card and verify it")
    sub.add_argument("--id", required=True, help="exact /dev/disk/by-id name of the card")
    sub.add_argument("--confirm", required=True, help="the same by-id, repeated as the approval")
    sub.add_argument("--input", required=True, help="manifest input id of the image")
    sub.add_argument("--image", type=Path, required=True, help="downloaded image file")
    sub.set_defaults(handler=cmd_flash)
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
