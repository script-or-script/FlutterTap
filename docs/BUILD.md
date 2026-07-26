# Building FlutterTap

## Requirements

- Android Studio (or just the Android SDK/NDK + JDK 17+)
- NDK `27.2.12479018` (or newer; set `ndkVersion` in `module/build.gradle.kts`
  if you use a different one)
- CMake `3.22.1` (bundled with the Android SDK)
- Git (the two native dependencies are git submodules)

## First-time setup

```bash
git clone --recurse-submodules <repo-url> FlutterTap
cd FlutterTap
# If you cloned without --recurse-submodules:
git submodule update --init --recursive
```

Create `local.properties` pointing at your SDK if Android Studio didn't do it
for you:

```
sdk.dir=/path/to/Android/Sdk
```

## Building the native module

```bash
./gradlew :module:externalNativeBuildRelease
```

Produces `libfluttertap.so` for `arm64-v8a` and `x86_64` under
`module/build/intermediates/cxx/.../obj/<abi>/`.

## Building the manager app

```bash
./gradlew :manager-app:assembleDebug   # or assembleRelease
```

APK output: `manager-app/build/outputs/apk/`.

## Packaging a flashable module zip

```bash
./scripts/build_module_zip.sh
```

Builds the native module (both ABIs), strips debug symbols with the NDK's
`llvm-strip`, and produces `dist/FlutterTap-<version>.zip` with the layout
Magisk/KernelSU/APatch expect:

```
FlutterTap-<version>.zip
├── module.prop
├── customize.sh
├── uninstall.sh
└── zygisk/
    ├── arm64-v8a.so
    └── x86_64.so
```

## Notes on architecture support

Only `arm64-v8a` and `x86_64` are built. The original Frida script's own
address-resolution logic only has branches for `Process.arch == 'arm64'` and
`'x64'` -- there's no 32-bit path to port in the first place, and essentially
every Android 10+ device is 64-bit, so this isn't a real-world limitation.
`x86_64` covers Android Studio's emulator images.

## Third-party dependencies

Both `capstone` and `dobby` are git submodules under
`module/src/main/cpp/third_party/`. `dobby` is pinned to a specific commit
(not upstream's default branch) because of build breakage in later commits --
see `docs/ARCHITECTURE.md` for the full explanation and the two local patches
applied on top of it.
