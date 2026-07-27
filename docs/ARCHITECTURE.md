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

## Real-device findings that changed the original script's approach

Testing against real, unrelated apps on physical hardware surfaced three
compatibility gaps in the address-resolution approach the original script
uses. All three are things that would also affect the original Frida script
if pointed at the same binaries -- they're not FlutterTap-specific bugs, but
FlutterTap fixes them since it has to work unattended, without a human
adjusting the script per target.

- **Libraries mapped directly out of the APK.** Modern Android/AGP defaults
  to `extractNativeLibs="false"`, which keeps native libraries compressed-off
  inside the APK and mmaps them directly from the zip rather than extracting
  them to their own file. `/proc/self/maps` then shows the mapping's path as
  the APK itself (e.g. `.../base.apk`), not `.../libflutter.so` -- so the
  original suffix-matching approach to "find libflutter.so" fails outright.
  `find_module_by_suffix` (`elf_utils.cpp`) uses `dl_iterate_phdr` instead:
  bionic reports the correct `dlpi_addr`/`dlpi_name` (as something like
  `.../base.apk!/lib/arm64-v8a/libflutter.so`) regardless of which loading
  method was used.
- **Merged rodata+text PT_LOAD segment.** The original `parseElf()` (and this
  port, initially) assumed the classic ELF layout: a rodata-only `PT_LOAD` at
  `p_vaddr == 0`, and a separate executable `PT_LOAD` at a nonzero vaddr.
  Some `libflutter.so` builds (linked with newer lld defaults, e.g.
  `--no-rosegment`) instead merge rodata and `.text` into a single `R+E`
  `PT_LOAD` starting at vaddr 0. `parse_elf_segments` now identifies "the
  executable segment" by its `PF_X` flag rather than by assuming it's the
  second `PT_LOAD` -- which is the same segment as rodata in the merged case,
  and the classic second segment otherwise.
- **Byte patterns baked in a specific register.** The script's `adrp`/`add`
  byte pattern (`?9 ?? ?? ?0 29 ?? ?? 91`, arm64) and `lea` pattern
  (`48 8d 3d ?? ?? ?? FF`, x64) both have "fixed" nibbles that are actually
  bits of the *specific registers* whatever binary the pattern was
  reverse-engineered from happened to use (register `x9` for the arm64 case,
  `rdi` for the x64 `ModRM` byte). A `libflutter.so` built with a different
  compiler/toolchain that allocates different registers for the same
  computation won't match the pattern at all -- confirmed by testing against
  a real, unmodified release APK. `resolveVerifyCertChainArm64`/`X64` now
  scan for the mnemonic (`adrp`, or `lea` with a RIP-relative operand)
  directly via Capstone and validate the *computed target address*, which is
  register-allocation-agnostic. This also makes the original script's
  alibaba.com-specific pattern variant unnecessary -- a register-agnostic
  scan already covers whatever register a different build happens to use.

### Known limitation: GetSockAddr can resolve to the wrong function on some builds

After the three fixes above, one specific test app (see docs/relatorio's
validation log) still installs hooks successfully -- `verify_cert_chain` and
`GetSockAddr` both "resolve" and the TLS bypass works -- but the socket
rewrite never fires for its real network requests, even after forcing a
brand new connection (confirmed by toggling Wi-Fi off/on before retrying, to
rule out a reused keep-alive connection). Three other scenarios (two public
training apps tested by the developer, and a real production app tested
independently by the project's author against their own client's app) work
end-to-end with the exact same code, including full captured, decrypted
traffic.

The likely explanation: `GetSockAddr`'s address is derived by walking forward
from `Socket_CreateConnect` and taking the target of the *2nd* `bl`
instruction (mirroring the original script exactly). On a build whose
`Socket_CreateConnect` has a different number of calls before the real
`GetSockAddr` call (a plausible compiler/engine-version difference), that
walk lands on a function that happens to *look* like a sockaddr-filling
routine (same prologue shape, writes a family value at offset 0, zeroes a
`sockaddr_storage`-sized buffer) without being the one actually invoked for
outbound TCP connections in that build. This wasn't chased further: pinning
it down requires reversing that specific build's full `Socket_CreateConnect`
call graph, with no guarantee of a quick answer, and the test app in
question isn't load-bearing for the project (see the development report).

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

## Module install scripts (`module/template/`)

Standard Magisk module layout, understood the same way by Magisk, KernelSU
(including KernelSU Next), SukiSu Ultra and APatch:

- `module.prop` -- id/name/version/description shown in the module manager.
- `customize.sh` -- runs at install time; writes the default `config.json` if
  one doesn't already exist (see above), and leaves an existing one alone.
- `uninstall.sh` -- deliberately does nothing to `/data/adb/fluttertap`, so
  reinstalling the module later restores the user's settings exactly as they
  left them.
- `action.sh` -- runs on demand when the user taps the "Action" button next
  to FlutterTap in the module manager's list (present automatically whenever
  a module ships this file, no extra `module.prop` flag needed). Just opens
  the manager app (`am start -n com.eduardolopes.fluttertap/.MainActivity`),
  since that's the only settings surface FlutterTap has -- there's nothing
  else useful to run on demand.

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

- Lists launchable apps on the device, plus an opt-in toggle to also show
  system apps (`AppRepository.listSystemOnlyApps`, for OEM/system components
  without a launcher icon that can still embed a Flutter engine), and lets
  the user pick which ones FlutterTap should hook (checkbox list with search
  and per-app icons; already-selected apps float to the top of the list).
- Edits the proxy IP/port and a master enable switch.
- Detects root / the active root manager (Magisk/KernelSU/APatch), and shows
  a single combined flag for whether the module is both installed *and*
  enabled (`RootManager.queryStatus` checks for the module directory AND the
  absence of its `disable` marker file -- the same convention all three root
  solutions use) -- for display only, this has no effect on whether the
  module itself works, since all three implement the same Zygisk API. If root
  wasn't granted (e.g. the user dismissed the automatic prompt), a button
  retries the request (`RootManager.requestRoot`, closes any cached non-root
  shell and asks again).
- Writes `config.json` through a root shell (`libsu`), base64-encoded so no
  part of the JSON ever needs shell escaping.
- Ships English, Portuguese (Brazil), Chinese, Spanish, Arabic, French and
  Hindi strings, switchable in-app via `AppCompatDelegate.setApplicationLocales`.
  `MainActivity` extends `AppCompatActivity` (not the plain `ComponentActivity`
  Compose activities default to) specifically for this: AppCompatDelegate's
  per-app language backport only auto-recreates the activity with the new
  locale on API <33 when it does. The `Theme.FlutterTap` style also has to
  descend from `Theme.AppCompat` for the same reason -- `AppCompatActivity`
  throws on `setContentView` otherwise. `FlutterTapApplication.onCreate` is
  where libsu's one-time shell builder is configured, not `MainActivity`,
  since the locale-triggered activity recreate would otherwise re-run it and
  crash ("the main shell was already created").
- Always derives its color scheme from `isSystemInDarkTheme()` (plus a
  `values-night/themes.xml` for the window background/status bar before
  Compose even loads), so it can't end up showing a light background while
  the system is in dark mode.
