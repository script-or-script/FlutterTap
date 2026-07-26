// FlutterTap native module -- by Eduardo Lopes
//
// Port of the flutter+burp.js `parseElf()` / module lookup logic (Android
// branch only -- the original script's MachO/iOS branch does not apply here).
#pragma once

#include <sys/types.h>

#include <cstdint>
#include <string>

struct MappedModule {
    uintptr_t base = 0;
    std::string path;
    dev_t dev = 0;
    ino_t inode = 0;
};

// Scans /proc/self/maps for the first mapping whose path ends with
// `name_suffix` (e.g. "libflutter.so"). Returns false if not currently loaded.
bool find_module_by_suffix(const char *name_suffix, MappedModule &out);

struct ElfSegments {
    // First PT_LOAD with p_vaddr == 0 (rodata / everything before .text).
    uint64_t rodata_memsz = 0;
    // First PT_LOAD with p_vaddr != 0 (.text and onwards).
    uint64_t text_vaddr = 0;
    uint64_t text_memsz = 0;
    // PT_GNU_RELRO, if present.
    bool has_relro = false;
    uint64_t relro_vaddr = 0;
    uint64_t relro_memsz = 0;
};

// Parses the 64-bit ELF program headers of `mod` (reading from the mapped
// memory first, falling back to reading the backing file for any field that
// comes back as zero, exactly like the original script's fallback path).
bool parse_elf_segments(const MappedModule &mod, ElfSegments &out);
