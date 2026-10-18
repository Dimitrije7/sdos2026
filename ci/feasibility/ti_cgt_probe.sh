#!/usr/bin/env bash
# Task 005 probe: install one pinned TI compiler unattended on a GitHub-hosted
# runner, compile a Q15 FIR kernel, and report sizes and timings as JSON.
# The installer and installed tree are deleted on exit; no TI payload, license
# text, or log content is written to the summary.
set -euo pipefail

usage() {
  echo "usage: $0 <manifest-input-id> <c6000|c7000> <summary.json>" >&2
  exit 2
}
[[ $# -eq 3 ]] || usage
input_id=$1
family=$2
summary=$3
repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)

case "$family" in
  c6000) compiler=cl6x target_flags=(-mv6600 --abi=eabi) ;;
  c7000) compiler=cl7x target_flags=(-mv7100) ;;
  *) usage ;;
esac

pin() { python3 "$repo_root/ci/manifest_pin.py" "$input_id" "$1"; }
url=$(pin url)
file=$(pin file_name)
sha=$(pin sha256)
version=$(pin version)

work=$(mktemp -d "${RUNNER_TEMP:-/tmp}/sdos-ti-XXXXXX")
trap 'rm -rf "$work"' EXIT
avail_before=$(df --output=avail -B1 "$work" | tail -n 1)

start=$(date +%s)
curl --fail --silent --show-error --location --retry 3 --output "$work/$file" "$url"
download_s=$(($(date +%s) - start))
download_bytes=$(stat -c %s "$work/$file")
if ! echo "$sha  $work/$file" | sha256sum --check --status; then
  echo "SHA-256 mismatch for $file; refusing to install" >&2
  exit 1
fi

chmod +x "$work/$file"
start=$(date +%s)
if ! /usr/bin/time -f '%M' -o "$work/install.rss" \
  "$work/$file" --mode unattended --prefix "$work/install" >"$work/install.log" 2>&1; then
  echo "unattended installation failed; last installer lines follow" >&2
  tail -n 20 "$work/install.log" >&2
  exit 1
fi
install_s=$(($(date +%s) - start))
rm -f "$work/$file"

cc=$(find "$work/install" -type f -name "$compiler" -perm -u+x | head -n 1)
if [[ -z $cc ]]; then
  echo "$compiler not found after installation" >&2
  exit 1
fi
tool_root=$(dirname "$(dirname "$cc")")
compiler_revision=$("$cc" --compiler_revision 2>&1 | head -n 1)

cat >"$work/fir_q15.c" <<'EOF'
#include <stdint.h>

void fir_q15(const int16_t *restrict x, const int16_t *restrict h,
             int16_t *restrict y, int32_t n, int32_t taps)
{
    for (int32_t i = 0; i < n; ++i) {
        int32_t acc = 0;
        for (int32_t k = 0; k < taps; ++k) {
            acc += (int32_t)x[i + k] * (int32_t)h[k];
        }
        y[i] = (int16_t)(acc >> 15);
    }
}
EOF

start=$(date +%s)
/usr/bin/time -f '%M' -o "$work/compile.rss" \
  "$cc" "${target_flags[@]}" -O3 --c99 -I"$tool_root/include" \
  --compile_only --output_file="$work/fir_q15.obj" "$work/fir_q15.c"
compile_s=$(($(date +%s) - start))

installed_bytes=$(du -sb "$work/install" | cut -f 1)
avail_after=$(df --output=avail -B1 "$work" | tail -n 1)
license_files=$(cd "$work/install" &&
  find . -maxdepth 4 -type f -iregex '.*\(licen[cs]e\|manifest\|copying\|tspa\).*' |
  sort | head -n 50)

FAMILY=$family INPUT_ID=$input_id VERSION=$version DOWNLOAD_BYTES=$download_bytes \
  DOWNLOAD_S=$download_s INSTALL_S=$install_s COMPILE_S=$compile_s \
  INSTALLED_BYTES=$installed_bytes INSTALL_RSS_KB=$(cat "$work/install.rss") \
  COMPILE_RSS_KB=$(cat "$work/compile.rss") OBJECT_BYTES=$(stat -c %s "$work/fir_q15.obj") \
  COMPILER_REVISION=$compiler_revision LICENSE_FILES=$license_files \
  AVAIL_BEFORE=$avail_before AVAIL_AFTER=$avail_after \
  python3 - "$summary" <<'EOF'
import json, os, sys
env = os.environ
summary = {
    "probe": "ti-cgt",
    "family": env["FAMILY"],
    "manifest_input": env["INPUT_ID"],
    "version": env["VERSION"],
    "compiler_revision": env["COMPILER_REVISION"],
    "download_bytes": int(env["DOWNLOAD_BYTES"]),
    "download_seconds": int(env["DOWNLOAD_S"]),
    "unattended_install_seconds": int(env["INSTALL_S"]),
    "installed_bytes": int(env["INSTALLED_BYTES"]),
    "install_peak_rss_kib": int(env["INSTALL_RSS_KB"]),
    "compile_seconds": int(env["COMPILE_S"]),
    "compile_peak_rss_kib": int(env["COMPILE_RSS_KB"]),
    "fir_object_bytes": int(env["OBJECT_BYTES"]),
    "disk_available_before_bytes": int(env["AVAIL_BEFORE"]),
    "disk_available_after_bytes": int(env["AVAIL_AFTER"]),
    "license_file_paths": [p for p in env["LICENSE_FILES"].splitlines() if p],
    "runner_image": env.get("ImageOS", "") + " " + env.get("ImageVersion", ""),
}
with open(sys.argv[1], "w", encoding="utf-8") as handle:
    json.dump(summary, handle, indent=2)
    handle.write("\n")
print(json.dumps(summary, indent=2))
EOF
