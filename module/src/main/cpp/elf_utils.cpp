// FlutterTap native module -- by Eduardo Lopes
#include "elf_utils.h"

#include <elf.h>
#include <fcntl.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

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

} // namespace

bool find_module_by_suffix(const char *name_suffix, MappedModule &out) {
    std::ifstream maps("/proc/self/maps");
    if (!maps.is_open()) return false;

    std::string line;
    const size_t suffix_len = strlen(name_suffix);
    while (std::getline(maps, line)) {
        if (line.size() < suffix_len) continue;
        if (line.compare(line.size() - suffix_len, suffix_len, name_suffix) != 0) continue;

        // "<start>-<end> <perms> <offset> <dev-maj>:<dev-min> <inode> <path>"
        std::istringstream iss(line);
        std::string addrRange, perms, offset, devStr, inodeStr, path;
        iss >> addrRange >> perms >> offset >> devStr >> inodeStr;
        std::getline(iss, path);
        size_t firstNonSpace = path.find_first_not_of(' ');
        path = firstNonSpace == std::string::npos ? "" : path.substr(firstNonSpace);

        uintptr_t start = std::stoull(addrRange.substr(0, addrRange.find('-')), nullptr, 16);

        unsigned int devMajor = 0, devMinor = 0;
        sscanf(devStr.c_str(), "%x:%x", &devMajor, &devMinor);

        out.base = start;
        out.path = path;
        out.dev = makedev(devMajor, devMinor);
        out.inode = static_cast<ino_t>(std::stoull(inodeStr));
        return true;
    }
    return false;
}

bool parse_elf_segments(const MappedModule &mod, ElfSegments &out) {
    Elf64_Ehdr ehdr = readMem<Elf64_Ehdr>(mod.base);
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        ft_log_warn("elf: bad magic for %s", mod.path.c_str());
        return false;
    }

    int fd = open(mod.path.c_str(), O_RDONLY);

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

        if (phdr.p_type == PT_LOAD && phdr.p_vaddr == 0) {
            out.rodata_memsz = phdr.p_memsz;
            continue;
        }
        if (phdr.p_type == PT_LOAD && phdr.p_vaddr != 0) {
            if (!foundTextSegment) {
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
