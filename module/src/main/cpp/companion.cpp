// FlutterTap native module -- by Eduardo Lopes
#include "companion.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <vector>

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

std::string readConfigFile() {
    FILE *f = fopen(kConfigPath, "rb");
    if (!f) return "";
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
