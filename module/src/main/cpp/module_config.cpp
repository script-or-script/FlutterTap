// FlutterTap native module -- by Eduardo Lopes
#include "module_config.h"

#include "json_min.h"
#include "log.h"

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
    if (json_text.empty()) {
        // Nothing to report here -- the companion already warns once when the
        // config file is missing or unreadable.
        return cfg;
    }
    if (!json_parse(json_text, root) || !root.isObject()) {
        // Falling back to the defaults means an empty target list, i.e. the
        // module silently does nothing. Worth one line so a hand-edited config
        // with a stray comma is diagnosable.
        ft_log_warn("config: parse failed (%zu bytes), using defaults", json_text.size());
        return cfg;
    }

    cfg.enabled = root.getBool("enabled", cfg.enabled);
    cfg.proxy_ip = root.getString("proxy_ip", cfg.proxy_ip);

    // Range-checked before the narrowing cast to uint16_t in hooks.cpp: an
    // out-of-range value like 80833 would otherwise wrap silently to 15297,
    // and even the log line would show the wrapped number.
    const double port = root.getNumber("proxy_port", cfg.proxy_port);
    if (port >= 1 && port <= 65535) {
        cfg.proxy_port = static_cast<int>(port);
    } else {
        ft_log_warn("config: proxy_port %g out of range, using %d", port, cfg.proxy_port);
    }

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
