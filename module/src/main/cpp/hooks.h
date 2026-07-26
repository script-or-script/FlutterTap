// FlutterTap native module -- by Eduardo Lopes
//
// Installs the three hooks from flutter+burp.js:
//   1. GetSockAddr    -- capture the sockaddr pointer about to be used (onEnter style)
//   2. socket()       -- inline-hooked process-wide (same scope as the
//                        original script's Module.getGlobalExportByName("socket"));
//                        overwrite the captured sockaddr with the proxy IP/port
//   3. verify_cert_chain -- force the TLS certificate chain check to pass
//
// Deliberately NOT using zygisk::Api::pltHookRegister for the socket() hook:
// that API is only valid until post[XXX]Specialize returns, but libflutter.so
// (and therefore the earliest point we can even resolve these addresses)
// typically only loads well after that point, during normal app runtime.
// Dobby's hooks have no such lifecycle restriction.
#pragma once

#include "addr_resolver.h"
#include "elf_utils.h"
#include "module_config.h"

namespace hooks {

bool install(const MappedModule &flutter_mod, const ResolvedAddrs &addrs, const ModuleConfig &cfg);

} // namespace hooks
