#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamERKNS_6StringE(Jsc::WtfAbi::PrintStream* out, const NKWebKit::WtfAbi::String* string);
void APS5_VABI _ZNK3WTF3URL4dumpERNS_11PrintStreamE(const NKWebKit::WtfAbi::URL* self, Jsc::WtfAbi::PrintStream* out);

}
