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

## You do not need to apply this manually

The `third_party/dobby` submodule already points at a fork that carries this
patch, so `git clone --recurse-submodules` gets a buildable tree with no extra
steps:

- fork: https://github.com/script-or-script/Dobby
- branch: `fluttertap-android-build`
- pinned commit: `025e9fc`, whose parent is upstream `e9fe7fb`

This file is kept as the readable record of exactly what was changed in a
third-party dependency and why — reviewing a `.patch` is considerably easier
than diffing two forks. It is also what you would re-apply if you ever want to
rebase onto a newer upstream Dobby:

```sh
cd module/src/main/cpp/third_party/dobby
git fetch https://github.com/jmpews/Dobby.git master
git checkout -b rebase-attempt FETCH_HEAD
git apply ../../../../../../patches/dobby-android-build.patch
```
