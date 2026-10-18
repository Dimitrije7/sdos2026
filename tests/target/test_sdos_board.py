#!/usr/bin/env python3
"""Mocked tests for safe board discovery, selection, identity, and capture."""

from __future__ import annotations

import contextlib
import hashlib
import io
import json
import lzma
import os
import tempfile
import threading
import time
import unittest
from pathlib import Path

from tests.target.sdos_board import __main__ as cli
from tests.target.sdos_board import flash, guards, network, report, serial_capture
from tests.target.sdos_board.discovery import FixtureHost, discover, is_placeholder_serial

FIXTURES = Path(__file__).resolve().parent / "fixtures"
OK_READER = "usb-Generic_MicroSD_Reader_200605992601B6A2-0:0"
OK_UART = "usb-FTDI_TTL232R-3V3_FT8ABCDE-if00-port0"


def bench(name: str):
    return discover(FixtureHost.load(FIXTURES / name))


class DiscoveryTests(unittest.TestCase):
    def test_healthy_bench_has_stable_identities_and_no_errors(self) -> None:
        result = bench("bench-ok.json")
        self.assertEqual([uart.by_id for uart in result.uarts], [OK_UART])
        self.assertEqual([reader.by_id for reader in result.readers], [OK_READER])
        self.assertFalse([item for item in result.diagnostics if item.severity == "error"])

    def test_system_nvme_is_never_a_reader(self) -> None:
        devices = {reader.device for reader in bench("bench-ok.json").readers}
        self.assertNotIn("/dev/nvme0n1", devices)

    def test_usb_disk_hosting_root_is_excluded(self) -> None:
        result = bench("bench-hazards.json")
        self.assertNotIn("/dev/sdf", {reader.device for reader in result.readers})
        self.assertIn("reader-hosts-system-mount", {item.code for item in result.diagnostics})

    def test_hidden_duplicate_uart_is_an_error(self) -> None:
        result = bench("bench-hazards.json")
        errors = {(item.code, item.subject) for item in result.diagnostics if item.severity == "error"}
        self.assertIn(("uart-without-stable-identity", "/dev/ttyUSB0"), errors)

    def test_string_flags_from_older_lsblk_are_parsed(self) -> None:
        reader = next(
            item for item in bench("bench-hazards.json").readers if item.device == "/dev/sdb"
        )
        self.assertTrue(reader.removable)

    def test_placeholder_serials(self) -> None:
        for value in ("", "000000000000", "Generic", "AAAA", "12", None):
            self.assertTrue(is_placeholder_serial(value), value)
        self.assertFalse(is_placeholder_serial("FT8ABCDE"))

    def test_empty_host_reports_actionable_missing_devices(self) -> None:
        result = discover(FixtureHost({}))
        codes = {item.code: item for item in result.diagnostics}
        self.assertIn("no-uart", codes)
        self.assertIn("microSD card", codes["no-reader"].action)


