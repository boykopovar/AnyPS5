#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"
#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

extern "C" {

void APS5_VABI _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(const NKWebKit::HttpAbi::ASCIILiteral* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF13MonotonicTime4dumpERNS_11PrintStreamE(const WTF::MonotonicTime* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF24TimeWithDynamicClockType4dumpERNS_11PrintStreamE(const WTF::TimeWithDynamicClockType* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(const WTF::Seconds* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF8WallTime4dumpERNS_11PrintStreamE(const WTF::WallTime* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(const WTF::MediaTime* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}
