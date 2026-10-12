#!/bin/bash
# Builds MotorCycles.app (universal: Apple silicon + Intel) and a zip for release.
# Usage: patcher/scripts/build-app.sh VERSION   (run from anywhere)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:?version}"
OUT=dist; APP="$OUT/MotorCycles.app"
rm -rf "$OUT"; mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
swift build -c release --arch arm64 --arch x86_64 --product MotorCycles
cp "$(swift build -c release --arch arm64 --arch x86_64 --show-bin-path)/MotorCycles" "$APP/Contents/MacOS/MotorCycles"
cp Resources/motorcycles.mcpatch "$APP/Contents/Resources/"
# Icon
TMP=$(mktemp -d); swift scripts/render-svg.swift ../docs/logo/motorcycles-appicon.svg "$TMP/icon.png" 1
mkdir "$TMP/AppIcon.iconset"
for s in 16 32 128 256 512; do
  sips -z $s $s "$TMP/icon.png" --out "$TMP/AppIcon.iconset/icon_${s}x${s}.png" >/dev/null
  sips -z $((s*2)) $((s*2)) "$TMP/icon.png" --out "$TMP/AppIcon.iconset/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$TMP/AppIcon.iconset" -o "$APP/Contents/Resources/AppIcon.icns"
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
 <key>CFBundleName</key><string>Motor:Cycles</string>
 <key>CFBundleDisplayName</key><string>Motor:Cycles</string>
 <key>CFBundleExecutable</key><string>MotorCycles</string>
 <key>CFBundleIdentifier</key><string>io.github.nzimas.motorcycles</string>
 <key>CFBundlePackageType</key><string>APPL</string>
 <key>CFBundleShortVersionString</key><string>${VERSION}</string>
 <key>CFBundleVersion</key><string>${VERSION}</string>
 <key>CFBundleIconFile</key><string>AppIcon</string>
 <key>LSMinimumSystemVersion</key><string>13.0</string>
 <key>NSHighResolutionCapable</key><true/>
 <key>NSHumanReadableCopyright</key><string>MIT License. Not affiliated with Elektron.</string>
</dict></plist>
PLIST
codesign --force --deep --sign - "$APP"
(cd "$OUT" && ditto -c -k --keepParent MotorCycles.app "MotorCycles-${VERSION}-macOS.zip")
echo "Built $APP and $OUT/MotorCycles-${VERSION}-macOS.zip"
