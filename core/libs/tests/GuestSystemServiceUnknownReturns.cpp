#include "prx/libSceSystemService/libSceSystemService.hpp"

#include <cstdlib>

int main() {
    constexpr int unavailable = static_cast<int>(0x80A10002u);
    constexpr int rejected = static_cast<int>(0x80A10005u);

    libSceSystemServiceUnknown_kDGjRMtUDxo_nid_no_patch();
    libSceSystemServiceUnknown_qdTPJWjquDc_nid_no_patch();
    if (libSceSystemServiceUnknown_OyTwhCKWXi4_nid_no_patch() != static_cast<int>(0x80F50013u)) std::abort();
    if (libSceSystemServiceUnknown_G3oPdw3lkps_nid_no_patch() != static_cast<int>(0x80F50013u)) std::abort();

    if (sceSystemServiceAcquireFb0() != unavailable) std::abort();
    if (sceSystemServiceDisablePartyVoice() != unavailable) std::abort();
    if (sceSystemServiceDisableVoiceRecognition() != unavailable) std::abort();
    if (sceSystemServiceReenablePartyVoice() != unavailable) std::abort();
    if (sceSystemServiceReenableVoiceRecognition() != unavailable) std::abort();
    if (sceSystemServiceReleaseFb0() != unavailable) std::abort();
    if (sceSystemServiceLoadExecVideoServiceWebApp() != unavailable) std::abort();
    if (sceSystemServiceLaunchUdsApp() != rejected) std::abort();
    if (sceSystemServiceRequestToChangeRenderingMode() != rejected) std::abort();
}
