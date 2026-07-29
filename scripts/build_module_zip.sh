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
cp module/template/action.sh "$STAGE_DIR/"
chmod 755 "$STAGE_DIR/action.sh"

# `|| true` on every `find ... | head -1`: under `set -e` with `pipefail`, `head`
# closing the pipe early can SIGPIPE `find` (status 141) even on success, and a
# non-matching glob would abort the script instead of reaching the `-z`/`-n`
# checks these lookups exist to feed.
find_so() {
  local abi="$1"
  find module/build/intermediates/cxx -type f -path "*/obj/${abi}/libfluttertap.so" 2>/dev/null | head -1 || true
}

# NDK location, in order of preference: explicit env vars, then the default SDK
# path for each host OS (Windows/Linux/macOS).
find_ndk_tool() {
  local name="$1" root
  for root in "${ANDROID_NDK_HOME:-}" "${ANDROID_NDK_ROOT:-}" \
              "${ANDROID_SDK_ROOT:-}/ndk" "${ANDROID_HOME:-}/ndk" \
              "$HOME/AppData/Local/Android/Sdk/ndk" \
              "$HOME/Android/Sdk/ndk" \
              "$HOME/Library/Android/sdk/ndk"; do
    [ -n "$root" ] && [ -d "$root" ] || continue
    local hit
    # Depth 8 covers both a versioned SDK ndk/ root and ANDROID_NDK_HOME
    # pointing straight at one NDK version directory.
    hit="$(find "$root" -maxdepth 8 -type f -iname "${name}*" -path "*/llvm/prebuilt/*/bin/*" 2>/dev/null | head -1 || true)"
    if [ -n "$hit" ]; then echo "$hit"; return 0; fi
  done
  return 0
}

STRIP_BIN="$(find_ndk_tool llvm-strip)"
READELF_BIN="$(find_ndk_tool llvm-readelf)"

# Under Git Bash/MSYS the NDK tools are native Windows binaries and cannot read
# MSYS-style paths (/c/...). MSYS normally rewrites arguments automatically, but
# MSYS2_ARG_CONV_EXCL in the caller's environment silently disables that -- which
# made llvm-strip fail with "No such file or directory" on a perfectly valid
# path. Convert explicitly so the script works either way.
host_path() {
  if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else printf '%s' "$1"; fi
}
if [ -z "$READELF_BIN" ]; then
  echo "ERROR: llvm-readelf not found in any NDK; cannot verify the built .so." >&2
  echo "       Set ANDROID_NDK_HOME, or see docs/BUILD.md." >&2
  exit 1
fi

for abi in arm64-v8a x86_64; do
  so_path="$(find_so "$abi")"
  if [ -z "$so_path" ]; then
    echo "ERROR: could not find built libfluttertap.so for $abi" >&2
    exit 1
  fi
  dest="$STAGE_DIR/zygisk/${abi}.so"
  cp "$so_path" "$dest"
  if [ -n "$STRIP_BIN" ]; then
    "$STRIP_BIN" --strip-unneeded "$(host_path "$dest")"
  fi
  echo "  - ${abi}.so <- $so_path ($(du -h "$dest" | cut -f1))"

  # Guard against silently reintroducing thread_local: TLS relocations are what
  # the Zygisk Next Linker's minimal ELF loader is least likely to handle
  # identically to the system linker. Use pthread TSD instead (see the note in
  # mem_scan.cpp).
  if "$READELF_BIN" -r "$(host_path "$dest")" 2>/dev/null | grep -qE "TLSDESC|DTPMOD|TPOFF|TPREL"; then
    echo "ERROR: ${abi}.so contains TLS relocations -- use pthread TSD, not thread_local" >&2
    exit 1
  fi
done

# Apache-2.0 (Dobby) requires the license to accompany binary distributions, and
# Capstone's BSD-3 requires its copyright notice to be reproduced. Both are
# statically linked into the .so files above, so they ship with the zip.
echo "==> Bundling third-party licenses"
mkdir -p "$STAGE_DIR/licenses"
cp module/src/main/cpp/third_party/dobby/LICENSE "$STAGE_DIR/licenses/Dobby-LICENSE-Apache-2.0.txt"
cp module/src/main/cpp/third_party/capstone/LICENSE.TXT "$STAGE_DIR/licenses/Capstone-LICENSE-BSD-3.txt"
cp module/src/main/cpp/third_party/capstone/LICENSE_LLVM.TXT "$STAGE_DIR/licenses/Capstone-LICENSE-LLVM.txt"
cp LICENSE "$STAGE_DIR/licenses/FlutterTap-LICENSE-MIT.txt"

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
