# FlutterTap -- Architecture

_by Eduardo Lopes_

FlutterTap ports `flutter+burp.js` (a Frida script that bypasses Flutter/BoringSSL
TLS pinning and redirects a Flutter app's outgoing connections to a proxy) into a
persistent **Zygisk** module, so the same capability works under Magisk, KernelSU
(with Zygisk/Zygisk Next) or APatch without a tethered `frida-server`.

## Why Zygisk

The original script hooks three things inside the target app's process:
BoringSSL's certificate-chain verification, the Flutter engine's internal
`GetSockAddr` function, and libc's `socket()`. All three only exist *inside* an
app's process once it has loaded `libflutter.so` -- there is no file on disk to
patch. Zygisk is the standard way to run native code inside every (or a
filtered set of) app process on Android without a tethered debugger, which is
why it was chosen over a classic Magisk file-overlay module.

## Component map

| Original script (`flutter+burp.js`) | FlutterTap native module |
|---|---|
| `findAppId()` | `AppSpecializeArgs::nice_name` (read in `preAppSpecialize`) |
| `BURP_PROXY_IP` / `BURP_PROXY_PORT` (hardcoded) | `/data/adb/fluttertap/config.json`, editable from the manager app |
| `parseElf()` | `elf_utils.cpp` (`parse_elf_segments`) |
| `scanMemory()` (AOB scan with `?`-wildcards) | `mem_scan.cpp` (`scan_all_matches`, `compile_pattern`) |
| `Instruction.parse` (Frida/Capstone under the hood) | Capstone, linked directly (`addr_resolver.cpp`) |
| `Interceptor.attach(GetSockAddr)` (onEnter) | `DobbyInstrument` (`hooks.cpp`) |
| `Interceptor.attach(socket)` (onEnter, rewrite sockaddr) | `DobbyHook` on libc's global `socket()` (same process-wide scope as the original) |
| `Interceptor.attach(verify_cert_chain)` (onLeave, force true) | `DobbyHook` with a wrapper that calls the original then forces the return value |

The address-resolution logic (`addr_resolver.cpp`) is a byte-for-byte port of
the script's arm64 (`adrp`/`add` pair) and x64 (`lea reg, [rip+disp]`) paths,
including its quirks (see below), using **Capstone** -- the same disassembly
engine Frida's own `Instruction.parse` is built on -- instead of a hand-rolled
decoder, to avoid subtly misreading an instruction encoding.

## Preserved quirks (intentional, for fidelity)

- **iOS/macOS branch dropped.** The script has parallel Linux/ELF and
  Darwin/Mach-O paths; only Android/ELF is relevant here.
- **32-bit not supported.** `Process.arch` in the original script is only
  handled for `'arm64'` and `'x64'` -- there is no 32-bit ARM/x86 branch at
  all. FlutterTap only ships `arm64-v8a` and `x86_64` for the same reason
  (every Android 10+ device relevant today is 64-bit anyway; see `module/build.gradle.kts`).
- **"last match wins".** `Memory.scan`'s `onMatch` callback keeps overwriting
  the same JS variable for every match found (no early break), so the last
  match in ascending-address order silently wins if a pattern matches more
  than once. `scan_all_matches` returns every match and the resolver iterates
  them the same way, instead of stopping at the first hit.
- **x64 byte-by-byte stepping.** The script's x64 backtraces (`push rbp` /
  `push r15` search, and the `Socket_CreateConnect` -> `GetSockAddr` "2nd call"
  walk) step one byte at a time and just retry on a failed decode, rather than
  advancing by the decoded instruction length. `addr_resolver.cpp` reproduces
  this exactly (`disasmOne` at `addr`, then `addr + 1`, ...) since that's
  what was empirically verified to work against real BoringSSL/Flutter builds.
- **alibaba.com pattern variant.** The script special-cases
  `com.alibaba.intl.android.apps.poseidon`'s slightly different `adrp`/`add`
  byte pattern; `resolveVerifyCertChainArm64` keeps that special case.

## Deliberate improvements (behavior-preserving)

The original script is meant for a single, short-lived, human-supervised Frida
session. FlutterTap runs unattended, for the app's entire lifetime, so a few
things were hardened without changing what actually happens on the wire:

- **Thread-local captured sockaddr** instead of a single global, so concurrent
  connections on different threads can't race with each other (`hooks.cpp`).
- **SIGSEGV/SIGBUS guard** around every heuristic memory read
  (`mem_scan_install_crash_guard`), so a bad scan result logs and gives up
  instead of crashing the host app. The guard chains to whatever handler was
  already installed (Dart VM, crash reporter, ...) for anything that isn't one
  of FlutterTap's own guarded reads.