class GuardTests(unittest.TestCase):
    def test_select_uart_accepts_name_or_full_by_id_path(self) -> None:
        result = bench("bench-ok.json")
        self.assertEqual(guards.select_uart(result, OK_UART).device, "/dev/ttyUSB0")
        self.assertEqual(
            guards.select_uart(result, f"/dev/serial/by-id/{OK_UART}").device, "/dev/ttyUSB0"
        )

    def test_enumeration_paths_are_refused(self) -> None:
        result = bench("bench-ok.json")
        with self.assertRaisesRegex(guards.BoardAccessError, "not a stable identity"):
            guards.select_uart(result, "/dev/ttyUSB0")
        with self.assertRaisesRegex(guards.BoardAccessError, "not a stable identity"):
            guards.check_write_target(result, "/dev/sdb")

    def test_ambiguous_serialless_uart_is_refused(self) -> None:
        with self.assertRaisesRegex(guards.BoardAccessError, "disconnect the duplicate"):
            guards.select_uart(bench("bench-hazards.json"), "usb-1a86_USB_Serial-if00-port0")

    def test_missing_device_lists_alternatives(self) -> None:
        with self.assertRaisesRegex(guards.BoardAccessError, f"connected adapters: {OK_UART}"):
            guards.select_uart(bench("bench-ok.json"), "usb-FTDI_other-if00-port0")

    def test_safe_target_returns_an_approval_record_only(self) -> None:
        record = guards.check_write_target(bench("bench-ok.json"), OK_READER)
        self.assertEqual(record["device"], "/dev/sdb")
        self.assertIn("approval", record["approval_required"])

    def test_unsafe_targets_are_refused_with_reasons(self) -> None:
        result = bench("bench-hazards.json")
        cases = {
            "usb-Generic_STORAGE_DEVICE_000000000000-0:0": "placeholder serial",
            "usb-Acme_Reader_ACME1234567-0:0": "mounted at /media/student/BOOT",
            "usb-Acme_Reader_ACME7654321-0:0": "below 32 GB",
            "usb-Acme_Reader_ACME5550001-0:0": "no medium",
            "usb-Samsung_Flash_Drive_0374120090008421-0:0": "no removable reader",
        }
        for identity, reason in cases.items():
            with self.subTest(identity=identity):
                with self.assertRaisesRegex(guards.BoardAccessError, reason):
                    guards.check_write_target(result, identity)


class SdSlotTests(unittest.TestCase):
    CARD = "mmc-SC32G_0x1a2b3c4d"

    def test_card_in_builtin_slot_is_a_reader_and_emmc_is_not(self) -> None:
        result = bench("bench-sd-slot.json")
        self.assertEqual([(r.by_id, r.transport) for r in result.readers], [(self.CARD, "mmc")])

    def test_mounted_old_image_is_refused(self) -> None:
        with self.assertRaisesRegex(guards.BoardAccessError, "mounted at /media/maintainer/rootfs"):
            guards.check_write_target(bench("bench-sd-slot.json"), self.CARD)

    def test_approval_record_lists_content_a_write_would_erase(self) -> None:
        data = json.loads((FIXTURES / "bench-sd-slot.json").read_text())
        data["lsblk"]["blockdevices"][1]["children"][1]["mountpoints"] = [None]
        record = guards.check_write_target(discover(FixtureHost(data)), self.CARD)
        self.assertEqual(
            record["existing_content_erased_by_a_write"],
            ["mmcblk0p1 vfat label=BOOT 134217728 bytes", "mmcblk0p2 ext4 label=rootfs 31779717120 bytes"],
        )

    def test_board_usb_c_console_is_a_uart_with_the_board_serial(self) -> None:
        uart = guards.select_uart(
            bench("bench-sd-slot.json"), "usb-BeagleBoard.org_BeagleBone_AI-64_1234BBAI6400017-if02"
        )
        self.assertEqual((uart.device, uart.unique_serial), ("/dev/ttyACM0", True))

    def test_board_gadget_storage_is_never_a_reader(self) -> None:
        data = json.loads((FIXTURES / "bench-sd-slot.json").read_text())
        data["disk_by_id"]["usb-Linux_File-Stor_Gadget_1234BBBK5678-0:0"] = "/dev/sda"
        data["lsblk"]["blockdevices"].append(
            {"name": "sda", "path": "/dev/sda", "type": "disk", "tran": "usb", "rm": True,
             "hotplug": True, "size": 18874368, "model": "File-Stor Gadget",
             "serial": "1234BBBK5678", "vendor": "Linux", "mountpoints": [None]}
        )
        result = discover(FixtureHost(data))
        self.assertNotIn("/dev/sda", {reader.device for reader in result.readers})
        self.assertIn("board-gadget-storage", {item.code for item in result.diagnostics})

    def test_beagle_default_gadget_serial_is_a_placeholder(self) -> None:
        self.assertTrue(is_placeholder_serial("1234BBBK5678"))

    def test_zero_card_serial_is_a_placeholder(self) -> None:
        self.assertTrue(is_placeholder_serial("0x00000000"))


