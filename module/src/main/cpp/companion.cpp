// FlutterTap native module -- by Eduardo Lopes
#include "companion.h"

#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>

#include "log.h"

namespace {

bool writeFull(int fd, const void *buf, size_t size) {
    const auto *p = static_cast<const uint8_t *>(buf);
    size_t written = 0;
    while (written < size) {
        ssize_t n = write(fd, p + written, size - written);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

bool readFull(int fd, void *buf, size_t size) {
    auto *p = static_cast<uint8_t *>(buf);
    size_t got = 0;
    while (got < size) {
        ssize_t n = read(fd, p + got, size - got);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return false;
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

// stdio is fine here -- a small regular file, none of the partial-transfer or
// EINTR concerns that the socket path above has to handle.
std::string readConfigFile() {
    FILE *f = fopen(kConfigPath, "rb");
    if (!f) {
        // Without this, the most common real-world failure (module installed
        // but never configured, or the config unreadable) produces no logcat
        // output at all and looks identical to "configured, no apps selected".
        // Logged once: the companion is a long-lived root daemon and this runs
        // on every app launch, so an unconditional warning would spam logcat.
        static bool warned = false;
        if (!warned) {
            warned = true;
            ft_log_warn("companion: cannot read %s (errno %d)", kConfigPath, errno);
        }
        return "";
    }
    std::string content;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        content.append(buf, n);
    }
    fclose(f);
    return content;
}

} // namespace

void companion_handler(int client_fd) {
    std::string json = readConfigFile();
    auto len = static_cast<uint32_t>(json.size());
    if (!writeFull(client_fd, &len, sizeof(len))) {
        ft_log_warn("companion: failed to write config length");
        return;
    }
    if (len > 0 && !writeFull(client_fd, json.data(), len)) {
        ft_log_warn("companion: failed to write config body");
    }
}

std::string companion_fetch_config(int companion_fd) {
    if (companion_fd < 0) return "";
    uint32_t len = 0;
    if (!readFull(companion_fd, &len, sizeof(len))) return "";
    if (len == 0 || len > 1024 * 1024) return ""; // sanity cap, config.json is tiny
    std::string result(len, '\0');
    if (!readFull(companion_fd, result.data(), len)) return "";
    return result;
}
