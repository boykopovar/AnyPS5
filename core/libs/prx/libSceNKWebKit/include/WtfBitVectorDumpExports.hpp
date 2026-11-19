#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libSceNKWebKit/include/WtfBitVector.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

void APS5_VABI _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(const WTF::BitVector* self, Jsc::WtfAbi::PrintStream* out);

}
