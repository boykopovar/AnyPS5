#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"

extern "C" {

void APS5_VABI _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(const WebCore::PlatformTimeRanges* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}
