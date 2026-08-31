#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SVG_FILE="$ROOT_DIR/gfx/speedcrunch.svg"
RES_DIR="$ROOT_DIR/src/resources"
DOC_DIR="$ROOT_DIR/doc/src"

PNG_OUT="$RES_DIR/speedcrunch.png"
DOC_LOGO_OUT="$DOC_DIR/logo.png"
ICO_OUT="$RES_DIR/speedcrunch.ico"
ICNS_OUT="$RES_DIR/speedcrunch.icns"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1" >&2
    exit 1
  fi
}

require_cmd rsvg-convert
require_cmd sips
require_cmd magick
require_cmd xcrun

if ! xcrun --find actool >/dev/null 2>&1; then
  echo "Missing required Xcode tool: actool" >&2
  exit 1
fi

if [[ ! -f "$SVG_FILE" ]]; then
  echo "SVG not found: $SVG_FILE" >&2
  exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

MASTER_PNG="$TMP_DIR/speedcrunch-1024.png"

echo "Rendering SVG -> 1024x1024 PNG..."
rsvg-convert -w 1024 -h 1024 "$SVG_FILE" -o "$MASTER_PNG"

echo "Generating Linux PNG (256x256)..."
sips -z 256 256 "$MASTER_PNG" --out "$PNG_OUT" >/dev/null
cp "$PNG_OUT" "$DOC_LOGO_OUT"

echo "Generating Windows ICO (16..256)..."
magick "$MASTER_PNG" -background none \
  -define icon:auto-resize=16,24,32,48,64,128,256 \
  "$ICO_OUT"

echo "Generating macOS ICNS..."
ASSET_CATALOG_DIR="$TMP_DIR/SpeedCrunch.xcassets"
APP_ICONSET_DIR="$ASSET_CATALOG_DIR/speedcrunch.appiconset"
ACTOOL_OUT_DIR="$TMP_DIR/actool-output"
mkdir -p "$APP_ICONSET_DIR" "$ACTOOL_OUT_DIR"

for s in 16 32 128 256 512; do
  retina_s=$((s * 2))
  sips -z "$s" "$s" "$MASTER_PNG" \
    --out "$APP_ICONSET_DIR/icon_${s}x${s}.png" >/dev/null
  sips -z "$retina_s" "$retina_s" "$MASTER_PNG" \
    --out "$APP_ICONSET_DIR/icon_${s}x${s}@2x.png" >/dev/null
done

cat > "$APP_ICONSET_DIR/Contents.json" <<'EOF'
{
  "images" : [
    { "filename" : "icon_16x16.png",       "idiom" : "mac", "scale" : "1x", "size" : "16x16" },
    { "filename" : "icon_16x16@2x.png",    "idiom" : "mac", "scale" : "2x", "size" : "16x16" },
    { "filename" : "icon_32x32.png",       "idiom" : "mac", "scale" : "1x", "size" : "32x32" },
    { "filename" : "icon_32x32@2x.png",    "idiom" : "mac", "scale" : "2x", "size" : "32x32" },
    { "filename" : "icon_128x128.png",     "idiom" : "mac", "scale" : "1x", "size" : "128x128" },
    { "filename" : "icon_128x128@2x.png",  "idiom" : "mac", "scale" : "2x", "size" : "128x128" },
    { "filename" : "icon_256x256.png",     "idiom" : "mac", "scale" : "1x", "size" : "256x256" },
    { "filename" : "icon_256x256@2x.png",  "idiom" : "mac", "scale" : "2x", "size" : "256x256" },
    { "filename" : "icon_512x512.png",     "idiom" : "mac", "scale" : "1x", "size" : "512x512" },
    { "filename" : "icon_512x512@2x.png",  "idiom" : "mac", "scale" : "2x", "size" : "512x512" }
  ],
  "info" : { "author" : "xcode", "version" : 1 }
}
EOF

xcrun actool \
  --compile "$ACTOOL_OUT_DIR" \
  --platform macosx \
  --minimum-deployment-target 10.14 \
  --app-icon speedcrunch \
  --output-partial-info-plist "$TMP_DIR/actool-info.plist" \
  "$ASSET_CATALOG_DIR" >/dev/null
cp "$ACTOOL_OUT_DIR/speedcrunch.icns" "$ICNS_OUT"

echo "Done."
file "$PNG_OUT" "$DOC_LOGO_OUT" "$ICO_OUT" "$ICNS_OUT"
