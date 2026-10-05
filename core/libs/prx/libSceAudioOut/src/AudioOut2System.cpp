#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int USER_ID_SYSTEM = 0xFF;
static constexpr std::uint32_t SUPPORTED_3D_LATENCY = 2;

extern "C" {

int APS5_VABI sceAudioOut2Initialize(void) {
    return 0;
}

int APS5_VABI sceAudioOut2Set3DLatency(int userId, std::uint32_t latency) {
    if (userId != USER_ID_SYSTEM || latency != SUPPORTED_3D_LATENCY) NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2GetSystemState(AudioOut2SystemState* state) {
    if (!state) return static_cast<int>(0x80260502);
    state->loudness = 0.0f;
    return 0;
}

int APS5_VABI sceAudioOut2SetSystemDebugState(const AudioOut2SystemDebugStateParam* param) {
    (void)param;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
