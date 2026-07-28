// FlutterTap native module -- by Eduardo Lopes
//
// Port of the address-resolution logic in flutter+burp.js: given libflutter.so's
// loaded segments, locate (a) the BoringSSL cert-chain verification function
// and (b) the Flutter engine's internal GetSockAddr function.
//
// It keeps the original script's string-xref strategy (arm64: adrp/add pairs;
// x64: rip-relative lea) but deliberately does NOT reuse its instruction byte
// patterns: those bake in whichever registers the reference binary's compiler
// happened to pick, so they break on other builds. Instead the scan matches
// mnemonics via Capstone -- the same disassembler backing Frida's
// Instruction.parse. See addr_resolver.cpp for the full reasoning.
#pragma once

#include <cstdint>

#include "elf_utils.h"

struct ResolvedAddrs {
    uintptr_t verify_cert_chain = 0;
    uintptr_t get_sock_addr = 0;
};

bool resolve_addresses(const MappedModule &mod, const ElfSegments &segs, ResolvedAddrs &out);
