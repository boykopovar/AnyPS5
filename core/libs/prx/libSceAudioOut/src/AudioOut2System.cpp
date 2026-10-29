#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "AudioOut2Internal.hpp"

static constexpr std::uint32_t MAX_SUPPORTED_3D_OUTPUT = 2;
static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_PARAM = static_cast<int>(0x80268001);

static std::atomic<bool> g_chatEnabled{false};

extern "C" {

int APS5_VABI sceAudioOut2InitializeEx(std::uint32_t param) {
    (void)param;
    AudioOut2MarkInitialized_nid_no_patch();
    return 0;
}

int APS5_VABI sceAudioOut2Set3DLatency(int userId, std::uint32_t output) {
    if (output > MAX_SUPPORTED_3D_OUTPUT) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    if (userId != 0 && (userId != 0xFF || output == 0)) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    return 0;
}

int APS5_VABI sceAudioOut2GetSystemState(AudioOut2SystemState* state) {
    if (!state) return static_cast<int>(0x80260502);
    state->loudness = 0.0f;
    return 0;
}

int APS5_VABI sceAudioOut2SetSystemDebugState(const AudioOut2SystemDebugStateParam* param) {
    constexpr int ErrorInvalidPointer = static_cast<int>(0x8026800C);
    constexpr std::uint32_t DebugStateIdLoudness = 1;
    constexpr std::uint32_t DebugStateIdUnmodelled = 0x20;
    constexpr std::size_t LoudnessParamSize = 8;
    constexpr std::size_t UnmodelledParamSize = 4;
    if (param == nullptr) return ErrorInvalidPointer;
    if (param->debug_state_id == DebugStateIdUnmodelled && param->param_size == UnmodelledParamSize) NotImplemented_nid_no_patch(__func__);
    if (param->debug_state_id != DebugStateIdLoudness || param->param_size != LoudnessParamSize) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    if (param->param == nullptr) NotImplemented_nid_no_patch(__func__);
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
