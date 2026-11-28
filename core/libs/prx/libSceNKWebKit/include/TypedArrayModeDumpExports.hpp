#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(NKWebKit::WtfDumpAbi::PrintStream* out, std::uint8_t mode);

}
