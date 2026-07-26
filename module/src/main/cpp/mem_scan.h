// FlutterTap native module -- by Eduardo Lopes
//
// Port of flutter+burp.js `scanMemory()` (the generic byte-pattern / AOB scan
// part; the arch-specific instruction decoding lives in addr_resolver.*).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Installs a SIGSEGV/SIGBUS guard so a bad computed address (e.g. a scan
// landing on a partial/incorrectly classified page) logs and aborts the scan
// instead of crashing the host app process. Call once, early, from the
// module's onLoad. Not present in the original one-shot Frida script (Frida's
// JS runtime already isolates crashes) but necessary here since we run
// in-process for the whole app lifetime.
void mem_scan_install_crash_guard();

// Guarded memcpy: returns false (instead of crashing) if `src` turns out to
// be unreadable. Shared by mem_scan.cpp and addr_resolver.cpp.
bool safe_read(void *dst, const void *src, size_t n);

// value/mask pairs: for byte i, memory matches iff (memory[i] & mask[i]) == (value[i] & mask[i]).
// mask 0x00 == wildcard byte ("??" in the original hex-pattern strings),
// partial masks (e.g. 0x0F) implement the nibble wildcards ("?9", "?0", ...).
struct BytePattern {
    std::vector<uint8_t> value;
    std::vector<uint8_t> mask;
};

// Parses patterns like "?9 ?? ?? ?0 29 ?? ?? 91" or "73 73 6C 5F 63 6C 69 65 6E 74 00".
BytePattern compile_pattern(const std::string &pattern_str);

// Returns every match address in [start, start+size) in ascending order.
// Reads are guarded; if `start` turns out to be unmapped/unreadable the
// function returns an empty result instead of crashing.
std::vector<uintptr_t> scan_all_matches(uintptr_t start, size_t size, const BytePattern &pattern);
