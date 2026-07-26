// FlutterTap native module -- by Eduardo Lopes
#include "addr_resolver.h"

#include <capstone/capstone.h>

#include <cstring>
#include <string>

#include "log.h"
#include "mem_scan.h"

namespace {

struct CapstoneHandles {
    csh arm64 = 0;
    csh x86 = 0;
    bool arm64_ok = false;
    bool x86_ok = false;
};

CapstoneHandles openHandles() {
    CapstoneHandles h;
    if (cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &h.arm64) == CS_ERR_OK) {
        cs_option(h.arm64, CS_OPT_DETAIL, CS_OPT_ON);
        h.arm64_ok = true;
    }
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &h.x86) == CS_ERR_OK) {
        cs_option(h.x86, CS_OPT_DETAIL, CS_OPT_ON);
        h.x86_ok = true;
    }
    return h;
}

void closeHandles(CapstoneHandles &h) {
    if (h.arm64_ok) cs_close(&h.arm64);
    if (h.x86_ok) cs_close(&h.x86);
}

// Disassembles exactly one instruction at `addr` (reading its bytes with the
// SIGSEGV-guarded safe_read, since `addr` comes from a heuristic scan and may
// not be valid). `fn` receives the decoded cs_insn by reference and its
// result becomes disasmOne's own result.
template <typename Fn>
bool disasmOne(csh handle, uintptr_t addr, Fn &&fn) {
    uint8_t buf[16] = {0};
    if (!safe_read(buf, reinterpret_cast<void *>(addr), sizeof(buf))) return false;
    cs_insn *insn = nullptr;
    size_t count = cs_disasm(handle, buf, sizeof(buf), addr, 1, &insn);
    if (count == 0) return false;
    bool ok = fn(insn[0]);
    cs_free(insn, count);
    return ok;
}

BytePattern compile_pointer_pattern(uintptr_t addr) {
    BytePattern p;
    for (int i = 0; i < 8; i++) {
        p.value.push_back(static_cast<uint8_t>((addr >> (i * 8)) & 0xFF));
        p.mask.push_back(0xFF);
    }
    return p;
}

// arm64: locate the adrp/add pair that materializes the address of the
// "ssl_client" string, then backtrace to the enclosing function's prologue
// (sub sp, sp, #N ; stp|str ...). Mirrors the "ssl_client_adrp_add" branch
// of flutter+burp.js exactly, including that later matches overwrite earlier
// ones (there is no early break in the original onMatch/onComplete flow).
uintptr_t resolveVerifyCertChainArm64(csh handle, const MappedModule &mod, const ElfSegments &segs,
                                       uintptr_t sslClientAddr, const std::string &pkg) {
    std::string patternStr = "?9 ?? ?? ?0 29 ?? ?? 91";
    if (pkg == "com.alibaba.intl.android.apps.poseidon") {
        patternStr = "?9 ?? ?? ?0 ?? ?? ?? ?? 29 ?? ?? 91";
    }
    BytePattern pat = compile_pattern(patternStr);
    uintptr_t textBase = mod.base + segs.text_vaddr;
    auto matches = scan_all_matches(textBase, segs.text_memsz, pat);

    uintptr_t result = 0;
    for (uintptr_t matchAddr : matches) {
        int64_t adrpTarget = 0;
        bool gotAdrp = disasmOne(handle, matchAddr, [&](cs_insn &insn) {
            if (strcmp(insn.mnemonic, "adrp") != 0 || !insn.detail) return false;
            for (int i = 0; i < insn.detail->arm64.op_count; i++) {
                if (insn.detail->arm64.operands[i].type == ARM64_OP_IMM) {
                    adrpTarget = insn.detail->arm64.operands[i].imm;
                    return true;
                }
            }
            return false;
        });
        if (!gotAdrp) continue;

        int64_t addImm = 0;
        auto tryAdd = [&](uintptr_t addr) {
            return disasmOne(handle, addr, [&](cs_insn &insn) {
                if (strcmp(insn.mnemonic, "add") != 0 || !insn.detail) return false;
                for (int i = 0; i < insn.detail->arm64.op_count; i++) {
                    if (insn.detail->arm64.operands[i].type == ARM64_OP_IMM) {
                        addImm = insn.detail->arm64.operands[i].imm;
                        return true;
                    }
                }
                return false;
            });
        };
        // Matches the JS "if next isn't add, look one instruction further" fallback.
        bool gotAdd = tryAdd(matchAddr + 4) || tryAdd(matchAddr + 8);
        if (!gotAdd) continue;

        if (static_cast<uintptr_t>(adrpTarget + addImm) != sslClientAddr) continue;

        // Backtrace looking for `sub` immediately followed by `stp`/`str` (function prologue).
        constexpr uint32_t kMaxBacktrace = 0x8000;
        for (uint32_t off = 0; off < kMaxBacktrace; off += 4) {
            uintptr_t subAddr = matchAddr - off;
            bool isSub = disasmOne(handle, subAddr, [](cs_insn &insn) { return strcmp(insn.mnemonic, "sub") == 0; });
            if (!isSub) continue;
            bool isPrologue = disasmOne(handle, subAddr + 4, [](cs_insn &insn) {
                return strcmp(insn.mnemonic, "stp") == 0 || strcmp(insn.mnemonic, "str") == 0;
            });
            if (isPrologue) {
                result = subAddr; // last valid candidate wins, keep iterating remaining matches
                break;
            }
        }
    }
    return result;
}

