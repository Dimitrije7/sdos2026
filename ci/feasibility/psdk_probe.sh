#!/usr/bin/env bash
# Task 005 probe: stream the pinned TI Processor SDK RTOS archive, extract only
# the folders course firmware needs, verify the archive hash, and report sizes
# and timings as JSON. Extracted files are deleted on exit, and a hash mismatch
# fails the probe after the stream ends.
set -euo pipefail

usage() {
  echo "usage: $0 <summary.json>" >&2
  exit 2
}
[[ $# -eq 1 ]] || usage
summary=$1
repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
input_id=ti.psdk-rtos-j721e-11.02.01.03

pin() { python3 "$repo_root/ci/manifest_pin.py" "$input_id" "$1"; }
url=$(pin url)
sha=$(pin sha256)
version=$(pin version)

work=$(mktemp -d "${RUNNER_TEMP:-/tmp}/sdos-psdk-XXXXXX")
trap 'rm -rf "$work"' EXIT
avail_before=$(df --output=avail -B1 "$work" | tail -n 1)

# Course firmware needs the PDK, the DSP libraries, and the SDK's build scripts.
patterns=('*/pdk_jacinto_*' '*/dsplib_*' '*/fftlib_*' '*/mmalib_*' '*/psdk_rtos/*')

start=$(date +%s)
set +e
curl --fail --silent --show-error --location --retry 3 "$url" |
  tee >(sha256sum | cut -d ' ' -f 1 >"$work/stream.sha256") |
  /usr/bin/time -f '%M' -o "$work/extract.rss" \
    tar -xzf - -C "$work" --wildcards "${patterns[@]}" 2>"$work/tar.err"
statuses=("${PIPESTATUS[@]}")
set -e
stream_s=$(($(date +%s) - start))
if [[ ${statuses[0]} -ne 0 || ${statuses[1]} -ne 0 ]]; then
  echo "SDK download failed with curl status ${statuses[0]}" >&2
  exit 1
fi
# tar exits 2 when a pattern matched nothing; report those patterns instead.
unmatched=$(sed -n 's/^tar: \(.*\): Not found in archive$/\1/p' "$work/tar.err")
if [[ ${statuses[2]} -ne 0 && -z $unmatched ]]; then
  echo "SDK extraction failed:" >&2
  tail -n 20 "$work/tar.err" >&2
  exit 1
fi
# Wait for the process substitution to finish writing the digest.
for _ in $(seq 1 30); do
  [[ -s $work/stream.sha256 ]] && break
  sleep 1
done
actual=$(cat "$work/stream.sha256")
if [[ $actual != "$sha" ]]; then
  echo "SHA-256 mismatch for the SDK stream; extracted files are discarded" >&2
  exit 1
fi

extracted_bytes=$(du -sb --exclude=stream.sha256 --exclude=extract.rss --exclude=tar.err "$work" | cut -f 1)
avail_after=$(df --output=avail -B1 "$work" | tail -n 1)
folders=$(find "$work" -mindepth 2 -maxdepth 2 -type d -printf '%f\n' | sort)

INPUT_ID=$input_id VERSION=$version STREAM_S=$stream_s EXTRACTED_BYTES=$extracted_bytes \
  EXTRACT_RSS_KB=$(cat "$work/extract.rss") FOLDERS=$folders UNMATCHED=$unmatched \
  AVAIL_BEFORE=$avail_before AVAIL_AFTER=$avail_after \
  python3 - "$summary" <<'EOF'
import json, os, sys
env = os.environ
summary = {
    "probe": "psdk-stream-extract",
    "manifest_input": env["INPUT_ID"],
    "version": env["VERSION"],
    "archive_sha256_verified": True,
    "stream_and_extract_seconds": int(env["STREAM_S"]),
    "extracted_bytes": int(env["EXTRACTED_BYTES"]),
    "extract_peak_rss_kib": int(env["EXTRACT_RSS_KB"]),
    "extracted_folders": [f for f in env["FOLDERS"].splitlines() if f],
    "unmatched_patterns": [p for p in env["UNMATCHED"].splitlines() if p],
    "disk_available_before_bytes": int(env["AVAIL_BEFORE"]),
    "disk_available_after_bytes": int(env["AVAIL_AFTER"]),
    "runner_image": env.get("ImageOS", "") + " " + env.get("ImageVersion", ""),
}
with open(sys.argv[1], "w", encoding="utf-8") as handle:
    json.dump(summary, handle, indent=2)
    handle.write("\n")
print(json.dumps(summary, indent=2))
EOF
