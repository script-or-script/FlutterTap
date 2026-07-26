// FlutterTap native module -- by Eduardo Lopes
//
// The Zygisk companion process runs as full root, unaffected by the SELinux
// domain restrictions applied to the zygote/app process. We use it purely to
// read the persistent config file, since preAppSpecialize (which runs with
// zygote's own restricted policy) is not guaranteed to have read access to
// files under /data/adb.
#pragma once

#include <string>

// Path is intentionally OUTSIDE /data/adb/modules/<id> so the user's
// settings survive module updates/reinstalls.
inline constexpr const char *kConfigPath = "/data/adb/fluttertap/config.json";

// Registered via REGISTER_ZYGISK_COMPANION. Runs in the root daemon process.
void companion_handler(int client_fd);

// Called from the module process (preAppSpecialize) after api->connectCompanion().
// Returns the raw JSON text sent by the companion, or "" on any IO error.
std::string companion_fetch_config(int companion_fd);