class NetworkTests(unittest.TestCase):
    SCAN = b"board.local ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIExample\n"
    LISTED = b"256 SHA256:abcDEF123 board.local (ED25519)\n"

    def runner(self, argv, stdin):
        if argv[0] == "ssh-keyscan":
            return 0, self.SCAN
        self.assertEqual(stdin, self.SCAN)
        return 0, self.LISTED

    def test_host_keys_are_fingerprinted_without_login(self) -> None:
        keys = network.ssh_host_keys("board.local", self.runner)
        self.assertEqual([(key.key_type, key.fingerprint) for key in keys], [("ED25519", "SHA256:abcDEF123")])

    def test_unreachable_host_is_not_scanned(self) -> None:
        def refuse(_argv, _stdin):
            raise AssertionError("must not scan an unreachable host")

        target = network.probe("board.local", lambda _h: ("192.0.2.10",), lambda _a: False, refuse)
        self.assertFalse(target.ssh_reachable)
        self.assertIn("does not accept SSH", network.check_known_identity(target, ["SHA256:x"])[0])

    def test_changed_host_key_is_reported(self) -> None:
        target = network.probe("board.local", lambda _h: ("192.0.2.10",), lambda _a: True, self.runner)
        self.assertEqual(network.check_known_identity(target, ["SHA256:abcDEF123"]), [])
        self.assertIn("match no recorded", network.check_known_identity(target, ["SHA256:old"])[0])


class SerialCaptureTests(unittest.TestCase):
    def run_capture(self, payloads, **kwargs):
        master, slave = os.openpty()
        slave_path = os.ttyname(slave)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        out = Path(temporary.name)

        def board():
            time.sleep(0.2)
            for payload in payloads:
                os.write(master, payload)
                time.sleep(0.05)

        writer = threading.Thread(target=board)
        writer.start()
        try:
            result = serial_capture.capture(slave_path, out, identity="pty-test", **kwargs)
        finally:
            writer.join(timeout=10)
            os.close(master)
            os.close(slave)
        return result, out

    def test_capture_stops_on_pattern_and_keeps_raw_bytes(self) -> None:
        payloads = [b"U-Boot 2026.10\r\n", b"Starting kernel ...\r\n", b"BeagleBone login: \n"]
        metadata, out = self.run_capture(payloads, until=r"login:", seconds=5)
        self.assertEqual(metadata["stop_reason"], "pattern-matched")
        self.assertEqual(metadata["direction"], "receive-only")
        raw = (out / "raw.bin").read_bytes()
        self.assertTrue(raw.startswith(b"U-Boot 2026.10"))
        lines = (out / "console.log").read_text().splitlines()
        self.assertEqual(len(lines), 3)
        self.assertRegex(lines[0], r"^\d{4}-\d\d-\d\dT[\d:.]+Z \+\d+\.\d{6} U-Boot 2026\.10$")
        self.assertEqual(json.loads((out / "capture.json").read_text())["lines"], 3)

    def test_time_limit_flushes_a_partial_line(self) -> None:
        metadata, out = self.run_capture([b"partial output without newline"], seconds=0.8)
        self.assertEqual(metadata["stop_reason"], "time-limit")
        self.assertIn("[partial] partial output", (out / "console.log").read_text())


class FlashTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.payload = os.urandom(3 * 1024 * 1024 + 17)
        self.image = self.dir / "card.img.xz"
        with lzma.open(self.image, "wb") as handle:
            handle.write(self.payload)
        self.target = self.dir / "card.bin"
        self.target.write_bytes(b"old image" * 1000)
        self.sha = hashlib.sha256(self.payload).hexdigest()

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_write_and_readback_match_the_manifest_hash(self) -> None:
        result = flash.write_image(self.image, str(self.target), self.sha, len(self.payload))
        self.assertEqual(result["readback_sha256"], self.sha)
        self.assertEqual(self.target.read_bytes()[: len(self.payload)], self.payload)

    def test_wrong_image_content_fails(self) -> None:
        with self.assertRaisesRegex(flash.FlashError, "manifest expects"):
            flash.write_image(self.image, str(self.target), "0" * 64, len(self.payload))

    def test_confirmation_must_repeat_the_target(self) -> None:
        discovery = bench("bench-ok.json")
        with self.assertRaisesRegex(guards.BoardAccessError, "--confirm must repeat"):
            flash.authorize(discovery, OK_READER, "usb-Other_Reader-0:0")
        self.assertEqual(flash.authorize(discovery, OK_READER, OK_READER)["by_id"], OK_READER)

    def test_mounted_card_is_refused_before_confirmation(self) -> None:
        with self.assertRaisesRegex(guards.BoardAccessError, "mounted"):
            flash.authorize(bench("bench-sd-slot.json"), "mmc-SC32G_0x1a2b3c4d", "mmc-SC32G_0x1a2b3c4d")

    def test_manifest_image_has_expanded_hash(self) -> None:
        image = flash.manifest_image("beagle.bbai64-debian-13.7-iot-2026-10-09")
        self.assertEqual(image["expanded_size"], 10737418240)
        with self.assertRaises(flash.FlashError):
            flash.manifest_image("beagle.bbai64-iot-2026-10-09-bmap")

    def test_cli_refuses_flash_from_a_fixture(self) -> None:
        argv = ["--fixture", str(FIXTURES / "bench-ok.json"), "flash", "--id", OK_READER,
                "--confirm", OK_READER, "--input", "x", "--image", str(self.image)]
        self.assertEqual(cli.main(argv), 3)


class BoardReportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.data = report.parse((FIXTURES / "board-report.raw.txt").read_text())
        self.results = {r["item"]: r for r in report.evaluate(self.data)}

    def test_dsps_are_found_by_address_not_enumeration(self) -> None:
        self.assertEqual(self.results["remoteproc C71x_0"]["status"], "pass")
        self.assertIn("remoteproc2", self.results["remoteproc C71x_0"]["detail"])

    def test_missing_kernel_feature_is_an_explicit_failure(self) -> None:
        self.assertEqual(self.results["kernel features"]["status"], "fail")
        self.assertIn("CONFIG_FTRACE", self.results["kernel features"]["detail"])

    def test_reserved_memory_is_decoded(self) -> None:
        first = self.data["memory"]["reserved"][0]
        self.assertEqual(first["base"], "0xa6000000")
        self.assertEqual(first["size"], 0x100000)

    def test_health_items_have_results(self) -> None:
        self.assertEqual(self.results["storage"]["status"], "pass")
        self.assertEqual(self.results["time"]["status"], "fail")
        self.assertEqual(self.results["ethernet"]["status"], "fail")
        self.assertEqual(self.results["debugfs and tracefs"]["status"], "fail")
        self.assertEqual(self.results["persistent logs"]["status"], "pass")
        self.assertEqual(self.results["CMA"]["status"], "pass")

    def test_mac_addresses_are_redacted(self) -> None:
        text = json.dumps(self.data)
        self.assertNotIn("1c:ba:8c:a2:ed:6b", text)
        self.assertIn("mac-", text)

    def test_cli_evaluates_a_saved_report_without_a_board(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = cli.main(["board-report", "--raw", str(FIXTURES / "board-report.raw.txt"), "--out", tmp])
            self.assertEqual(code, 1)  # the fixture has failing items
            saved = json.loads((Path(tmp) / "board-report.json").read_text())
            self.assertEqual(saved["schema"], report.SCHEMA)
            self.assertIn("FAIL  kernel features", out.getvalue())


class CliTests(unittest.TestCase):
    def test_refusal_exit_code_and_redaction(self) -> None:
        fixture = str(FIXTURES / "bench-hazards.json")
        self.assertEqual(
            cli.main(["--fixture", fixture, "check-write-target", "--id", "usb-Acme_Reader_ACME7654321-0:0"]),
            3,
        )
        report = cli.redact(bench("bench-ok.json").to_dict())
        text = json.dumps(report)
        self.assertNotIn("FT8ABCDE", text)
        self.assertNotIn("200605992601B6A2", text)
        self.assertIn("redacted-", report["uarts"][0]["by_id"])


if __name__ == "__main__":
    unittest.main()
