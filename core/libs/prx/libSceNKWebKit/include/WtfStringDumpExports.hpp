#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(NKWebKit::WtfDumpAbi::PrintStream* out, const NKWebKit::WtfAbi::StringImpl* string);
void APS5_VABI _ZNK3WTF3URL4dumpERNS_11PrintStreamE(const NKWebKit::WtfAbi::URL* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}
