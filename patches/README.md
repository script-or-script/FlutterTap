# Patches

## `dobby-android-build.patch`

Two fixes needed to build [Dobby](https://github.com/jmpews/Dobby) for Android
with a current NDK:

- `external/logging/logging/logging.h` — adds the missing `inline` on the
  out-of-line `Logger::Shared()` definition. Without it, linking `dobby_static`
  into any real binary fails with duplicate-symbol errors.
- `CMakeLists.txt` — skips the shared `dobby` target on Android. It fails to
  link for the same reason, and nothing here uses it; only `dobby_static` is
  linked.

Applies on top of upstream commit `e9fe7fb` (2023-04-21).

### Why this file exists

The `third_party/dobby` submodule is currently pinned to `025e9fc`, which is
this patch already committed **on a local clone only** — it is not reachable
from any remote, so `git clone --recurse-submodules` cannot fetch it. This file
is a backup of that patch so it survives independently of the local checkout.

**This is a temporary state.** The intended fix is a fork of Dobby under the
project author's own account with this patch pushed to it, and `.gitmodules`
repointed there — after which the submodule pin resolves for everyone and this
directory can be deleted. Until that happens, a fresh clone of FlutterTap will
not build without applying this patch manually:

```sh
cd module/src/main/cpp/third_party/dobby
git fetch origin e9fe7fbecae47a2287e761080f8b1133cc22e8fa
git checkout e9fe7fbecae47a2287e761080f8b1133cc22e8fa
git apply ../../../../../../patches/dobby-android-build.patch
```
