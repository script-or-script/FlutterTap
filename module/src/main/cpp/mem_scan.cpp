// FlutterTap native module -- by Eduardo Lopes
#include "mem_scan.h"

#include <csetjmp>
#include <csignal>
#include <cstring>
#include <sstream>

#include "log.h"

namespace {

thread_local sigjmp_buf g_jmpBuf;
thread_local volatile sig_atomic_t g_guardActive = 0;

struct sigaction g_oldSegv {};
struct sigaction g_oldBus {};

void invokeOld(const struct sigaction &old, int sig, siginfo_t *info, void *ctx) {
    if (old.sa_flags & SA_SIGINFO) {
        if (old.sa_sigaction) old.sa_sigaction(sig, info, ctx);
        return;
    }
    if (old.sa_handler == SIG_DFL) {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    if (old.sa_handler != SIG_IGN && old.sa_handler != nullptr) {
        old.sa_handler(sig);
    }
}

void segvHandler(int sig, siginfo_t *info, void *ctx) {
    if (g_guardActive) {
        g_guardActive = 0;
        siglongjmp(g_jmpBuf, 1);
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
    if (sigsetjmp(g_jmpBuf, 1) != 0) {
        g_guardActive = 0;
        return false;
    }
    g_guardActive = 1;
    memcpy(dst, src, n);
    g_guardActive = 0;
    return true;
}

void mem_scan_install_crash_guard() {
    struct sigaction sa {};
    sa.sa_sigaction = segvHandler;
    sa.sa_flags = SA_SIGINFO;
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