- **Zero footprint on non-target apps.** `preAppSpecialize` decides whether the
  current process is one of the user-selected target packages *before* doing
  any work, and calls `zygisk::Option::DLCLOSE_MODULE_LIBRARY` to unload
  itself immediately for every app the user didn't select.
- **`socket()` hooked via Dobby directly (`dlsym` + `DobbyHook`), not
  `zygisk::Api::pltHookRegister`.** The Zygisk API object stops working right
  after `post[XXX]Specialize` returns, but `libflutter.so` (and therefore the
  earliest point these addresses can even be resolved) only loads well after
  that, during normal app startup. A background thread polls
  `/proc/self/maps` for `libflutter.so` (mirroring the script's own
  `awaitForCondition`/`setInterval` polling loop) and only then resolves
  addresses and installs hooks -- entirely through Dobby, which has no such
  lifecycle restriction.

## Config file

`/data/adb/fluttertap/config.json` (schema shared with `manager-app`'s
`ConfigData.kt`):

```json
{
  "enabled": true,
  "proxy_ip": "192.168.15.17",
  "proxy_port": 8083,
  "target_packages": ["com.example.app"]
}
```

It lives outside `/data/adb/modules/fluttertap` (the module's own installed
payload) so user settings survive module updates/reinstalls. The native
module can't reliably read files under `/data/adb` directly from
`preAppSpecialize` (zygote's own SELinux domain may not be allowed to), so it
is read through the Zygisk **companion process** (`companion.cpp`), which runs
as unrestricted root, via a tiny length-prefixed protocol over the socket
`Api::connectCompanion()` provides.

## Third-party code and why each patch exists

- **Capstone 5.0.9** (BSD-3), vendored as a git submodule, built with only
  `CAPSTONE_ARM64_SUPPORT`/`CAPSTONE_X86_SUPPORT` enabled. Unmodified.
- **Dobby** (Apache-2.0), vendored as a git submodule pinned to commit
  `e9fe7fb` (2023-04-21) rather than the upstream default branch HEAD.
  Upstream's HEAD (as of this writing, last commit 2024-03-14) has several
  unresolved regressions in its Linux/Android backend (`RuntimeModule.base`
  vs. `.load_address`, `MemRange.start` field vs. `start()` accessor) --
  confirmed independently by upstream's own CI, whose `linux_and_android`
  release job produces no `dobby-android-all.tar.gz` artifact even though the
  workflow defines one. `e9fe7fb` is the last commit before the refactor that
  introduced this inconsistency. Two local patches are applied on top (see
  `git log` inside `module/src/main/cpp/third_party/dobby`):
  1. `logging.h`: `Logger::Shared()` was missing `inline` on its out-of-line
     definition, so every translation unit that included the header emitted
     its own strong symbol, and linking failed with "duplicate symbol"
     wherever `dobby_static` is actually linked into a real binary.
  2. `CMakeLists.txt`: the shared `dobby` target (which nothing here uses --
     only `dobby_static` is linked) fails to link on this NDK/lld combination
     for the same duplicate-symbol reason and is skipped on Android.
- **zygisk.hpp** (0BSD), copied from `topjohnwu/zygisk-module-sample` --
  the canonical public Zygisk API header, explicitly released under a
  public-domain-equivalent license "so you don't have to worry about any
  licensing issues while developing Zygisk modules." This same API is what
  KernelSU and APatch also implement, which is why one header works across
  all three root solutions.

## Manager app

A Jetpack Compose app (`manager-app/`) that:

- Lists launchable apps on the device and lets the user pick which ones
  FlutterTap should hook (checkbox list with search).
- Edits the proxy IP/port and a master enable switch.
- Detects root / the active backend (Magisk/KernelSU/APatch) and whether the
  module is installed, for display only -- this has no effect on whether the
  module itself works, since all three implement the same Zygisk API.
- Writes `config.json` through a root shell (`libsu`), base64-encoded so no
  part of the JSON ever needs shell escaping.
- Ships English, Portuguese (Brazil) and Chinese strings, switchable in-app
  via `AppCompatDelegate.setApplicationLocales` (works without
  `AppCompatActivity`, backed by the `AppLocalesMetadataHolderService` entry
  in `AndroidManifest.xml`, per AndroidX's own documented approach for
  non-AppCompat activities).
- Always derives its color scheme from `isSystemInDarkTheme()` (plus a
  `values-night/themes.xml` for the window background/status bar before
  Compose even loads), so it can't end up showing a light background while
  the system is in dark mode.
