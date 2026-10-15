#!/bin/sh
# Packages a release build: sh tools/package.sh windows|linux|macos   (run after `make`)
# Result in dist/: UltimateCraft/ (portable folder), plus a .tar.gz (Linux) or .zip (macOS).
set -e
PLAT=${1:-linux}
VER=$(sed -n 's/^#define UC_VERSION "\(.*\)".*/\1/p' src/core/common.h)
rm -rf dist/UltimateCraft && mkdir -p dist/UltimateCraft
case "$PLAT" in
  windows) cp ultimatecraft.exe dist/UltimateCraft/; [ -f ultimatecraft-console.exe ] && cp ultimatecraft-console.exe dist/UltimateCraft/ || true ;;
  *)       cp ultimatecraft dist/UltimateCraft/ ;;
esac
cp -r res datapacks dist/UltimateCraft/
cp README.md CHANGELOG.md LICENSE EULA.md PRIVACY.md THIRD_PARTY_LICENSES.md dist/UltimateCraft/
cp CONTRIBUTING.md SECURITY.md dist/UltimateCraft/ 2>/dev/null || true
# the sound recordings are CC BY / CC0 / public domain: their credits must travel with the game
mkdir -p dist/UltimateCraft/tools/audio_src && cp tools/audio_src/CREDITS.md dist/UltimateCraft/tools/audio_src/
mkdir -p dist/UltimateCraft/docs && cp docs/*.md dist/UltimateCraft/docs/ 2>/dev/null || true
cp -r docs/images dist/UltimateCraft/docs/ 2>/dev/null || true
cd dist
case "$PLAT" in
  linux) tar czf "UltimateCraft-$VER-linux.tar.gz" UltimateCraft ;;
  macos) zip -qr "UltimateCraft-$VER-macos.zip" UltimateCraft ;;
esac
echo "packaged UltimateCraft $VER for $PLAT in dist/"
