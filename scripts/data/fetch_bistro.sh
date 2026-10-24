#!/usr/bin/env bash
# Resumable download of Amazon Lumberyard Bistro (NVIDIA ORCA, CC BY 4.0).
# The download URL carries a short-lived signature, so it is re-resolved from
# the /bistro entry point on every attempt and never stored.
# Usage: fetch_bistro.sh [--verify-only]
set -euo pipefail

DATA_ROOT="${ECL_DATA:-$HOME/data/engine-core-labs}"
DEST_DIR="$DATA_ROOT/bistro"
ZIP="$DEST_DIR/Bistro_v5_2.zip"
EXPECTED_SIZE=894377473
# Pinned on 2026-10-23 after the first complete download (phase 1).
EXPECTED_SHA256="${BISTRO_SHA256:-0d50e3c724c6c5da19f8eb99ad3f53e36fec37ffa2df9621f9ccf0603f3934e1}"

mkdir -p "$DEST_DIR"

resolve_url() {
  curl -sS -o /dev/null -L --max-time 60 -r 0-0 -w '%{url_effective}' https://developer.nvidia.com/bistro
}

size_of() { stat -c %s "$1" 2>/dev/null || echo 0; }

if [[ "${1:-}" != "--verify-only" ]]; then
  attempt=0
  while (( $(size_of "$ZIP") < EXPECTED_SIZE )); do
    attempt=$((attempt + 1))
    (( attempt > 50 )) && { echo "giving up after 50 attempts" >&2; exit 1; }
    url="$(resolve_url)"
    echo "$(date -Is) attempt $attempt, have $(size_of "$ZIP") bytes"
    curl -sS -C - --max-time 1800 --speed-limit 1024 --speed-time 120 -o "$ZIP" "$url" || sleep 10
  done
fi

actual_size=$(size_of "$ZIP")
if (( actual_size != EXPECTED_SIZE )); then
  echo "size mismatch: $actual_size != $EXPECTED_SIZE" >&2; exit 1
fi
unzip -tq "$ZIP" >/dev/null || { echo "zip CRC check failed" >&2; exit 1; }
sha=$(sha256sum "$ZIP" | cut -d' ' -f1)
if [[ -n "$EXPECTED_SHA256" && "$sha" != "$EXPECTED_SHA256" ]]; then
  echo "sha256 mismatch: $sha" >&2; exit 1
fi
echo "$sha  Bistro_v5_2.zip" > "$DEST_DIR/SHA256SUMS"
unzip -Z1 "$ZIP" > "$DEST_DIR/manifest.txt"
echo "ok size=$actual_size sha256=$sha entries=$(wc -l < "$DEST_DIR/manifest.txt")"
