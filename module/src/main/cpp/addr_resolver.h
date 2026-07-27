// FlutterTap native module -- by Eduardo Lopes
//
// Port of the address-resolution logic in flutter+burp.js: given libflutter.so's
// loaded segments, locate (a) the BoringSSL cert-chain verification function
// and (b) the Flutter engine's internal GetSockAddr function, using the same
// string-xref + instruction-pattern approach as the original script (arm64:
// adrp/add pairs; x64: rip-relative lea), disassembled with Capstone -- the
// same engine backing Frida's Instruction.parse.
#pragma once

#include <cstdint>
#include <string>

#include "elf_utils.h"

struct ResolvedAddrs {
    uintptr_t verify_cert_chain = 0;
    uintptr_t get_sock_addr = 0;
};

// `package_name` is currently unused by the resolver itself (kept for
// logging/future use) -- the original script's alibaba.com-specific pattern
// variant is unnecessary here since the scan is register-allocation-agnostic.
// See resolveVerifyCertChainArm64's comment in addr_resolver.cpp.
bool resolve_addresses(const MappedModule &mod, const ElfSegments &segs, const std::string &package_name,
                       ResolvedAddrs &out);
