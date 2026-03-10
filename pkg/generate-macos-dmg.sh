#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT_DIR/src"
BUILD_DIR="$SRC_DIR/build-arm64"
OUT_DIR="$SRC_DIR/build/dmg"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1" >&2
    exit 1
  fi
}

require_cmd cmake
require_cmd cpack

mkdir -p "$OUT_DIR"

if [[ -z "${JOBS:-}" ]]; then
  JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

echo "Configuring arm64 build..."
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_OSX_ARCHITECTURES="arm64"

echo "Building SpeedCrunch (arm64, parallel jobs: $JOBS)..."
cmake --build "$BUILD_DIR" --target SpeedCrunch --parallel "$JOBS"

echo "Generating DMG with CPack (DragNDrop)..."
(
  cd "$BUILD_DIR"
  cpack -G DragNDrop
)

DMG_PATH="$BUILD_DIR/SpeedCrunch.dmg"
OUT_PATH="$OUT_DIR/SpeedCrunch.dmg"
if [[ ! -f "$DMG_PATH" ]]; then
  echo "DMG generation failed (missing $DMG_PATH)." >&2
  exit 1
fi

cp -f "$DMG_PATH" "$OUT_PATH"
echo "DMG generated:"
ls -lh "$OUT_PATH"