// x64: locate `lea rdi, [rip+disp]` referencing the "ssl_client" string, then
// brute-force step backwards one byte at a time (exactly like the original
// script's try/catch loop) looking for `push rbp` immediately followed by
// `push r15`.
uintptr_t resolveVerifyCertChainX64(csh handle, const MappedModule &mod, const ElfSegments &segs,
                                     uintptr_t sslClientAddr) {
    BytePattern pat = compile_pattern("48 8d 3d ?? ?? ?? FF");
    uintptr_t textBase = mod.base + segs.text_vaddr;
    auto matches = scan_all_matches(textBase, segs.text_memsz, pat);

    uintptr_t result = 0;
    for (uintptr_t matchAddr : matches) {
        int64_t disp = 0;
        uint64_t ripValue = 0;
        bool gotLea = disasmOne(handle, matchAddr, [&](cs_insn &insn) {
            if (strcmp(insn.mnemonic, "lea") != 0 || !insn.detail) return false;
            for (int i = 0; i < insn.detail->x86.op_count; i++) {
                if (insn.detail->x86.operands[i].type == X86_OP_MEM) {
                    disp = insn.detail->x86.operands[i].mem.disp;
                    ripValue = insn.address + insn.size;
                    return true;
                }
            }
            return false;
        });
        if (!gotLea) continue;
        if (static_cast<uintptr_t>(ripValue + disp) != sslClientAddr) continue;

        constexpr uint32_t kMaxBacktrace = 0x8000;
        for (uint32_t off = 0; off < kMaxBacktrace; off += 1) {
            uintptr_t addr = matchAddr - off;
            bool isPushRbp =
                disasmOne(handle, addr, [](cs_insn &insn) {
                    return strcmp(insn.mnemonic, "push") == 0 && strcmp(insn.op_str, "rbp") == 0;
                });
            if (!isPushRbp) continue;

            uint64_t nextAddr = 0;
            disasmOne(handle, addr, [&](cs_insn &insn) {
                nextAddr = insn.address + insn.size;
                return true;
            });
            bool isPushR15 = disasmOne(handle, nextAddr, [](cs_insn &insn) {
                return strcmp(insn.mnemonic, "push") == 0 && strcmp(insn.op_str, "r15") == 0;
            });
            if (isPushR15) {
                result = addr;
                break;
            }
        }
    }
    return result;
}

uintptr_t walkBlCountArm64(csh handle, uintptr_t funcAddr, int targetCount) {
    int count = 0;
    constexpr uint32_t kMaxWalk = 0x2000;
    for (uint32_t off = 0; off < kMaxWalk; off += 4) {
        uintptr_t addr = funcAddr + off;
        int64_t target = 0;
        bool isBl = disasmOne(handle, addr, [&](cs_insn &insn) {
            if (strcmp(insn.mnemonic, "bl") != 0 || !insn.detail) return false;
            for (int i = 0; i < insn.detail->arm64.op_count; i++) {
                if (insn.detail->arm64.operands[i].type == ARM64_OP_IMM) {
                    target = insn.detail->arm64.operands[i].imm;
                    return true;
                }
            }
            return false;
        });
        if (isBl) {
            count++;
            if (count == targetCount) return static_cast<uintptr_t>(target);
        }
    }
    return 0;
}

