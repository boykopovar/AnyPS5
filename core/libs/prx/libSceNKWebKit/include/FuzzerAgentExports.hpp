#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

void APS5_VABI _ZN3JSC11FuzzerAgentD0Ev(void* self);
std::uint64_t APS5_VABI _ZN3JSC11FuzzerAgent13getPredictionEPNS_9CodeBlockERKNS_10CodeOriginEm(void* self, void* codeBlock, const void* codeOrigin, std::uint64_t original);

}
