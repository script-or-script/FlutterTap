# Changelog

## v1.0.0

First release.

- Zygisk module that redirects the traffic of selected Flutter apps to a
  configurable proxy and bypasses BoringSSL's TLS certificate verification, with
  no CA certificate installed on the device and no app repackaging.
- Manager app (Jetpack Compose) for picking target apps and setting the proxy
  IP/port on the device. Configuration can also be provisioned directly by
  writing `/data/adb/fluttertap/config.json`.
- Target functions are resolved at runtime by parsing `libflutter.so`'s ELF
  segments, scanning for anchor strings and disassembling with Capstone, so
  resolution does not depend on which registers a particular build's compiler
  allocated. Hooks are installed with Dobby.
- Non-selected apps are untouched — the target check runs in
  `preAppSpecialize` and the library unloads itself immediately.
- Compatible with Magisk's built-in Zygisk, Zygisk Next (including with
  **Zygisk Next Linker** enabled) and NeoZygisk.
- Android 10 (API 29) through 17 (API 37), `arm64-v8a` and `x86_64`.

Validated on hardware in two environments: OnePlus 5 (Android 10, Magisk v27.2 +
NeoZygisk 2.3) and Pixel 8a (Android 17, SukiSu Ultra + Zygisk Next 1.4.3 with
the Linker enabled).

Known limitation: on some `libflutter.so` builds the socket-redirect target can
resolve to the wrong function, in which case the TLS bypass still works but the
redirect does not fire. See `docs/ARCHITECTURE.md`.
