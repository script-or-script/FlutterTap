<p align="center">
  <img src="docs/assets/banner.png" alt="FlutterTap — Zygisk module for intercepting Flutter app traffic">
</p>

<p align="center">
  <b>English</b> ·
  <a href="README.pt-BR.md">Português (BR)</a> ·
  <a href="README.zh-CN.md">中文</a>
</p>

A **Zygisk** module that redirects the network traffic of selected **Flutter** apps to a configurable
proxy and bypasses BoringSSL's TLS certificate verification — with no certificate installed on the
device, no app repackaging, and no Frida session tethered over USB.

It ships with a **manager app** (Jetpack Compose) for picking target apps and setting the proxy IP/port
directly on the phone.

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/5-bypass-app2.png" alt="HTTPS traffic from a Flutter app captured and decrypted in Burp Suite">
</p>
<p align="center">
  <em>HTTPS traffic from a Flutter app arriving decrypted in Burp — with no CA certificate installed on the device.</em>
</p>

## Why it exists

Flutter apps don't use Android's TLS stack. The engine bundles its own **BoringSSL** and keeps its own
trust chain, which means installing Burp's CA certificate on the system **intercepts nothing** — the app
ignores the Android certificate store entirely. And since Flutter doesn't honour the Wi-Fi proxy either,
the traffic simply leaves the device without ever passing through your interceptor.

The workaround is well known, but it normally requires running a Frida script with `frida-server` active
and a cable attached, every session. FlutterTap turns that same approach into a persistent module:
install once, pick your apps, and it runs on its own on every boot.

## Why a module, and not Frida or LSPosed

**Frida** is unbeatable for *discovering* how an app works — you edit JavaScript and see the effect in
seconds. That's how the technique was born. The problem is *operating* with it: it depends on
`frida-server` and a live session (close the terminal, lose the hook), doesn't survive a reboot, needs
spawn (`-f`) to catch startup hooks, and — most relevant in real engagements — **opens a listening port
and leaves well-known artifacts** (thread names, memory strings, `/proc` entries). Detecting Frida is
literally the first thing any mobile protection SDK does.

**LSPosed/Xposed** isn't a matter of convenience but of layer: it hooks **Java/ART methods**. In Flutter,
`verify_cert_chain` and `GetSockAddr` are **unexported native functions inside `libflutter.so`** — there
is no Java method to intercept. An LSPosed module would have to do exactly the same native work
FlutterTap does, while dragging in the whole Xposed runtime for nothing.

|  | Frida | LSPosed | FlutterTap |
|---|---|---|---|
| Reaches native BoringSSL | yes | **not directly** | yes |
| Survives a reboot | no | yes | yes |
| No cable / external tooling | no | yes | yes |
| Listening port / external process | yes (detectable) | no | no |
| Catches the app from its first instant | only via manual spawn | yes | yes |
| Zero impact on non-selected apps | — | depends | yes (`DLCLOSE`) |
| Development iteration speed | **excellent** | medium | slow |

That last row is an honest drawback: iterating on a native module is far slower than editing a `.js`.
Which is exactly why the split makes sense — **Frida to discover, a module to operate**.

### Why it was built

The motivation comes from real mobile traffic analysis work. In an actual engagement the app has to
behave **naturally over time**: opened and closed repeatedly, receiving notifications, syncing in the
background, used by someone who has never heard of Frida. And the setup has to be **reproducible** —
reconfigurable by anyone ticking checkboxes, with no terminal and no memorised commands.

FlutterTap turns a manual, fragile procedure into **infrastructure**. Per-app targeting exists precisely
for that: it is not a global interceptor — it stays inert in every app you didn't select, unloading
itself from memory immediately, which reduces both the risk of side effects and the detection surface.

## How it works

None of the functions involved are exported, so they have to be located at runtime, in every process:

1. The module is loaded into each app process via Zygisk and checks whether that package is in the target
   list — if it isn't, it unloads itself immediately (zero footprint on non-selected apps).
