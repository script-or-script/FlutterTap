// FlutterTap native module -- by Eduardo Lopes
#include "mem_scan.h"

#include <pthread.h>

#include <csetjmp>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "log.h"

namespace {

// Per-thread state for the guarded-read machinery. Deliberately pthread
// thread-specific data rather than `thread_local`: a thread_local makes the
// compiler emit dynamic-TLS relocations (R_AARCH64_TLSDESC on arm64,
// R_X86_64_DTPMOD64 on x86_64), and Zygisk Next's optional "Zygisk Next
// Linker" preloads modules with its own minimal ELF loader that never
// registers a TLS block for us -- the first access to a thread_local then
// segfaults, in every single hooked process. pthread_get/setspecific are
// ordinary libc calls (plain JUMP_SLOT relocations), so they behave
// identically under the system linker and under that minimal loader.
// See docs/ARCHITECTURE.md.
struct GuardState {
    sigjmp_buf jmpBuf;
    volatile sig_atomic_t active;
};

pthread_key_t g_guardKey;
pthread_once_t g_guardKeyOnce = PTHREAD_ONCE_INIT;
// Also read from the signal handler, so kept signal-safe: a plain
// sig_atomic_t, no std::atomic and no locking.
volatile sig_atomic_t g_guardKeyReady = 0;

void freeGuardState(void *state) { free(state); }

void createGuardKey() {
    if (pthread_key_create(&g_guardKey, freeGuardState) == 0) g_guardKeyReady = 1;
}

// Allocates on first use per thread. Never call this from the signal handler:
// malloc isn't async-signal-safe. safe_read() calls it *before* arming the
// guard, so by the time the handler runs the state already exists.
GuardState *guardState() {
    pthread_once(&g_guardKeyOnce, createGuardKey);
    if (!g_guardKeyReady) return nullptr;
    auto *state = static_cast<GuardState *>(pthread_getspecific(g_guardKey));
    if (state == nullptr) {
        state = static_cast<GuardState *>(calloc(1, sizeof(GuardState)));
        if (state != nullptr && pthread_setspecific(g_guardKey, state) != 0) {
            free(state);
            state = nullptr;
        }
    }
    return state;
}

struct sigaction g_oldSegv {};
struct sigaction g_oldBus {};

// Hands the signal back to whoever owned it before us. Every path must end
// either in a handler that resolves the fault or in the process dying:
// returning from a SIGSEGV handler without fixing the fault re-executes the
// faulting instruction, so a silent return here would spin the app at 100% CPU
// forever instead of crashing.
void invokeOld(const struct sigaction &old, int sig, siginfo_t *info, void *ctx) {
    if ((old.sa_flags & SA_SIGINFO) && old.sa_sigaction != nullptr) {
        old.sa_sigaction(sig, info, ctx);
        return;
    }
    if (!(old.sa_flags & SA_SIGINFO) && old.sa_handler != SIG_DFL && old.sa_handler != SIG_IGN &&
        old.sa_handler != nullptr) {
        old.sa_handler(sig);
        return;
    }
    // SIG_DFL, SIG_IGN, or a malformed disposition. Restore whatever was there
    // and return so the instruction re-faults naturally: unlike raise(), that
    // keeps the tombstone's fault address and registers pointing at the real
    // faulting instruction instead of at this handler frame -- otherwise an
    // unrelated app crash looks like it came from FlutterTap.
    sigaction(sig, &old, nullptr);
}

void segvHandler(int sig, siginfo_t *info, void *ctx) {
    if (g_guardKeyReady) {
        // Read-only lookup, no allocation: safe to do from a signal handler.
        auto *state = static_cast<GuardState *>(pthread_getspecific(g_guardKey));
        if (state != nullptr && state->active) {
            state->active = 0;
            siglongjmp(state->jmpBuf, 1);
        }
    }
    // Not one of our guarded reads: don't swallow a real crash, chain to
    // whatever handler (Dart VM / crash reporter / default) was there before.
    invokeOld(sig == SIGSEGV ? g_oldSegv : g_oldBus, sig, info, ctx);
}

int hexNibble(char c) {
    if (c == '?') return -1;
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

bool safe_read(void *dst, const void *src, size_t n) {
    GuardState *state = guardState();
    // No per-thread guard available (allocation failed): fail closed rather
    // than attempt an unguarded read that could take down the host app.
    if (state == nullptr) return false;
    if (sigsetjmp(state->jmpBuf, 1) != 0) {
        state->active = 0;
        return false;
    }
    state->active = 1;
    memcpy(dst, src, n);
    state->active = 0;
    return true;
}

void mem_scan_install_crash_guard() {
    // Idempotent on purpose: a second call would capture *our own* handler as
    // "the old handler", and invokeOld() would then recurse into segvHandler
    // forever on the first non-guarded SIGSEGV.
    static bool installed = false;
    if (installed) return;
    installed = true;

    pthread_once(&g_guardKeyOnce, createGuardKey);

    struct sigaction sa {};
    sa.sa_sigaction = segvHandler;
    // SA_ONSTACK matches what ART installs its own SIGSEGV handler with: it
    // uses guard-page faults for implicit stack-overflow checks, so on a real
    // stack overflow the handler must run on the alternate stack, not on the
    // exhausted one. Harmless where no sigaltstack exists -- the kernel just
    // ignores the flag.
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &g_oldSegv);
    sigaction(SIGBUS, &sa, &g_oldBus);
}

BytePattern compile_pattern(const std::string &pattern_str) {
    BytePattern pattern;
    std::istringstream iss(pattern_str);
    std::string tok;
    while (iss >> tok) {
        if (tok.size() != 2) continue;
        int hi = hexNibble(tok[0]);
        int lo = hexNibble(tok[1]);
        uint8_t value = 0, mask = 0;
        if (hi >= 0) {
            value |= static_cast<uint8_t>(hi << 4);
            mask |= 0xF0;
        }
        if (lo >= 0) {
            value |= static_cast<uint8_t>(lo);
            mask |= 0x0F;
        }
        pattern.value.push_back(value);
        pattern.mask.push_back(mask);
    }
    return pattern;
}

std::vector<uintptr_t> scan_all_matches(uintptr_t start, size_t size, const BytePattern &pattern) {
    std::vector<uintptr_t> matches;
    if (pattern.value.empty() || size < pattern.value.size() || start == 0) return matches;

    std::vector<uint8_t> buf(size);
    if (!safe_read(buf.data(), reinterpret_cast<void *>(start), size)) {
        ft_log_warn("mem_scan: unreadable region at 0x%zx size=%zu, skipping", start, size);
        return matches;
    }

    const size_t patLen = pattern.value.size();
    for (size_t i = 0; i + patLen <= size; i++) {
        bool ok = true;
        for (size_t j = 0; j < patLen; j++) {
            if ((buf[i + j] & pattern.mask[j]) != (pattern.value[j] & pattern.mask[j])) {
                ok = false;
                break;
            }
        }
        if (ok) matches.push_back(start + i);
    }
    return matches;
}
