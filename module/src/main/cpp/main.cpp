// FlutterTap -- native Zygisk module
// by Eduardo Lopes
//
// Ports flutter+burp.js (a Flutter/BoringSSL TLS-pinning bypass + traffic
// redirector originally built for Frida) into a persistent Zygisk module so
// it works without a USB-tethered frida-server, under Magisk, KernelSU or
// APatch. See docs/ARCHITECTURE.md for the full write-up of how each piece
// of the original script was ported.
#include <pthread.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "addr_resolver.h"
#include "companion.h"
#include "elf_utils.h"
#include "hooks.h"
#include "log.h"
#include "mem_scan.h"
#include "module_config.h"
#include "zygisk.hpp"

namespace {

constexpr const char *kFlutterLib = "libflutter.so";
constexpr int kPollIntervalUs = 50 * 1000; // 50ms, mirrors the original script's polling loop
constexpr int kMaxPollAttempts = 20 * 60 * (1000000 / kPollIntervalUs); // ~20 minutes, generous upper bound

struct MonitorContext {
    std::string processName;
    ModuleConfig cfg;
};

void runHookPipeline(const std::string &processName, const ModuleConfig &cfg) {
    MappedModule flutterMod;
    if (!find_module_by_suffix(kFlutterLib, flutterMod)) {
        ft_log_warn("%s: libflutter.so not found after waiting, giving up", processName.c_str());
        return;
    }
    ft_log_info("%s: libflutter.so loaded at 0x%zx (%s)", processName.c_str(), flutterMod.base,
                flutterMod.path.c_str());

    ElfSegments segs;
    if (!parse_elf_segments(flutterMod, segs)) {
        ft_log_error("%s: failed to parse ELF segments of %s", processName.c_str(), flutterMod.path.c_str());
        return;
    }

    ResolvedAddrs addrs;
    if (!resolve_addresses(flutterMod, segs, addrs)) {
        ft_log_error("%s: address resolution failed, hooks not installed", processName.c_str());
        return;
    }
    ft_log_info("%s: verify_cert_chain=0x%zx GetSockAddr=0x%zx", processName.c_str(), addrs.verify_cert_chain,
                addrs.get_sock_addr);

    if (!hooks::install(flutterMod, addrs, cfg)) {
        ft_log_error("%s: one or more hooks failed to install", processName.c_str());
    }
}

void *monitorThreadMain(void *arg) {
    std::unique_ptr<MonitorContext> ctx(static_cast<MonitorContext *>(arg));

    MappedModule probe;
    int attempts = 0;
    while (!find_module_by_suffix(kFlutterLib, probe)) {
        if (++attempts > kMaxPollAttempts) {
            ft_log_warn("%s: timed out waiting for libflutter.so", ctx->processName.c_str());
            return nullptr;
        }
        usleep(kPollIntervalUs);
    }

    runHookPipeline(ctx->processName, ctx->cfg);
    return nullptr;
}

} // namespace

class FlutterTapModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        api_ = api;
        env_ = env;
        mem_scan_install_crash_guard();
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        shouldHook_ = false;

        if (args->nice_name) {
            const char *niceName = env_->GetStringUTFChars(args->nice_name, nullptr);
            if (niceName) {
                processName_ = niceName;
                env_->ReleaseStringUTFChars(args->nice_name, niceName);
            }
        }
        if (processName_.empty()) return;

        // The companion connection (and therefore this whole config fetch) is
        // only valid here, before the sandbox is applied -- must not be
        // deferred to postAppSpecialize or later.
        int companionFd = api_->connectCompanion();
        std::string configJson = companion_fetch_config(companionFd);
        if (companionFd >= 0) close(companionFd);

        cfg_ = module_config_parse(configJson);
        shouldHook_ = cfg_.isTargeted(processName_);

        if (!shouldHook_) {
            // Not a target process: unload ourselves ASAP to keep FlutterTap's
            // footprint at zero for every app the user didn't select.
            api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        (void)args;
        if (!shouldHook_) return;

        ft_log_info("%s: selected for hooking, waiting for libflutter.so", processName_.c_str());
        auto *ctx = new MonitorContext{processName_, cfg_};
        pthread_t thread;
        if (pthread_create(&thread, nullptr, &monitorThreadMain, ctx) != 0) {
            ft_log_error("%s: failed to spawn monitor thread", processName_.c_str());
            delete ctx;
            return;
        }
        pthread_detach(thread);
    }

private:
    zygisk::Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    std::string processName_;
    ModuleConfig cfg_;
    bool shouldHook_ = false;
};

REGISTER_ZYGISK_MODULE(FlutterTapModule)
REGISTER_ZYGISK_COMPANION(companion_handler)
