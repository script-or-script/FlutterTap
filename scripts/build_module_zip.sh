#!/usr/bin/env bash
# Builds the native module for arm64-v8a + x86_64 and packages a flashable
# Magisk/KernelSU/APatch zip in dist/. by Eduardo Lopes
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

VERSION=$(grep '^version=' module/template/module.prop | cut -d= -f2)
DIST_DIR="$ROOT_DIR/dist"
STAGE_DIR="$DIST_DIR/stage"

echo "==> Building native module (arm64-v8a, x86_64)"
./gradlew :module:externalNativeBuildRelease

echo "==> Staging module files"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/zygisk"

cp module/template/module.prop "$STAGE_DIR/"
cp module/template/customize.sh "$STAGE_DIR/"
cp module/template/uninstall.sh "$STAGE_DIR/"

find_so() {
  local abi="$1"
  find module/build/intermediates/cxx -type f -path "*/obj/${abi}/libfluttertap.so" | head -1
}

STRIP_BIN="$(find "${ANDROID_NDK_HOME:-$HOME/AppData/Local/Android/Sdk/ndk}"/*/toolchains/llvm/prebuilt/*/bin -maxdepth 1 -iname "llvm-strip*" 2>/dev/null | head -1)"

for abi in arm64-v8a x86_64; do
  so_path="$(find_so "$abi")"
  if [ -z "$so_path" ]; then
    echo "ERROR: could not find built libfluttertap.so for $abi" >&2
    exit 1
  fi
  dest="$STAGE_DIR/zygisk/${abi}.so"
  cp "$so_path" "$dest"
  if [ -n "$STRIP_BIN" ]; then
    "$STRIP_BIN" --strip-unneeded "$dest"
  fi
  echo "  - ${abi}.so <- $so_path ($(du -h "$dest" | cut -f1))"
done

mkdir -p "$DIST_DIR"
ZIP_PATH="$DIST_DIR/FlutterTap-${VERSION}.zip"
rm -f "$ZIP_PATH"

if command -v zip >/dev/null 2>&1; then
  (cd "$STAGE_DIR" && zip -r -X "$ZIP_PATH" . -x ".*")
else
  # Git Bash on Windows usually doesn't ship `zip`; fall back to PowerShell.
  STAGE_DIR_WIN="$(cygpath -w "$STAGE_DIR" 2>/dev/null || echo "$STAGE_DIR")"
  ZIP_PATH_WIN="$(cygpath -w "$ZIP_PATH" 2>/dev/null || echo "$ZIP_PATH")"
  powershell.exe -NoProfile -Command "Compress-Archive -Path '${STAGE_DIR_WIN}\\*' -DestinationPath '${ZIP_PATH_WIN}' -Force"
fi

echo "==> Done: $ZIP_PATH"
