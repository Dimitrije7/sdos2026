#!/usr/bin/env bash
# Task 005 probe: on a GitHub-hosted arm64 runner, fetch and verify the pinned
# BeagleBone AI-64 image, expand it sparsely with its block map, record static
# kernel/DTB/firmware facts, and install a trivial package through chroot to
# prove the "SDOS packages over the official image" approach. Requires sudo.
# All image files are deleted on exit; only a JSON summary remains.
set -euo pipefail

usage() {
  echo "usage: $0 <summary.json>" >&2
  exit 2
}
[[ $# -eq 1 ]] || usage
summary=$1
repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
image_id=beagle.bbai64-debian-13.7-iot-2026-10-09
bmap_id=beagle.bbai64-iot-2026-10-09-bmap

if [[ $(uname -m) != aarch64 ]]; then
  echo "run on an arm64 runner so the chroot executes natively" >&2
  exit 2
fi

pin() { python3 "$repo_root/ci/manifest_pin.py" "$1" "$2"; }
work=$(mktemp -d "${RUNNER_TEMP:-/tmp}/sdos-image-XXXXXX")
root=$work/root
loop=""
cleanup() {
  set +e
  for mount_point in "$root/proc" "$root/sys" "$root/dev" "$root"; do
    mountpoint -q "$mount_point" && sudo umount "$mount_point"
  done
  [[ -n $loop ]] && sudo losetup --detach "$loop"
  rm -rf "$work"
}
trap cleanup EXIT

stage_avail() { df --output=avail -B1 "$work" | tail -n 1; }
avail_start=$(stage_avail)

start=$(date +%s)
for id in "$image_id" "$bmap_id"; do
  curl --fail --silent --show-error --location --retry 3 \
    --output "$work/$(pin "$id" file_name)" "$(pin "$id" url)"
  echo "$(pin "$id" sha256)  $work/$(pin "$id" file_name)" | sha256sum --check --status ||
    {
      echo "SHA-256 mismatch for $id" >&2
      exit 1
    }
done
download_s=$(($(date +%s) - start))
avail_downloaded=$(stage_avail)

image_xz=$work/$(pin "$image_id" file_name)
start=$(date +%s)
bmaptool copy --bmap "$work/$(pin "$bmap_id" file_name)" "$image_xz" "$work/image.img"
expand_s=$(($(date +%s) - start))
rm -f "$image_xz"
allocated_bytes=$(du -B1 "$work/image.img" | cut -f 1)
avail_expanded=$(stage_avail)

loop=$(sudo losetup --find --show --partscan "$work/image.img")
sudo udevadm settle
rootfs_part=$(lsblk -lnpo NAME,FSTYPE,SIZE --bytes "$loop" |
  awk '$2 == "ext4" {print $3, $1}' | sort -n | tail -n 1 | cut -d ' ' -f 2)
mkdir -p "$root"
sudo mount -o ro "$rootfs_part" "$root"

facts=$(sudo python3 - "$root" <<'EOF'
import glob, hashlib, json, os, subprocess, sys
root = sys.argv[1]
def sha(path):
    with open(path, "rb") as handle:
        return hashlib.sha256(handle.read()).hexdigest()
def resolve_in_root(path):
    """Follow symlinks without leaving the mounted image."""
    for _ in range(16):
        if not os.path.islink(path):
            return path
        target = os.readlink(path)
        if target.startswith("/"):
            path = os.path.join(root, target.lstrip("/"))
        else:
            path = os.path.normpath(os.path.join(os.path.dirname(path), target))
    return path
os_release = {}
with open(os.path.join(root, "etc/os-release")) as handle:
    for line in handle:
        if "=" in line:
            key, value = line.rstrip().split("=", 1)
            os_release[key] = value.strip('"')
dtbs = sorted(glob.glob(os.path.join(root, "boot/dtbs/*/ti/k3-j721e-beagleboneai64*.dtb")))
firmware = []
for path in sorted(glob.glob(os.path.join(root, "lib/firmware/j7*"))):
    real = resolve_in_root(path)
    entry = {"name": os.path.basename(path)}
    if os.path.islink(path):
        entry["link_target"] = os.readlink(path)
    if os.path.isfile(real):
        entry["sha256"] = sha(real)
        entry["bytes"] = os.path.getsize(real)
    firmware.append(entry)
remoteproc = []
if dtbs:
    dts = subprocess.run(["dtc", "-q", "-I", "dtb", "-O", "dts", dtbs[0]],
                         capture_output=True, text=True, check=True).stdout
    remoteproc = sorted({line.strip() for line in dts.splitlines()
                         if "firmware-name" in line and ("c66" in line or "c71" in line)})
print(json.dumps({
    "os_pretty_name": os_release.get("PRETTY_NAME"),
    "kernels": sorted(os.path.basename(p) for p in glob.glob(os.path.join(root, "lib/modules/*"))),
    "dtbs": [{"path": os.path.relpath(p, root), "sha256": sha(p)} for p in dtbs],
    "dsp_firmware_files": firmware,
    "dtb_dsp_firmware_names": remoteproc,
}))
EOF
)

# Prove the delta approach: install a trivial package into the image rootfs.
sudo umount "$root"
sudo mount "$rootfs_part" "$root"
pkg=$work/pkg
mkdir -p "$pkg/DEBIAN" "$pkg/usr/share/sdos"
printf 'Task 005 feasibility probe\n' >"$pkg/usr/share/sdos/feasibility.txt"
cat >"$pkg/DEBIAN/control" <<'EOF'
Package: sdos-feasibility-probe
Version: 0.0.1
Architecture: all
Maintainer: SDOS project <noreply@example.invalid>
Description: Task 005 delta-install feasibility probe
EOF
dpkg-deb --root-owner-group --build "$pkg" "$work/probe.deb" >/dev/null
sudo cp "$work/probe.deb" "$root/tmp/probe.deb"
for fs in proc sys dev; do sudo mount --bind "/$fs" "$root/$fs"; done
start=$(date +%s)
sudo chroot "$root" dpkg -i /tmp/probe.deb >/dev/null
installed_version=$(sudo chroot "$root" dpkg-query -W -f '${Version}' sdos-feasibility-probe)
chroot_s=$(($(date +%s) - start))
avail_peak=$(stage_avail)

FACTS=$facts DOWNLOAD_S=$download_s EXPAND_S=$expand_s CHROOT_S=$chroot_s \
  ALLOCATED=$allocated_bytes INSTALLED_VERSION=$installed_version \
  AVAIL_START=$avail_start AVAIL_DOWNLOADED=$avail_downloaded \
  AVAIL_EXPANDED=$avail_expanded AVAIL_PEAK=$avail_peak \
  python3 - "$summary" <<'EOF'
import json, os, sys
env = os.environ
summary = {
    "probe": "image-delta",
    "manifest_input": "beagle.bbai64-debian-13.7-iot-2026-10-09",
    "download_and_verify_seconds": int(env["DOWNLOAD_S"]),
    "bmap_expand_seconds": int(env["EXPAND_S"]),
    "expanded_allocated_bytes": int(env["ALLOCATED"]),
    "chroot_package_install_seconds": int(env["CHROOT_S"]),
    "probe_package_installed_version": env["INSTALLED_VERSION"],
    "disk_available_bytes": {
        "start": int(env["AVAIL_START"]),
        "after_download": int(env["AVAIL_DOWNLOADED"]),
        "after_expand": int(env["AVAIL_EXPANDED"]),
        "after_install": int(env["AVAIL_PEAK"]),
    },
    "static_image_facts": json.loads(env["FACTS"]),
    "evidence_class": "image-static, not booted-board evidence",
    "runner_image": env.get("ImageOS", "") + " " + env.get("ImageVersion", ""),
}
with open(sys.argv[1], "w", encoding="utf-8") as handle:
    json.dump(summary, handle, indent=2)
    handle.write("\n")
print(json.dumps(summary, indent=2))
EOF
