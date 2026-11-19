#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(Jsc::WtfAbi::PrintStream* out, const NKWebKit::WtfAbi::StringImpl* string);

}
