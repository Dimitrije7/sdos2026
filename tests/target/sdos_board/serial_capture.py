"""Loss-resistant, receive-only serial capture with timestamps.

The port is opened read-only, so capture can never type into the board's
console. Every received chunk is appended to ``raw.bin`` and fsynced before the
next read, and every completed line is written to ``console.log`` with UTC and
monotonic timestamps. ``capture.json`` records the stop reason and counts, and
is rewritten on every exit path, including signals.
"""

from __future__ import annotations

import datetime as dt
import json
import os
import re
import select
import signal
import termios
import threading
import time
import tty
from pathlib import Path
from typing import Any

BAUD_RATES = {
    9600: termios.B9600,
    57600: termios.B57600,
    115200: termios.B115200,
}


def _utc() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="microseconds").replace("+00:00", "Z")


def _configure(fd: int, baud: int) -> None:
    if baud not in BAUD_RATES:
        raise ValueError(f"unsupported baud rate {baud}")
    tty.setraw(fd)
    attributes = termios.tcgetattr(fd)
    attributes[4] = attributes[5] = BAUD_RATES[baud]
    attributes[2] |= termios.CLOCAL | termios.CREAD
    termios.tcsetattr(fd, termios.TCSANOW, attributes)


def capture(
    device: str,
    out_dir: Path,
    *,
    identity: str,
    baud: int = 115200,
    seconds: float | None = None,
    until: str | None = None,
    configure: bool = True,
) -> dict[str, Any]:
    """Capture until a time limit, a regex match on a line, or a signal."""
    out_dir.mkdir(parents=True, exist_ok=True)
    pattern = re.compile(until) if until else None
    started_utc = _utc()
    started = time.monotonic()
    stop_reason = "device-closed"
    stopped_by_signal: list[int] = []
    counts = {"bytes": 0, "lines": 0}

    def on_signal(signum: int, _frame: Any) -> None:
        stopped_by_signal.append(signum)

    previous = {}
    if threading.current_thread() is threading.main_thread():
        previous = {sig: signal.signal(sig, on_signal) for sig in (signal.SIGINT, signal.SIGTERM)}
    fd = os.open(device, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    pending = b""
    try:
        if configure:
            _configure(fd, baud)
        with (out_dir / "raw.bin").open("ab") as raw, (out_dir / "console.log").open(
            "a", encoding="utf-8"
        ) as log:

            def emit(line: bytes, partial: bool = False) -> bool:
                text = line.decode("utf-8", errors="replace").rstrip("\r")
                offset = time.monotonic() - started
                marker = " [partial]" if partial else ""
                log.write(f"{_utc()} +{offset:.6f}{marker} {text}\n")
                log.flush()
                counts["lines"] += 1
                return bool(pattern and pattern.search(text))

            while True:
                if stopped_by_signal:
                    stop_reason = f"signal-{stopped_by_signal[0]}"
                    break
                remaining = None
                if seconds is not None:
                    remaining = seconds - (time.monotonic() - started)
                    if remaining <= 0:
                        stop_reason = "time-limit"
                        break
                ready, _, _ = select.select([fd], [], [], min(remaining or 0.5, 0.5))
                if not ready:
                    continue
                try:
                    chunk = os.read(fd, 4096)
                except BlockingIOError:
                    continue
                except OSError:
                    stop_reason = "device-error"
                    break
                if not chunk:
                    break
                raw.write(chunk)
                raw.flush()
                os.fsync(raw.fileno())
                counts["bytes"] += len(chunk)
                pending += chunk
                matched = False
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    matched = emit(line) or matched
                if matched:
                    stop_reason = "pattern-matched"
                    break
            if pending:
                emit(pending, partial=True)
            os.fsync(log.fileno())
    finally:
        os.close(fd)
        for sig, handler in previous.items():
            signal.signal(sig, handler)
        metadata = {
            "identity": identity,
            "baud": baud,
            "started_utc": started_utc,
            "stopped_utc": _utc(),
            "duration_seconds": round(time.monotonic() - started, 6),
            "stop_reason": stop_reason,
            "bytes": counts["bytes"],
            "lines": counts["lines"],
            "until": until,
            "direction": "receive-only",
        }
        (out_dir / "capture.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    return metadata
