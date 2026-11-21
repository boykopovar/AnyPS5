#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/WtfBitVector.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"

namespace NKWebKit::WtfAbi {

struct Thread;

}

extern "C" {

void APS5_VABI _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(const WTF::BitVector* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(const NKWebKit::WtfAbi::Thread* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}
