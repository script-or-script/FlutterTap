// FlutterTap native module -- by Eduardo Lopes
#include "hooks.h"

#include <arpa/inet.h>
#include <dlfcn.h>
#include <dobby.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>

#include <string>

#include "log.h"
#include "mem_scan.h"

namespace {

// The original script keeps the captured sockaddr in a single global; we keep
// it per-thread instead since a persistent, long-running module (unlike a
// one-shot Frida session) can see multiple threads opening connections
// concurrently. This is a deliberate correctness improvement, not a
// functional change: same bypass, no cross-thread interleaving.
//
// pthread thread-specific data rather than `thread_local`: a thread_local
// makes the compiler emit dynamic-TLS relocations that Zygisk Next's optional
// "Zygisk Next Linker" minimal ELF loader cannot resolve, which segfaults
// every hooked process. See the fuller note in mem_scan.cpp.
pthread_key_t g_sockAddrKey;
pthread_once_t g_sockAddrKeyOnce = PTHREAD_ONCE_INIT;
// A plain bool, unlike mem_scan.cpp's volatile sig_atomic_t equivalent: this
// one is never read from a signal handler.
bool g_sockAddrKeyReady = false;

void createSockAddrKey() {
    // No destructor: the value is a borrowed pointer into the caller's
    // stack/heap, owned by the host app, never by us.
    if (pthread_key_create(&g_sockAddrKey, nullptr) == 0) g_sockAddrKeyReady = true;
}

void *capturedSockAddr() {
    pthread_once(&g_sockAddrKeyOnce, createSockAddrKey);
    return g_sockAddrKeyReady ? pthread_getspecific(g_sockAddrKey) : nullptr;
}

void setCapturedSockAddr(void *addr) {
    pthread_once(&g_sockAddrKeyOnce, createSockAddrKey);
    if (g_sockAddrKeyReady) pthread_setspecific(g_sockAddrKey, addr);
}

std::string g_proxyIp;
uint16_t g_proxyPort = 0;
// Parsed once at install() rather than on every intercepted socket(). Besides
// keeping inet_pton off the hot path, this closes a nasty failure mode: on a
// malformed address inet_pton returns 0 and leaves the output untouched, so
// parsing inside the hook would rewrite the *port* and leave the original
// destination host -- while still logging a successful-looking redirect.
in_addr g_proxyAddr{};

using SocketFn = int (*)(int, int, int);
SocketFn g_origSocket = nullptr;

using VerifyCertChainFn = int (*)(void *, void *, void *);
VerifyCertChainFn g_origVerifyCertChain = nullptr;

void getSockAddrInstrumentCallback(void *address, DobbyRegisterContext *ctx) {
    (void)address;
#if defined(__aarch64__)
    setCapturedSockAddr(reinterpret_cast<void *>(ctx->general.regs.x1));
#elif defined(__x86_64__)
    setCapturedSockAddr(reinterpret_cast<void *>(ctx->general.regs.rsi));
#endif
}

int fakeSocket(int domain, int type, int protocol) {
    void *sa = capturedSockAddr();
    setCapturedSockAddr(nullptr); // consume once so a later unrelated socket() isn't mistakenly rewritten

    if (sa != nullptr) {
        uint16_t family = 0;
        if (safe_read(&family, sa, sizeof(family)) && family == AF_INET) {
            auto *sin = reinterpret_cast<sockaddr_in *>(sa);
            sin->sin_port = htons(g_proxyPort);
            sin->sin_addr = g_proxyAddr;
            ft_log_info("hooks: overwrite sockaddr -> %s:%u", g_proxyIp.c_str(), g_proxyPort);
        }
    }
    return g_origSocket(domain, type, protocol);
}

int fakeVerifyCertChain(void *session, void *handshake, void *outAlert) {
    int ret = g_origVerifyCertChain(session, handshake, outAlert);
    if (ret == 0) {
        ft_log_info("hooks: verify_cert_chain bypass");
        return 1;
    }
    return ret;
}

// Must NOT be resolved via dlsym(RTLD_DEFAULT, ...): RTLD_DEFAULT makes bionic
// derive the lookup scope from the *calling* library, which it does by mapping
// our return address back to a linker soinfo. Under Zygisk Next's "Zygisk Next
// Linker" our .so is mapped by ZN's own minimal loader, so the system linker
// holds no soinfo for us, that caller lookup fails, and dlsym returns null.
// That cost us the socket hook specifically -- the Dobby hooks on libflutter.so
// still installed, so the TLS bypass worked while traffic silently kept going
// to its real destination instead of the proxy.
//
// An explicit libc handle needs no caller lookup, so it works under both
// loaders. RTLD_NOLOAD since libc is always already mapped -- we only want a
// handle to it, never to load anything.
void *resolveLibcSocket() {
    void *sym = nullptr;
    if (void *libc = dlopen("libc.so", RTLD_NOW | RTLD_NOLOAD)) {
        sym = dlsym(libc, "socket");
        dlclose(libc);
        if (sym != nullptr) {
            ft_log_info("hooks: resolved socket() via explicit libc handle");
            return sym;
        }
    }
    // Fallback for the plain-Zygisk path, where the system linker does know us.
    sym = dlsym(RTLD_DEFAULT, "socket");
    if (sym != nullptr) ft_log_info("hooks: resolved socket() via RTLD_DEFAULT");
    return sym;
}

} // namespace

namespace hooks {

bool install(const MappedModule &flutter_mod, const ResolvedAddrs &addrs, const ModuleConfig &cfg) {
    g_proxyIp = cfg.proxy_ip;
    g_proxyPort = static_cast<uint16_t>(cfg.proxy_port);

    bool ok = true;

    // Validate up front so a typo in the manager app fails loudly here instead
    // of silently producing connections that keep going to the real host.
    const bool proxyAddrOk = inet_pton(AF_INET, g_proxyIp.c_str(), &g_proxyAddr) == 1;
    if (!proxyAddrOk) {
        ft_log_error("hooks: invalid proxy IP '%s' -- socket redirect disabled", g_proxyIp.c_str());
        ok = false;
    }

    if (DobbyHook(reinterpret_cast<void *>(addrs.verify_cert_chain), reinterpret_cast<void *>(fakeVerifyCertChain),
                  reinterpret_cast<void **>(&g_origVerifyCertChain)) != 0) {
        ft_log_error("hooks: DobbyHook(verify_cert_chain) failed");
        ok = false;
    }

    if (DobbyInstrument(reinterpret_cast<void *>(addrs.get_sock_addr), getSockAddrInstrumentCallback) != 0) {
        ft_log_error("hooks: DobbyInstrument(GetSockAddr) failed");
        ok = false;
    }

    // Only worth hooking socket() if we have somewhere valid to redirect to;
    // the TLS bypass above still applies either way.
    if (proxyAddrOk) {
        void *socketAddr = resolveLibcSocket();
        if (!socketAddr) {
            ft_log_error("hooks: could not resolve libc socket()");
            ok = false;
        } else if (DobbyHook(socketAddr, reinterpret_cast<void *>(fakeSocket),
                             reinterpret_cast<void **>(&g_origSocket)) != 0) {
            ft_log_error("hooks: DobbyHook(socket) failed");
            ok = false;
        }
    }

    if (ok) {
        ft_log_info("hooks: installed for %s -> proxy %s:%u", flutter_mod.path.c_str(), g_proxyIp.c_str(),
                    g_proxyPort);
    }
    return ok;
}

} // namespace hooks
