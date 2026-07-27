// FlutterTap native module -- by Eduardo Lopes
#pragma once

#include <string>
#include <vector>

// Shared config written by the manager app (via root) and read by the
// native module through the root companion process at /data/adb/fluttertap/config.json
struct ModuleConfig {
    bool enabled = true;
    // proxy_port mirrors the original script's hardcoded value. proxy_ip is
    // deliberately NOT the original script's default (192.168.15.17) -- that
    // looks enough like a real home-network address (192.168.x.x) that users
    // could mistake it for their own and never notice they forgot to set it.
    // 203.0.113.0/24 (RFC 5737, "TEST-NET-3") is reserved for documentation
    // and can never collide with a real LAN/WAN address, making it obvious
    // this needs to be replaced.
    std::string proxy_ip = "203.0.113.1";
    int proxy_port = 8083;
    std::vector<std::string> target_packages;

    // True if `process_name` (Zygisk's AppSpecializeArgs::nice_name, e.g.
    // "com.example.app" or "com.example.app:remote") should be hooked.
    bool isTargeted(const std::string &process_name) const;
};

// Parses raw JSON text (as produced by the manager app / companion) into a
// ModuleConfig. Returns the built-in defaults untouched on parse failure.
ModuleConfig module_config_parse(const std::string &json_text);