2. On a monitoring thread, it waits for `libflutter.so` to load and parses its ELF segments.
3. It scans memory for the strings `"ssl_client"` and `"Socket_CreateConnect"` and, from those, resolves
   the real addresses of `verify_cert_chain` and `GetSockAddr` by **disassembling with Capstone** — the
   same library Frida uses under the hood.
4. It installs three hooks with Dobby: capture the destination `sockaddr`, rewrite IP/port to the proxy,
   and force the "certificate is valid" result.

Implementation details, including where and why this diverges from the original approach, are in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Compatibility

| Item | Support |
|---|---|
| Android | 10 (API 29) through 17 (API 37) |
| Architectures | `arm64-v8a`, `x86_64` |
| Root | Magisk, KernelSU, SukiSu Ultra, APatch |
| Zygisk | Magisk's built-in Zygisk, Zygisk Next (**including with Zygisk Next Linker enabled**) or NeoZygisk |

Validated on hardware in two deliberately different environments:

- **OnePlus 5** — Android 10, Kitsune Magisk v27.2 + NeoZygisk 2.3
- **Pixel 8a** — Android 17, SukiSu Ultra + Zygisk Next 1.4.3, with **Zygisk Next Linker** enabled

> **Zygisk is required.** Root alone is not enough: the whole mechanism lives in the Zygisk callbacks. On
> Magisk, turn on the "Zygisk" setting. On KernelSU/SukiSu Ultra, install Zygisk Next or NeoZygisk.
> APatch ships a compatible implementation.
>
> Compatibility with **Zygisk Next Linker** required specific fixes. If you maintain a Zygisk module that
> breaks with that feature, the "Zygisk Next Linker compatibility" section of
> [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) documents the three traps found (and one false lead that
> cost real time), in reusable form.

## Installation

