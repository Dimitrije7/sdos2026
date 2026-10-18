# Safe connected-board access

Status: implemented; mocked suite and a read-only connected-board discovery
passed on 2026-10-18  
Task: [007 — Build safe connected-board access](../tasks/007-safe-board-access.md)  
Code: [`tests/target/sdos_board/`](../tests/target/sdos_board/)

Agents use this tool to find and identify the bench hardware connected to the
development PC: the USB-UART adapter, the microSD reader, and the board on the
network. It never writes to a device. Later tasks that flash a card or change
board state call its guards first and then request the maintainer's approval.

## Safety rules the tool enforces

- **Stable identities only.** Devices are selected by their
  `/dev/serial/by-id` or `/dev/disk/by-id` name. An enumeration path such as
  `/dev/ttyUSB0` or `/dev/sdb` is refused, because it can change on replug.
- **Ambiguity fails closed.** Two identical adapters without a serial share
  one by-id name, and udev hides one of them. Discovery reports the hidden
  device, and selection refuses the shared name.
- **System disks are invisible.** Only removable USB disks are readers. A USB
  disk that backs `/`, `/boot`, or another system mount is excluded.
- **Readers need a unique serial.** A reader that reports a placeholder serial
  fails the hardware-BOM identity check and is refused as a write target.
- **Write targets are checked, not written.** `check-write-target` refuses a
  reader with mounted partitions, no medium, or a card smaller than 32 GB. It
  returns the record that the approval request must quote.
- **Capture is receive-only.** The UART is opened read-only, so capture cannot
  type into the board's console.

## Supported bench connections

The board's USB-C port carries both power and USB gadget functions. With the
board's USB-C connected to the development PC, the stock image provides:

| Function | Host side | Use |
|---|---|---|
| Virtual Ethernet | `192.168.7.2` (RNDIS) and `192.168.6.2` (NCM) | SSH and file transfer; no Ethernet cable is needed |
| Serial console | `/dev/ttyACM0` with a `usb-BeagleBoard.org_*` by-id name | Linux console after boot; U-Boot output needs the UART header |
| Mass storage | A small `Linux File-Stor Gadget` disk | Never a reader; discovery excludes it |

The gadget's USB serial `1234BBBK5678` is a fixed default shared by every
board. It identifies the connection, not the board, so discovery flags it as
non-unique. The SSH host keys, and later the EEPROM identity, identify the board.

A microSD card can sit in a USB reader or in a built-in SD slot. A built-in slot
appears as `/dev/mmcblk*` with an `mmc-<name>_<card serial>` by-id link; that
serial belongs to the card, so the identity follows the card. Internal eMMC,
which the kernel reports as type `MMC`, is excluded.

Power over USB-C needs a supply above 5 V/3 A. A PC port, and any
USB-A-to-USB-C cable, may not provide that. BeagleBoard's quick start says that
with a USB-A-to-C cable the board needs power from its DC barrel jack. The
reference bench therefore uses a 5 V, 4 A center-positive barrel supply, with
the USB-C cable carrying data only.

## Prerequisites

- Python 3.10 or newer on the development PC. The tool uses only the standard
  library.
- `lsblk` from util-linux, and `ssh-keyscan` and `ssh-keygen` from OpenSSH.
- Read access to the UART device, normally through the `dialout` group.

Run every command from the repository root.

## Commands

Discover everything connected. Add `--host` for each network name or address
to probe:

```sh
python3 -m tests.target.sdos_board discover --host beaglebone.local
```

With nothing connected, the output lists no devices and two warnings:

```text
"code": "no-uart",
...
"code": "no-reader",
```

The exit status is 0 when discovery found no blocking error and 1 when it did.
Every diagnostic carries a `code`, a `message`, and an `action` that says what
to change.

Resolve one UART, or check a card before a later task asks to write it:

```sh
python3 -m tests.target.sdos_board select-uart --id usb-FTDI_TTL232R-3V3_<serial>-if00-port0
python3 -m tests.target.sdos_board check-write-target --id usb-<vendor>_<model>_<serial>-0:0
```

A refusal prints `refused:` with the reason and exits with status 3.

Record the board's SSH host keys, or confirm that the same board still
answers:

```sh
python3 -m tests.target.sdos_board ssh-identity --host beaglebone.local
python3 -m tests.target.sdos_board ssh-identity --host beaglebone.local --expect SHA256:<recorded>
```

Capture the serial console until the login prompt or a time limit:

```sh
python3 -m tests.target.sdos_board capture --uart <by-id name> --out <dir> --until 'login:' --seconds 300
```

Capture writes three files to the output directory:

| File | Content |
|---|---|
| `raw.bin` | Every received byte, fsynced after each read |
| `console.log` | One line per console line, with UTC and monotonic timestamps |
| `capture.json` | Identity, baud rate, duration, counts, and stop reason |

Global options go before the command. `--fixture <file>` reads a recorded host
listing instead of the live machine. `--redact` replaces USB serials with short
hashes for evidence that will be published. `--json <file>` also writes the
output to a file.

## Failure modes and recovery

| Code or message | Cause | Recovery |
|---|---|---|
| `no-uart` | No USB serial adapter is connected | Connect the 3.3 V adapter to the PC and the board's debug UART header |
| `uart-without-stable-identity` | Two identical adapters without serials | Disconnect all but one, or use an adapter with a factory serial |
| `uart-placeholder-serial` | The adapter has no unique serial | Keep only one such adapter connected |
| `no-reader` | No removable USB disk or SD card is present | Insert the card into the built-in slot or a USB reader |
| `board-gadget-storage` | A board's exported USB drive is connected | None; it is excluded automatically |
| `reader-without-by-id` | udev created no `usb-*` link | Reconnect the reader directly to the PC, not through a hub |
| `reader-placeholder-serial` | The reader fails the identity check | Use a reader with a unique serial before any flashing |
| `partitions are mounted` | The desktop auto-mounted the card | The flashing task unmounts only after approval of that exact target |
| `does not accept SSH` | The board is off, still booting, or on another network | Check the serial console log for the boot stage and the address |
| `match no recorded fingerprint` | A different board or a reflashed image answered | Confirm the board identity before trusting the session |

## Tests

The mocked suite covers the healthy bench, every refusal above, SSH
fingerprinting without login, and capture through a pseudo-terminal:

```sh
python3 -m unittest -v tests.target.test_sdos_board
```

The fixtures in [`tests/target/fixtures/`](../tests/target/fixtures/) are
synthetic listings, not observations of real hardware.
