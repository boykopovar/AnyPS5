#include "prx/libSceGnmDriverCompat1/libSceGnmDriverCompat1.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

namespace {

constexpr std::uint32_t ContextState400SdkVersion = 0x04000000;

std::uint32_t CompiledSdkVersion() {
    const auto* process = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t version;
    std::memcpy(&version, process + 0x10, sizeof(version));
    return version;
}

}

extern "C" {

int APS5_VABI sceGnmDrawInitToDefaultContextStateInternalSizeCompat_nid_disambig1(void) {
    return CompiledSdkVersion() < ContextState400SdkVersion ? 0x20 : 0x100;
}

APS5_EXPORT("jajhf-Gi3AI", sceGnmDrawInitToDefaultContextStateInternalSizeCompat_nid_disambig1);

}
