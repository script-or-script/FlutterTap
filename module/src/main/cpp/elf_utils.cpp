// FlutterTap native module -- by Eduardo Lopes
#include "elf_utils.h"

#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

#include "log.h"

namespace {

template <typename T>
T readMem(uintptr_t addr) {
    T value;
    memcpy(&value, reinterpret_cast<void *>(addr), sizeof(T));
    return value;
}

// Same defensive "known program header types" whitelist the original script
// used; an unrecognized p_type stops the walk, matching flutter+burp.js.
bool isKnownPType(uint32_t p_type) {
    switch (p_type) {
        case PT_NULL:
        case PT_LOAD:
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_NOTE:
        case PT_SHLIB:
        case PT_PHDR:
        case PT_TLS:
        case PT_GNU_EH_FRAME:
        case PT_GNU_STACK:
        case PT_GNU_RELRO:
        case PT_GNU_PROPERTY:
            return true;
        default:
            return false;
    }
}

struct FindModuleCtx {
    const char *suffix;
    size_t suffixLen;
    bool found = false;
    uintptr_t base = 0;
    std::string path;
};

int findModuleCallback(struct dl_phdr_info *info, size_t, void *data) {
    auto *ctx = static_cast<FindModuleCtx *>(data);
    if (!info->dlpi_name) return 0;
    size_t nameLen = strlen(info->dlpi_name);
    if (nameLen < ctx->suffixLen) return 0;
    if (strcmp(info->dlpi_name + (nameLen - ctx->suffixLen), ctx->suffix) != 0) return 0;

    ctx->found = true;
    ctx->base = static_cast<uintptr_t>(info->dlpi_addr);
    ctx->path = info->dlpi_name;
    return 1; // non-zero stops dl_iterate_phdr from visiting further modules
}

} // namespace

bool find_module_by_suffix(const char *name_suffix, MappedModule &out) {
    // dl_iterate_phdr (not /proc/self/maps path matching) so this also works
    // when the APK was built with extractNativeLibs=false and the library is
    // mapped directly out of the APK's zip rather than as its own file --
    // bionic still reports dlpi_addr/dlpi_name correctly either way.
    FindModuleCtx ctx{name_suffix, strlen(name_suffix)};
    dl_iterate_phdr(findModuleCallback, &ctx);
    if (!ctx.found) return false;

    out.base = ctx.base;
    out.path = ctx.path;
    return true;
}

bool parse_elf_segments(const MappedModule &mod, ElfSegments &out) {
    Elf64_Ehdr ehdr = readMem<Elf64_Ehdr>(mod.base);
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        ft_log_warn("elf: bad magic for %s", mod.path.c_str());
        return false;
    }

    // When the library is mapped directly out of the APK (path looks like
    // ".../base.apk!/lib/arm64-v8a/libflutter.so"), there is no standalone
    // file to open at that path -- the ELF itself starts at a byte offset
    // inside base.apk that would need its own zip parsing to locate. Skip
    // the file-fallback entirely in that case; the in-memory reads above are
    // the primary path anyway and normally sufficient (the ELF/program
    // headers are always resident once the module is mapped).
    int fd = mod.path.find("!/") == std::string::npos ? open(mod.path.c_str(), O_RDONLY) : -1;

    auto readEhdrFromFile = [&]() -> Elf64_Ehdr {
        Elf64_Ehdr fileHdr{};
        if (fd >= 0) {
            lseek(fd, 0, SEEK_SET);
            read(fd, &fileHdr, sizeof(fileHdr));
        }
        return fileHdr;
    };

    uint64_t phoff = ehdr.e_phoff;
    uint64_t shoff = ehdr.e_shoff;
    uint16_t phentsize = ehdr.e_phentsize;
    uint16_t phnum = ehdr.e_phnum;

    if (shoff == 0 && fd >= 0) {
        ft_log_info("elf: shoff is 0, falling back to file read");
        shoff = readEhdrFromFile().e_shoff;
    }
    if (phentsize != sizeof(Elf64_Phdr)) {
        ft_log_warn("elf: unexpected e_phentsize=%u, assuming %zu", phentsize, sizeof(Elf64_Phdr));
        phentsize = sizeof(Elf64_Phdr);
    }
    if (phnum == 0) {
        if (fd >= 0) {
            phnum = readEhdrFromFile().e_phnum;
        }
        if (phnum == 0) {
            ft_log_warn("elf: phnum is 0, assuming 10 (only need to reach PT_GNU_RELRO)");
            phnum = 10;
        }
    }

    bool foundRodataSegment = false;
    bool foundTextSegment = false;
    for (uint16_t i = 0; i < phnum; i++) {
        uintptr_t phdrAddr = mod.base + phoff + static_cast<uint64_t>(i) * phentsize;
        Elf64_Phdr phdr = readMem<Elf64_Phdr>(phdrAddr);

        if (phdr.p_type == 0 && fd >= 0) {
            // Could be a relocation-in-progress artifact; confirm against the file.
            Elf64_Phdr filePhdr{};
            lseek(fd, static_cast<off_t>(phoff + static_cast<uint64_t>(i) * phentsize), SEEK_SET);
            read(fd, &filePhdr, sizeof(filePhdr));
            phdr.p_type = filePhdr.p_type;
        }

        if (!isKnownPType(phdr.p_type)) break;

        if (phdr.p_flags == 0 && fd >= 0) {
            Elf64_Phdr filePhdr{};
            lseek(fd, static_cast<off_t>(phoff + static_cast<uint64_t>(i) * phentsize), SEEK_SET);
            read(fd, &filePhdr, sizeof(filePhdr));
            phdr = filePhdr;
        }

        if (phdr.p_type == PT_LOAD) {
            // The original script (and this port, until this fix) assumed the
            // classic layout: a rodata-only PT_LOAD at vaddr 0, then a
            // separate executable PT_LOAD at a nonzero vaddr. Newer lld
            // defaults (e.g. --no-rosegment, now common in recently-built
            // libflutter.so) instead merge rodata and .text into a single
            // R+E PT_LOAD starting at vaddr 0 -- confirmed against a real
            // APK during testing. Using PF_X to find "the executable
            // segment" handles both layouts: for the classic layout it's
            // still the second PT_LOAD; for the merged layout it's the same
            // first PT_LOAD that also supplied rodata_memsz.
            if (!foundRodataSegment) {
                out.rodata_memsz = phdr.p_memsz;
                foundRodataSegment = true;
            }
            if (!foundTextSegment && (phdr.p_flags & PF_X)) {
                out.text_vaddr = phdr.p_vaddr;
                out.text_memsz = phdr.p_memsz;
                foundTextSegment = true;
            }
            continue;
        }
        if (phdr.p_type == PT_GNU_RELRO) {
            out.relro_vaddr = phdr.p_vaddr;
            out.relro_memsz = phdr.p_memsz;
            out.has_relro = true;
            break;
        }
    }

    if (fd >= 0) close(fd);
    (void)shoff; // kept for parity with the original script; not needed further here
    return true;
}
