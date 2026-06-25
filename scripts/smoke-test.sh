#!/usr/bin/env bash
# Minimal smoke test against a running server (default http://localhost:8080).
# Generates a test image with libvips if `vips` is available, otherwise expects
# a path to an image as $2.
set -euo pipefail

BASE="${1:-http://localhost:8080}"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

if [[ -n "${2:-}" ]]; then
  SRC="$2"
elif command -v vips >/dev/null 2>&1; then
  SRC="$OUT/src.png"
  vips black "$SRC" 640 480 --bands 3
else
  echo "No source image given and 'vips' not installed. Usage: $0 [base_url] [image]" >&2
  exit 1
fi

echo "== healthz =="
curl -fsS "$BASE/healthz"; echo

echo "== version =="
curl -fsS "$BASE/v1/version"; echo

echo "== resize -> webp =="
curl -fsS -X POST --data-binary "@$SRC" \
  "$BASE/v1/process?w=200&format=webp&q=80" -o "$OUT/out.webp"
file "$OUT/out.webp"

echo "== cover thumbnail =="
curl -fsS -X POST --data-binary "@$SRC" \
  "$BASE/v1/process?w=100&h=100&fit=cover" -o "$OUT/thumb.jpg"
file "$OUT/thumb.jpg"

echo "== AVIF encode (-> avif) =="
curl -fsS -X POST --data-binary "@$SRC" \
  "$BASE/v1/process?w=200&format=avif&q=50" -o "$OUT/out.avif"
file "$OUT/out.avif"

echo "== AVIF decode (avif -> png) =="
curl -fsS -X POST --data-binary "@$OUT/out.avif" \
  "$BASE/v1/process?w=100&format=png" -o "$OUT/from_avif.png"
file "$OUT/from_avif.png"

echo "All smoke tests passed."
