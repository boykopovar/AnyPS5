// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <stdexcept>

namespace {

std::array<Icu::CleanupFunction, 8> cleanupFunctions{};

}

extern "C" {

void APS5_VABI ucln_registerCleanup_nid_postfix(std::int32_t type, Icu::CleanupFunction function) {
    if (type >= 0 && type < 8) cleanupFunctions[type] = function;
}

void APS5_VABI ucln_cleanupOne_nid_postfix(std::int32_t type) {
    if (type < 0 || type >= 8) throw std::invalid_argument("ICU cleanup: library type out of range");
    if (!cleanupFunctions[type]) return;
    cleanupFunctions[type]();
    cleanupFunctions[type] = nullptr;
}

}
