// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
using CleanupFunction = std::int8_t (APS5_VABI*)();
}

extern "C" {
void APS5_VABI ucln_registerCleanup_nid_postfix(std::int32_t, Icu::CleanupFunction);
void APS5_VABI ucln_cleanupOne_nid_postfix(std::int32_t);
}
