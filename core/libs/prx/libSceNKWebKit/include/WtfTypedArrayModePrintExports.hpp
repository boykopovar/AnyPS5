#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(Jsc::WtfAbi::PrintStream* out, std::uint32_t mode);

}
