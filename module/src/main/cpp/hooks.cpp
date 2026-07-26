// FlutterTap native module -- by Eduardo Lopes
#include "hooks.h"

#include <arpa/inet.h>
#include <dlfcn.h>
#include <dobby.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <string>

#include "log.h"
#include "mem_scan.h"

namespace {

// The original script keeps the captured sockaddr in a single global; we use
// thread_local instead since a persistent, long-running module (unlike a
// one-shot Frida session) can see multiple threads opening connections
// concurrently. This is a deliberate correctness improvement, not a
// functional change: same bypass, race-free.
thread_local void *g_capturedSockAddr = nullptr;

std::string g_proxyIp;
uint16_t g_proxyPort = 0;

using SocketFn = int (*)(int, int, int);
SocketFn g_origSocket = nullptr;

using VerifyCertChainFn = int (*)(void *, void *, void *);
VerifyCertChainFn g_origVerifyCertChain = nullptr;

void getSockAddrInstrumentCallback(void *address, DobbyRegisterContext *ctx) {
    (void)address;
#if defined(__aarch64__)
    g_capturedSockAddr = reinterpret_cast<void *>(ctx->general.regs.x1);
#elif defined(__x86_64__)
    g_capturedSockAddr = reinterpret_cast<void *>(ctx->general.regs.rsi);
#endif
}

int fakeSocket(int domain, int type, int protocol) {
    void *sa = g_capturedSockAddr;
    g_capturedSockAddr = nullptr; // consume once so a later unrelated socket() isn't mistakenly rewritten

    if (sa != nullptr) {
        uint16_t family = 0;
        if (safe_read(&family, sa, sizeof(family)) && family == AF_INET) {
            auto *sin = reinterpret_cast<sockaddr_in *>(sa);
            sin->sin_port = htons(g_proxyPort);
            inet_pton(AF_INET, g_proxyIp.c_str(), &sin->sin_addr);
            ft_log_info("overwrite sockaddr -> %s:%u", g_proxyIp.c_str(), g_proxyPort);
        }
    }
    return g_origSocket(domain, type, protocol);
}

int fakeVerifyCertChain(void *session, void *handshake, void *outAlert) {
    int ret = g_origVerifyCertChain(session, handshake, outAlert);
    if (ret == 0) {
        ft_log_info("verify_cert_chain bypass");
        return 1;
    }
    return ret;
}

} // namespace

namespace hooks {

bool install(const MappedModule &flutter_mod, const ResolvedAddrs &addrs, const ModuleConfig &cfg) {
    g_proxyIp = cfg.proxy_ip;
    g_proxyPort = static_cast<uint16_t>(cfg.proxy_port);

    bool ok = true;

    if (DobbyHook(reinterpret_cast<void *>(addrs.verify_cert_chain), reinterpret_cast<void *>(fakeVerifyCertChain),
                  reinterpret_cast<void **>(&g_origVerifyCertChain)) != 0) {
        ft_log_error("DobbyHook(verify_cert_chain) failed");
        ok = false;
    }

    if (DobbyInstrument(reinterpret_cast<void *>(addrs.get_sock_addr), getSockAddrInstrumentCallback) != 0) {
        ft_log_error("DobbyInstrument(GetSockAddr) failed");
        ok = false;
    }

    void *socketAddr = dlsym(RTLD_DEFAULT, "socket");
    if (!socketAddr) {
        ft_log_error("dlsym(socket) failed");
        ok = false;
    } else if (DobbyHook(socketAddr, reinterpret_cast<void *>(fakeSocket), reinterpret_cast<void **>(&g_origSocket)) !=
               0) {
        ft_log_error("DobbyHook(socket) failed");
        ok = false;
    }

    if (ok) {
        ft_log_info("hooks installed for %s -> proxy %s:%u", flutter_mod.path.c_str(), g_proxyIp.c_str(),
                    g_proxyPort);
    }
    return ok;
}

} // namespace hooks
