# Third-party components

FlutterTap itself is MIT-licensed (see [`LICENSE`](LICENSE)). The native module
statically links the components below, so their licenses apply to the shipped
`.so` files inside `FlutterTap-<version>.zip` as well as to this source tree.
Full license texts are bundled in the flashable zip under `licenses/`.

| Component | Version / pin | License | Where it comes from |
|---|---|---|---|
| [Dobby](https://github.com/jmpews/Dobby) | submodule, pinned | Apache-2.0 | git submodule at `module/src/main/cpp/third_party/dobby` |
| [Capstone](https://github.com/capstone-engine/capstone) | 5.0.9 | BSD-3-Clause (parts under the LLVM license) | git submodule at `module/src/main/cpp/third_party/capstone` |
| `zygisk.hpp` | — | 0BSD | Zygisk public API header from [topjohnwu/zygisk-module-sample](https://github.com/topjohnwu/zygisk-module-sample), vendored at `module/src/main/cpp/include/zygisk.hpp` with its original notice intact |

Notes:

- **Dobby** is used for inline hooking (`DobbyHook`, `DobbyInstrument`). Apache-2.0
  section 4 requires the license to accompany binary distributions, which is why
  `licenses/` is included in the flashable zip.
- **Capstone** is used to disassemble `libflutter.so` when resolving hook
  addresses. It is BSD-3-Clause, but ships an additional `LICENSE_LLVM.TXT`
  covering parts derived from LLVM; both files are bundled.
- **Zygisk Next** is *not* vendored here. Only the public `zygisk.hpp` API header
  (0BSD, from the sample module above) is used, deliberately — see
  `docs/ARCHITECTURE.md`.
- The Dobby submodule carries a small local patch needed to build for Android;
  see [`patches/README.md`](patches/README.md).
