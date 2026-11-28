#include "prx/libSceGnmDriverCompat1/libSceGnmDriverCompat1.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

namespace {

constexpr std::uint32_t ContextState400SdkVersion = 0x04000000;

}

extern "C" {

int APS5_VABI sceGnmDrawInitToDefaultContextStateInternalSizeCompat_nid_disambig1(void) {
    return ApplicationCompiledSdkVersion() < ContextState400SdkVersion ? 0x20 : 0x100;
}

APS5_EXPORT("jajhf-Gi3AI", sceGnmDrawInitToDefaultContextStateInternalSizeCompat_nid_disambig1);

}
