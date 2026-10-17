#include <atomic>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "AudioOut2Internal.hpp"

static constexpr std::uint32_t MAX_SUPPORTED_3D_OUTPUT = 2;
static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_PARAM = static_cast<int>(0x80268001);

static std::atomic<bool> g_chatEnabled{false};

extern "C" {

int APS5_VABI sceAudioOut2Initialize(void) {
    return 0;
}

int APS5_VABI sceAudioOut2Set3DLatency(int userId, std::uint32_t output) {
    (void)userId;
    if (output > MAX_SUPPORTED_3D_OUTPUT) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    return 0;
}

int APS5_VABI sceAudioOut2GetSystemState(AudioOut2SystemState* state) {
    if (!state) return static_cast<int>(0x80260502);
    state->loudness = 0.0f;
    return 0;
}

int APS5_VABI sceAudioOut2SetSystemDebugState(const AudioOut2SystemDebugStateParam* param) {
    if (param == nullptr) return static_cast<int>(0x80260502);
    AUDIOOUT2_TRACE("t=%.3f SetSystemDebugState: debug state %u recorded; the host has no system mix to apply it to\n", AudioOut2TraceSeconds(), param->debug_state_id);
    return 0;
}

int APS5_VABI sceAudioOut2EnableChat(void) {
    if (!g_chatEnabled.exchange(true)) AUDIOOUT2_TRACE("t=%.3f EnableChat: chat audio is allowed alongside the title's; the host mixes no chat audio\n", AudioOut2TraceSeconds());
    return 0;
}

int APS5_VABI sceAudioOut2DisableChat(void) {
    if (g_chatEnabled.exchange(false)) AUDIOOUT2_TRACE("t=%.3f DisableChat: chat audio is no longer allowed alongside the title's\n", AudioOut2TraceSeconds());
    return 0;
}

}
