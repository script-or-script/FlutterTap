// FlutterTap native module -- by Eduardo Lopes
#include "module_config.h"

#include "json_min.h"

bool ModuleConfig::isTargeted(const std::string &process_name) const {
    if (!enabled) return false;
    for (const auto &pkg : target_packages) {
        if (process_name == pkg) return true;
        // Match named sub/isolate processes of the same app, e.g. "pkg:remote".
        if (process_name.size() > pkg.size() + 1 && process_name.compare(0, pkg.size(), pkg) == 0 &&
            process_name[pkg.size()] == ':') {
            return true;
        }
    }
    return false;
}

ModuleConfig module_config_parse(const std::string &json_text) {
    ModuleConfig cfg; // defaults

    JsonValue root;
    if (json_text.empty() || !json_parse(json_text, root) || !root.isObject()) {
        return cfg;
    }

    cfg.enabled = root.getBool("enabled", cfg.enabled);
    cfg.proxy_ip = root.getString("proxy_ip", cfg.proxy_ip);
    cfg.proxy_port = static_cast<int>(root.getNumber("proxy_port", cfg.proxy_port));

    if (const JsonValue *arr = root.getArray("target_packages")) {
        cfg.target_packages.clear();
        for (const auto &item : arr->arrValue) {
            if (item.type == JsonValue::Type::String && !item.strValue.empty()) {
                cfg.target_packages.push_back(item.strValue);
            }
        }
    }

    return cfg;
}