// Mirrors the original script's byte-by-byte `call` counting on x64 (it does
// not step by proper instruction length there either).
uintptr_t walkCallCountX64(csh handle, uintptr_t funcAddr, int targetCount) {
    int count = 0;
    constexpr uint32_t kMaxWalk = 0x8000;
    for (uint32_t off = 0; off < kMaxWalk; off += 1) {
        uintptr_t addr = funcAddr + off;
        int64_t target = 0;
        bool isCall = disasmOne(handle, addr, [&](cs_insn &insn) {
            if (strcmp(insn.mnemonic, "call") != 0 || !insn.detail) return false;
            for (int i = 0; i < insn.detail->x86.op_count; i++) {
                if (insn.detail->x86.operands[i].type == X86_OP_IMM) {
                    target = insn.detail->x86.operands[i].imm;
                    return true;
                }
            }
            return false;
        });
        if (isCall) {
            count++;
            if (count == targetCount) return static_cast<uintptr_t>(target);
        }
    }
    return 0;
}

} // namespace

bool resolve_addresses(const MappedModule &mod, const ElfSegments &segs, const std::string &package_name,
                       ResolvedAddrs &out) {
    if (segs.rodata_memsz == 0) {
        ft_log_warn("resolver: rodata size is 0, cannot scan");
        return false;
    }

    BytePattern sslClientPat = compile_pattern("73 73 6C 5F 63 6C 69 65 6E 74 00");
    BytePattern socketCcPat = compile_pattern("53 6f 63 6b 65 74 5f 43 72 65 61 74 65 43 6f 6e 6e 65 63 74 00");

    auto sslClientMatches = scan_all_matches(mod.base, segs.rodata_memsz, sslClientPat);
    auto socketCcMatches = scan_all_matches(mod.base, segs.rodata_memsz, socketCcPat);

    if (sslClientMatches.empty()) {
        ft_log_warn("resolver: 'ssl_client' string not found in %s", mod.path.c_str());
        return false;
    }
    if (socketCcMatches.empty()) {
        ft_log_warn("resolver: 'Socket_CreateConnect' string not found in %s", mod.path.c_str());
        return false;
    }

    uintptr_t sslClientAddr = sslClientMatches.back();
    uintptr_t socketCcStringAddr = socketCcMatches.back();

    CapstoneHandles cs = openHandles();

    uintptr_t verifyCertChain = 0;
#if defined(__aarch64__)
    if (!cs.arm64_ok) {
        ft_log_error("resolver: failed to open capstone (arm64)");
        closeHandles(cs);
        return false;
    }
    verifyCertChain = resolveVerifyCertChainArm64(cs.arm64, mod, segs, sslClientAddr, package_name);
#elif defined(__x86_64__)
    if (!cs.x86_ok) {
        ft_log_error("resolver: failed to open capstone (x86_64)");
        closeHandles(cs);
        return false;
    }
    verifyCertChain = resolveVerifyCertChainX64(cs.x86, mod, segs, sslClientAddr);
#else
    ft_log_warn("resolver: unsupported architecture (only arm64-v8a and x86_64 are supported)");
    closeHandles(cs);
    return false;
#endif

    if (verifyCertChain == 0) {
        ft_log_warn("resolver: could not locate verify_cert_chain");
        closeHandles(cs);
        return false;
    }

    if (!segs.has_relro) {
        ft_log_warn("resolver: %s has no PT_GNU_RELRO segment", mod.path.c_str());
        closeHandles(cs);
        return false;
    }

    BytePattern ptrPat = compile_pointer_pattern(socketCcStringAddr);
    auto relroMatches = scan_all_matches(mod.base + segs.relro_vaddr, segs.relro_memsz, ptrPat);
    if (relroMatches.empty()) {
        ft_log_warn("resolver: Socket_CreateConnect pointer not found in PT_GNU_RELRO");
        closeHandles(cs);
        return false;
    }
    uintptr_t slotAddr = relroMatches.back();

    uintptr_t socketCreateConnectFunc = 0;
    if (!safe_read(&socketCreateConnectFunc, reinterpret_cast<void *>(slotAddr - 0x10),
                    sizeof(socketCreateConnectFunc))) {
        ft_log_warn("resolver: failed reading Socket_CreateConnect function pointer");
        closeHandles(cs);
        return false;
    }

    uintptr_t getSockAddr = 0;
#if defined(__aarch64__)
    getSockAddr = walkBlCountArm64(cs.arm64, socketCreateConnectFunc, 2);
#elif defined(__x86_64__)
    getSockAddr = walkCallCountX64(cs.x86, socketCreateConnectFunc, 2);
#endif

    closeHandles(cs);

    if (getSockAddr == 0) {
        ft_log_warn("resolver: could not locate GetSockAddr");
        return false;
    }

    out.verify_cert_chain = verifyCertChain;
    out.get_sock_addr = getSockAddr;
    return true;
}
