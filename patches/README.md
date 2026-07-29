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

The `third_party/dobby` submodule already points at a repository carrying this
patch, so `git clone --recurse-submodules` gets a buildable tree with no extra
steps:

- repository: https://github.com/script-or-script/Dobby-android-patched
- branch: `snapshot-android`
- pinned commit: `3d71368`

That repository holds a **single snapshot commit** of upstream `e9fe7fb`
(2023-04-21) with this patch already applied — it is not a fork and carries no
upstream history, so the commit has no parent. That was a deliberate choice: a
fork shares object storage with its parent repository, which means anything ever
pushed to it stays reachable through the upstream repo even after the fork is
deleted. A standalone repository can actually be cleaned up.

This file is therefore the record of exactly what differs from upstream, which is
also what you would re-apply if you ever want to move onto a newer Dobby:

```sh
cd module/src/main/cpp/third_party/dobby
git fetch https://github.com/jmpews/Dobby.git master
git checkout -b rebase-attempt FETCH_HEAD
git apply ../../../../../../patches/dobby-android-build.patch
```
