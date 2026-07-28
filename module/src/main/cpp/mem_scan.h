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
// instead of crashing the host app process. Not present in the original
// one-shot Frida script (Frida's JS runtime already isolates crashes) but
// necessary here since we run in-process for the whole app lifetime.
//
// Call once per process, and ONLY from a process this library stays loaded in
// for good -- i.e. a confirmed hook target, never from onLoad(). The handler
// address points into this library, so installing it in a process that later
// DLCLOSEs us leaves the kernel jumping into unmapped memory on the next
// signal. See the call site in main.cpp for why that was fatal under the
// Zygisk Next Linker.
void mem_scan_install_crash_guard();

// Guarded memcpy: returns false (instead of crashing) if `src` turns out to
// be unreadable. Shared by mem_scan.cpp, addr_resolver.cpp and hooks.cpp.
bool safe_read(void *dst, const void *src, size_t n);

// value/mask pairs: for byte i, memory matches iff (memory[i] & mask[i]) == (value[i] & mask[i]).
// mask 0x00 == wildcard byte ("??" in the original hex-pattern strings),
// partial masks (e.g. 0x0F) implement the nibble wildcards ("?9", "?0", ...).
struct BytePattern {
    std::vector<uint8_t> value;
    std::vector<uint8_t> mask;
};

// Parses space-separated hex byte patterns, e.g. "73 73 6C 5F 63 6C 69 65 6E 74 00"
// for a literal string match, or "4? ?? B8" using "??"/nibble wildcards. Kept
// general-purpose like the original script's AOB scanner, even though the
// current callers (addr_resolver.cpp) only need literal string matches with
// no wildcards -- the register-agnostic Capstone scan replaced the wildcarded
// instruction-byte patterns the original script used.
BytePattern compile_pattern(const std::string &pattern_str);

// Returns every match address in [start, start+size) in ascending order.
// Reads are guarded; if `start` turns out to be unmapped/unreadable the
// function returns an empty result instead of crashing.
std::vector<uintptr_t> scan_all_matches(uintptr_t start, size_t size, const BytePattern &pattern);
