#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "AudioOut2Internal.hpp"

static constexpr std::uint32_t MAX_SUPPORTED_3D_OUTPUT = 2;
static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_PARAM = static_cast<int>(0x80268001);
static constexpr int SCE_AUDIO_OUT2_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80268004);
static constexpr int SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED = static_cast<int>(0x80268006);

static std::atomic<bool> g_chatEnabled{false};

extern "C" {

int APS5_VABI sceAudioOut2InitializeEx(std::uint32_t param) {
    (void)param;
    return AudioOut2TryInitialize_nid_no_patch() ? 0 : SCE_AUDIO_OUT2_ERROR_ALREADY_INITIALIZED;
}

int APS5_VABI sceAudioOut2Set3DLatency(int userId, std::uint32_t output) {
    if (output > MAX_SUPPORTED_3D_OUTPUT) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    if (userId != 0 && (userId != 0xFF || output == 0)) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    return 0;
}

int APS5_VABI sceAudioOut2GetSystemState(AudioOut2SystemState* state) {
    if (!state) return static_cast<int>(0x80260502);
    const float loudness = 0.0f;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(state, &loudness, sizeof(loudness));
    errno = savedErrno;
    return copied ? 0 : static_cast<int>(0x80260502);
}

int APS5_VABI sceAudioOut2GetSystemStateEx(void* state) {
    (void)state;
    if (!AudioOut2IsInitialized_nid_no_patch()) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2Terminate(void) {
    AudioOut2DestroyAllContexts();
    return 0;
}

int APS5_VABI sceAudioOut2SetSystemDebugState(const AudioOut2SystemDebugStateParam* param) {
    constexpr int ErrorInvalidPointer = static_cast<int>(0x8026800C);
    constexpr int ErrorDebugStateTwo = static_cast<int>(0x80268000);
    constexpr std::uint32_t DebugStateIdTwo = 2;
    constexpr std::uint32_t DebugStateIdLoudness = 1;
    constexpr std::uint32_t DebugStateIdUnmodelled = 0x20;
    constexpr std::size_t LoudnessParamSize = 8;
    constexpr std::size_t UnmodelledParamSize = 4;
    if (!AudioOut2IsInitialized_nid_no_patch()) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
    if (param == nullptr) return ErrorInvalidPointer;
    if (param->debug_state_id == DebugStateIdUnmodelled && param->param_size == UnmodelledParamSize) NotImplemented_nid_no_patch(__func__);
    if ((param->debug_state_id == DebugStateIdLoudness || param->debug_state_id == DebugStateIdTwo) && param->param == nullptr) return ErrorInvalidPointer;
    if (param->debug_state_id == DebugStateIdTwo && param->param_size == LoudnessParamSize) return ErrorDebugStateTwo;
    if (param->debug_state_id != DebugStateIdLoudness || param->param_size != LoudnessParamSize) return SCE_AUDIO_OUT2_ERROR_INVALID_PARAM;
    AUDIOOUT2_TRACE("t=%.3f SetSystemDebugState: debug state %u recorded; the host has no system mix to apply it to\n", AudioOut2TraceSeconds(), param->debug_state_id);
    return 0;
}

int APS5_VABI sceAudioOut2EnableChat(void) {
    if (!AudioOut2IsInitialized_nid_no_patch()) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
    if (!g_chatEnabled.exchange(true)) AUDIOOUT2_TRACE("t=%.3f EnableChat: chat audio is allowed alongside the title's; the host mixes no chat audio\n", AudioOut2TraceSeconds());
    return 0;
}

int APS5_VABI sceAudioOut2DisableChat(void) {
    if (!AudioOut2IsInitialized_nid_no_patch()) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
    if (g_chatEnabled.exchange(false)) AUDIOOUT2_TRACE("t=%.3f DisableChat: chat audio is no longer allowed alongside the title's\n", AudioOut2TraceSeconds());
    return 0;
}

}
