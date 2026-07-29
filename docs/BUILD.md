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

### Two things a fresh clone hits immediately

**Point Gradle at the SDK.** `local.properties` is gitignored (it holds a machine-specific
path), so a clone has none and the build stops with *"SDK location not found"*. Either export
`ANDROID_HOME=/path/to/Android/Sdk` or create `local.properties` with `sdk.dir=/path/to/Android/Sdk`.

**On Windows, clone into a short path.** Capstone has deeply nested directories
(`suite/synctools/tablegen/...`) and Windows' 260-character limit is easy to exceed: cloning into
something like `C:\Users\you\Documents\projects\...` fails with *"Filename too long"* while the
submodules are being fetched. Clone into e.g. `C:\dev\fluttertap`, or enable long paths with
`git config --global core.longpaths true`.

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

### Release signing

`assembleRelease` is signed only if the release keystore is configured; otherwise it produces an
**unsigned** APK, which Android will refuse to install. That is deliberate — a clone should build
without needing the maintainer's private key.

Credentials are read from `gradle.properties` in your `GRADLE_USER_HOME` (`~/.gradle/gradle.properties`),
never from this repository:

```properties
FLUTTERTAP_STORE_FILE=/absolute/path/to/your-release.jks
FLUTTERTAP_STORE_PASSWORD=...
FLUTTERTAP_KEY_ALIAS=fluttertap
FLUTTERTAP_KEY_PASSWORD=...
```

To create a keystore:

```bash
keytool -genkeypair -keystore your-release.jks -alias fluttertap \
        -keyalg RSA -keysize 4096 -validity 10000 -dname "CN=Your Name"
```

Keep the `.jks` **outside the repository** (`.gitignore` blocks `*.jks`/`*.keystore`, but the safest
place is simply elsewhere on disk), and back up both the keystore and its password. Losing them means
never being able to ship an update Android accepts as the same app — the only way out is changing the
`applicationId`.

Verify what you built:

```bash
apksigner verify --verbose --print-certs manager-app/build/outputs/apk/release/manager-app-release.apk
```

Expect `Verifies` with **v3 true and v1/v2 false**. That is correct, not a defect: v1 (JAR signing)
only matters below API 24 and v2 below API 28, so AGP skips both for `minSdk 29`.

Note that a release-signed APK cannot be installed over a debug-signed one — Android rejects the
signature change. Uninstall the old build first. Configuration in `/data/adb/fluttertap/` survives the
uninstall, so target apps and proxy settings are preserved.

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
