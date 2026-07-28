# Building FlutterTap

## Requirements

- Android Studio (or just the Android SDK/NDK + a JDK, see below)
- **JDK 17 or 21.** Not newer: Gradle 8.11.1 cannot parse the version string of
  JDK 22+ and fails with `IllegalArgumentException: <version>` before compiling
  anything. If your system default `java` is newer, point Gradle at Android
  Studio's bundled JBR, e.g.
  `export JAVA_HOME="C:/Program Files/Android/Android Studio/jbr"`.
- NDK `27.2.12479018` (or newer; set `ndkVersion` in `module/build.gradle.kts`
  if you use a different one). Both `llvm-strip` and `llvm-readelf` from the
  NDK toolchain are used by the packaging script.
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
├── action.sh
├── licenses/
│   ├── FlutterTap-LICENSE-MIT.txt
│   ├── Dobby-LICENSE-Apache-2.0.txt
│   ├── Capstone-LICENSE-BSD-3.txt
│   └── Capstone-LICENSE-LLVM.txt
└── zygisk/
    ├── arm64-v8a.so
    └── x86_64.so
```

The script also **fails the build if `llvm-readelf` finds any TLS relocation**
in either `.so`. That is a guard against reintroducing `thread_local`, which
the Zygisk Next Linker handles differently from the system linker -- see
`docs/ARCHITECTURE.md`. Use pthread thread-specific data instead.

`licenses/` is shipped because Dobby (Apache-2.0) and Capstone (BSD-3) are
statically linked into the `.so` files, and both licenses require their terms
to accompany binary redistribution. See [`THIRD_PARTY.md`](../THIRD_PARTY.md).

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