**Recommended — via MMRL:** in [MMRL](https://mmrl.dev/) open *Repositories → Add* and paste the
repository URL below. FlutterTap then installs in one tap and gets update notifications automatically:

```
https://raw.githubusercontent.com/script-or-script/FlutterTap-mmrl/main/json/modules.json
```

**Or manually:** download both files from the [latest release](../../releases/latest).

1. **Install the module**: flash `FlutterTap-<version>.zip` from your root manager app
   (Magisk / KernelSU / SukiSu Ultra / APatch). Reboot.
2. **Install the manager app** (`FlutterTap-manager-<version>.apk`) and grant root on first launch.
3. **Configure**: set the IP and port of your proxy (the IP of the machine running Burp, on the same
   Wi-Fi network) and tick the apps you want to intercept.
4. **Force-stop the target app and open it again.** The hook only enters new processes — reopening
   without force-stopping is not enough.

> **Root is required for the manager app too**, not just to save: the configuration lives under
> `/data/adb/`, so without root it cannot even read what is already stored.

In Burp, use a listener with **invisible proxying** enabled, since the app sends ordinary requests rather
than proxy-directed ones.

### Step by step, with evidence

Real captures from a Pixel 8a running Android 17 with SukiSu Ultra, in the order the steps happen.

**1. Environment and module installation**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/1-device.png" alt="Android 17 on a Pixel 8a, SukiSu Ultra running, the install log, and the module enabled">
</p>

Android 17 on a Pixel 8a, SukiSu Ultra running (LKM mode), the installer log ending in *Module installed
successfully*, and finally FlutterTap enabled in the module list — already showing the **Action** button,
which opens the manager app straight from there.

**2. Granting root and configuring the proxy**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/2-gerenciador-config.png" alt="Root denied, manual grant in SukiSu Ultra, and the app configured">
</p>

On first launch the app may show **"Root access denied"**: KernelSU-family managers persist the decision
per app and never re-show the prompt. The fix is highlighted on the screen itself — open SukiSu Ultra, go
to **App Profile**, and enable **SuperUser** manually. After that the app reports *Root access granted*,
*Module installed* and *Enabled*, and the real proxy IP/port can be saved.

**3. Preparing Burp**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/3-invisible-proxy.png" width="620" alt="Burp listener with invisible proxying enabled">
</p>

The listener needs **Support invisible proxying** checked (*Request handling* tab). Without it Burp drops
the connections: the app sends ordinary HTTP requests, not requests addressed to a proxy.

**4. End to end — plain HTTP**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/4-bypass-app1.png" alt="VulnApp targeted, request triggered, and traffic captured in Burp">
</p>

With **VulnApp** set as a target, the request triggered on the device shows up in Burp arriving on
**port 8083** — the listener's port, which proves the redirect happened. The
`user-agent: Dart/3.12 (dart:io)` confirms it came from Flutter's own HTTP client.

**5. End to end — decrypted HTTPS**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/5-bypass-app2.png" alt="Ostorlab Insecure App with HTTPS traffic decrypted in Burp">
</p>

The same with the **Ostorlab Insecure App**, now over **HTTPS**: Burp shows `https://ostorlab.co` with an
**HTTP/2 200** response and the HTML in the clear. This is what the pinning bypass delivers — and **no CA
certificate was installed on the device**.

### Configuring without the manager app

The app is just a front end for a single JSON file. To automate (provisioning, CI, a headless device),
write `/data/adb/fluttertap/config.json` directly:

```json
{
  "enabled": true,
  "proxy_ip": "192.168.1.10",
  "proxy_port": 8080,
  "target_packages": ["com.example.app"]
}
```

The file is re-read from disk for every process that starts, so `am force-stop` on the target app and
reopening it is enough — no reboot required. Matching is done against the process name, and listing
`com.example.app` also catches named subprocesses (`com.example.app:remote`).

## Diagnostics

```sh
adb logcat -s FlutterTap:V
```

For a target app, expect: `selected for hooking` → `libflutter.so loaded at ...` →
`verify_cert_chain=0x... GetSockAddr=0x...` → `hooks: installed for ... -> proxy IP:PORT`, then
`hooks: overwrite sockaddr` / `hooks: verify_cert_chain bypass` as the app makes requests.

If **nothing** shows up, it is almost always one of three things: the package name doesn't match the
process, `config.json` is invalid (the module falls back to defaults and intercepts nothing), or the app
was already running when the configuration changed.

## Building

Requires the Android SDK/NDK and **JDK 17 or 21** (Gradle 8.11.1 cannot parse JDK 22+; point `JAVA_HOME`
at Android Studio's bundled JBR if your default `java` is newer).

```sh
git clone --recurse-submodules https://github.com/script-or-script/FlutterTap.git
cd FlutterTap
export ANDROID_HOME=/path/to/Android/Sdk
./scripts/build_module_zip.sh          # produces dist/FlutterTap-<version>.zip
./gradlew :manager-app:assembleDebug
```

> On Windows, clone into a **short path** (e.g. `C:\dev\FlutterTap`). Capstone nests deeply enough to
> cross the 260-character limit, and the clone fails with *"Filename too long"* otherwise.

Full instructions, including release signing, are in [`docs/BUILD.md`](docs/BUILD.md).

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — how each piece works and why it was done that way,
  including Zygisk Next Linker compatibility
- [`docs/BUILD.md`](docs/BUILD.md) — building, packaging and signing

## Disclaimer

Intended for **authorised traffic analysis** — mobile penetration testing, reverse engineering and
security research on applications you have permission to test. Intercepting third-party application
traffic without authorisation is illegal in most jurisdictions.

## License

MIT — see [`LICENSE`](LICENSE). The native module statically links Dobby (Apache-2.0) and Capstone
(BSD-3); third-party components are listed in [`THIRD_PARTY.md`](THIRD_PARTY.md), and the full license
texts ship inside the distributed `.zip`.

---
Built by Eduardo Lopes
