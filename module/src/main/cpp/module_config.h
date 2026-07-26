// FlutterTap native module -- by Eduardo Lopes
#pragma once

#include <string>
#include <vector>

// Shared config written by the manager app (via root) and read by the
// native module through the root companion process at /data/adb/fluttertap/config.json
struct ModuleConfig {
    bool enabled = true;
    // Defaults mirror the original script's hardcoded values so behavior is
    // unchanged until the user picks their own values from the manager app.
    std::string proxy_ip = "192.168.15.17";
    int proxy_port = 8083;
    std::vector<std::string> target_packages;

    // True if `process_name` (Zygisk's AppSpecializeArgs::nice_name, e.g.
    // "com.example.app" or "com.example.app:remote") should be hooked.
    bool isTargeted(const std::string &process_name) const;
};

// Parses raw JSON text (as produced by the manager app / companion) into a
// ModuleConfig. Returns the built-in defaults untouched on parse failure.
ModuleConfig module_config_parse(const std::string &json_text);
