"""Read-only network identity: TCP reachability and SSH host-key fingerprints.

The SSH host key is the board's network identity. Recording its fingerprint on
first contact lets later sessions detect a swapped board or a changed image
instead of trusting whatever answers at the address.
"""

from __future__ import annotations

import socket
import subprocess
from dataclasses import asdict, dataclass
from typing import Any, Callable, Sequence

Runner = Callable[[Sequence[str], bytes | None], tuple[int, bytes]]


def run_command(argv: Sequence[str], stdin: bytes | None = None) -> tuple[int, bytes]:
    result = subprocess.run(list(argv), input=stdin, capture_output=True, timeout=30)
    return result.returncode, result.stdout


@dataclass(frozen=True)
class HostKey:
    key_type: str
    fingerprint: str


@dataclass(frozen=True)
class NetworkTarget:
    host: str
    addresses: tuple[str, ...]
    ssh_reachable: bool
    host_keys: tuple[HostKey, ...]

    def to_dict(self) -> dict[str, Any]:
        data = asdict(self)
        data["host_keys"] = [asdict(key) for key in self.host_keys]
        return data


def resolve(host: str) -> tuple[str, ...]:
    try:
        infos = socket.getaddrinfo(host, 22, type=socket.SOCK_STREAM)
    except socket.gaierror:
        return ()
    return tuple(sorted({info[4][0] for info in infos}))


def tcp_reachable(address: str, port: int = 22, timeout: float = 3.0) -> bool:
    try:
        with socket.create_connection((address, port), timeout=timeout):
            return True
    except OSError:
        return False


def ssh_host_keys(host: str, runner: Runner = run_command) -> tuple[HostKey, ...]:
    """Scan host keys and return SHA-256 fingerprints. Never logs in."""
    code, scanned = runner(["ssh-keyscan", "-T", "5", host], None)
    if code != 0 or not scanned.strip():
        return ()
    code, listed = runner(["ssh-keygen", "-l", "-E", "sha256", "-f", "-"], scanned)
    if code != 0:
        return ()
    keys = []
    for line in listed.decode("utf-8", errors="replace").splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[1].startswith("SHA256:"):
            key_type = parts[-1].strip("()")
            keys.append(HostKey(key_type=key_type, fingerprint=parts[1]))
    return tuple(sorted(set(keys), key=lambda key: key.key_type))


def probe(
    host: str,
    resolver: Callable[[str], tuple[str, ...]] = resolve,
    reachable: Callable[[str], bool] = tcp_reachable,
    runner: Runner = run_command,
) -> NetworkTarget:
    addresses = resolver(host)
    ssh_up = any(reachable(address) for address in addresses)
    keys = ssh_host_keys(host, runner) if ssh_up else ()
    return NetworkTarget(host=host, addresses=addresses, ssh_reachable=ssh_up, host_keys=keys)


def check_known_identity(target: NetworkTarget, expected: Sequence[str]) -> list[str]:
    """Compare scanned fingerprints with recorded ones; return problems found."""
    if not target.ssh_reachable:
        return [f"{target.host} does not accept SSH connections on port 22"]
    seen = {key.fingerprint for key in target.host_keys}
    if not seen.intersection(expected):
        return [
            f"{target.host} presents host keys {sorted(seen)} that match no recorded "
            "fingerprint; a different board or a reflashed image answered"
        ]
    return []
