#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

double APS5_VABI _ZN3WTF12randomNumberEv();
unsigned int APS5_VABI _ZN3WTF16weakRandomUint32Ev();
std::uint64_t APS5_VABI _ZN3WTF20ObjectIdentifierBase26generateIdentifierInternalEv();
std::uint64_t APS5_VABI _ZN3WTF20ObjectIdentifierBase36generateThreadSafeIdentifierInternalEv();
std::uint32_t APS5_VABI _ZN3WTF29cryptographicallyRandomNumberEv();

}
