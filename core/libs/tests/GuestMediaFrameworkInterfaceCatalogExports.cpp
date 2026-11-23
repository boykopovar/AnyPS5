#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "prx/libSceMediaFrameworkInterface/include/InventoryExports.hpp"

namespace {

using ThrowingStub = int (APS5_VABI *)(void);

const ThrowingStub kStubs[] = {
    sceMediaFwAddEsvmDecoder2,
    sceMediaFwAudioSyncFlush,
    sceMediaFwChangeVideoOutputMode,
    sceMediaFwChangeVideoOutputMode2,
    sceMediaFwInitializeCdm,
    sceMediaFwMediaKeySystemAccessCreateMediaKeys2,
    sceMediaFwMediaKeysSetServerCertificate2,
    sceMediaFwRegisterSrcAllocateStateChangeCallback,
    sceMediaFwReleaseVideoFrame,
    sceMediaFwRequestMediaKeySystemAccess2,
    sceMediaFwSetAudioTagFeature,
    sceMediaFwSetUniversalEsvm,
    sceMediaFwSourceBufferCanSwitchToType,
    sceMediaFwSyncVideo,
};

const char* kStubNames[] = {
    "sceMediaFwAddEsvmDecoder2",
    "sceMediaFwAudioSyncFlush",
    "sceMediaFwChangeVideoOutputMode",
    "sceMediaFwChangeVideoOutputMode2",
    "sceMediaFwInitializeCdm",
    "sceMediaFwMediaKeySystemAccessCreateMediaKeys2",
    "sceMediaFwMediaKeysSetServerCertificate2",
    "sceMediaFwRegisterSrcAllocateStateChangeCallback",
    "sceMediaFwReleaseVideoFrame",
    "sceMediaFwRequestMediaKeySystemAccess2",
    "sceMediaFwSetAudioTagFeature",
    "sceMediaFwSetUniversalEsvm",
    "sceMediaFwSourceBufferCanSwitchToType",
    "sceMediaFwSyncVideo"
};

void RequireThrowsStub(ThrowingStub stub, const char* name, unsigned line) {
    try {
        stub();
    } catch (const std::runtime_error&) {
        return;
    }
    std::fprintf(stderr, "libSceMediaFrameworkInterface stub did not throw: %s (line %u)\n", name, line);
    std::abort();
}

}

int main() {
    static_assert(sizeof(kStubs) / sizeof(kStubs[0]) == 14, "catalog declaration count changed");
    for (unsigned i = 0; i < sizeof(kStubs) / sizeof(kStubs[0]); ++i) {
        RequireThrowsStub(kStubs[i], kStubNames[i], __LINE__);
    }
    return 0;
}
