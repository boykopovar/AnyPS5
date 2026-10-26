#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <atomic>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_API_PARAM = static_cast<int>(0x80268201);
constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_STATES_ID = static_cast<int>(0x80268204);
constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_NOT_INITIALIZED = static_cast<int>(0x80268203);
constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_FLAG = static_cast<int>(0x80268205);
constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_POINTER = static_cast<int>(0x8026800C);
constexpr std::uint32_t AUDIO_OUT2_MASTERING_INIT_EX_MODE = 0x80000000u;
constexpr std::uint32_t AUDIO_OUT2_MASTERING_SET_PARAM_FLAGS_MAX = 1;

std::atomic<bool> g_masteringInitialized{false};

int InitializeMastering(uint32_t flags, uint32_t accepted) {
    if (g_masteringInitialized.load()) return 0;
    if ((flags & ~accepted) != 0) return SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_API_PARAM;
    g_masteringInitialized.store(true);
    return 0;
}

constexpr std::uint32_t AUDIO_OUT2_MASTERING_STATES_ID_DEFAULT = 1;
constexpr std::uint32_t AUDIO_OUT2_MASTERING_STATES_ID_V2 = 2;
constexpr std::uint32_t AUDIO_OUT2_MASTERING_STRUCT_ID_COMPRESSOR_DEFAULT = 0x01010001;
constexpr std::uint32_t AUDIO_OUT2_MASTERING_STRUCT_ID_COMPRESSOR_V2 = 0x01020001;
constexpr std::uint32_t AUDIO_OUT2_MASTERING_STRUCT_ID_LIMITER_DEFAULT = 0x01010003;

constexpr std::uint32_t AUDIO_OUT_MASTERING_PARAMS_ID_DEFAULT = 1;
constexpr std::uint32_t AUDIO_OUT_MASTERING_STATES_ID_DEFAULT = 1;

constexpr std::size_t AUDIO_OUT_MASTERING_COMPRESSOR_INPUT_RMS_OFFSET = 32;
constexpr std::size_t AUDIO_OUT_MASTERING_COMPRESSOR_GAIN_OFFSET = 128;
constexpr std::size_t AUDIO_OUT_MASTERING_LIMITER_INPUT_PEAK_OFFSET = 240;
constexpr std::size_t AUDIO_OUT_MASTERING_LIMITER_OUTPUT_PEAK_OFFSET = 272;
constexpr std::size_t AUDIO_OUT_MASTERING_LIMITER_GAIN_PEAK_OFFSET = 304;

static_assert(sizeof(AudioOut2MasteringStates) == 336);
static_assert(offsetof(AudioOut2MasteringStates, compressor_states) + offsetof(AudioOut2MasteringCompressorStates, input_rms) == AUDIO_OUT_MASTERING_COMPRESSOR_INPUT_RMS_OFFSET);
static_assert(offsetof(AudioOut2MasteringStates, compressor_states) + offsetof(AudioOut2MasteringCompressorStates, compression_coeff) == AUDIO_OUT_MASTERING_COMPRESSOR_GAIN_OFFSET);
static_assert(offsetof(AudioOut2MasteringStates, limiter_states) + offsetof(AudioOut2MasteringLimiterStates, input_peak) == AUDIO_OUT_MASTERING_LIMITER_INPUT_PEAK_OFFSET);
static_assert(offsetof(AudioOut2MasteringStates, limiter_states) + offsetof(AudioOut2MasteringLimiterStates, output_peak) == AUDIO_OUT_MASTERING_LIMITER_OUTPUT_PEAK_OFFSET);
static_assert(offsetof(AudioOut2MasteringStates, limiter_states) + offsetof(AudioOut2MasteringLimiterStates, gain_peak) == AUDIO_OUT_MASTERING_LIMITER_GAIN_PEAK_OFFSET);

}

extern "C" {

int APS5_VABI sceAudioOutMasteringInit(uint32_t flags) {
    if (flags != 0) NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOutMasteringTerm(void) {
    return 0;
}

int APS5_VABI sceAudioOutMasteringGetState(AudioOut2MasteringStatesHeader* state) {
    if (state == nullptr) APS5_INVALID_ARG_EX;
    if (state->states_id != AUDIO_OUT_MASTERING_STATES_ID_DEFAULT) NotImplemented_nid_no_patch(__func__);
    auto* full = static_cast<AudioOut2MasteringStates*>(static_cast<void*>(state));
    std::memset(full->compressor_states.input_rms, 0, sizeof(full->compressor_states.input_rms));
    std::memset(full->compressor_states.compression_coeff, 0, sizeof(full->compressor_states.compression_coeff));
    std::memset(full->limiter_states.input_peak, 0, sizeof(full->limiter_states.input_peak));
    std::memset(full->limiter_states.output_peak, 0, sizeof(full->limiter_states.output_peak));
    std::memset(full->limiter_states.gain_peak, 0, sizeof(full->limiter_states.gain_peak));
    return 0;
}

int APS5_VABI sceAudioOutMasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t flags) {
    if (param == nullptr) APS5_INVALID_ARG_EX;
    if (flags != 0) NotImplemented_nid_no_patch(__func__);
    if (param->params_id != AUDIO_OUT_MASTERING_PARAMS_ID_DEFAULT) NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2MasteringInit(uint32_t flags) {
    return InitializeMastering(flags, 0);
}

int APS5_VABI sceAudioOut2MasteringInitEx(uint32_t flags) {
    return InitializeMastering(flags, AUDIO_OUT2_MASTERING_INIT_EX_MODE);
}

int APS5_VABI sceAudioOut2MasteringTerm(void) {
    if (!g_masteringInitialized.exchange(false)) return SCE_AUDIO_OUT2_ERROR_MASTERING_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceAudioOut2MasteringGetState(AudioOut2MasteringStatesHeader* state, uint32_t output, AudioOut2UserHandle user) {
    (void)output;
    (void)user;
    if (!g_masteringInitialized.load()) return SCE_AUDIO_OUT2_ERROR_MASTERING_NOT_INITIALIZED;
    if (state == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_POINTER;
    switch (state->states_id) {
        case AUDIO_OUT2_MASTERING_STATES_ID_DEFAULT: {
            auto* full = static_cast<AudioOut2MasteringStates*>(static_cast<void*>(state));
            std::memset(full, 0, sizeof(*full));
            full->states_header.states_id = AUDIO_OUT2_MASTERING_STATES_ID_DEFAULT;
            full->compressor_states.descriptor.id = AUDIO_OUT2_MASTERING_STRUCT_ID_COMPRESSOR_DEFAULT;
            full->compressor_states.descriptor.size = sizeof(AudioOut2MasteringCompressorStates);
            full->limiter_states.descriptor.id = AUDIO_OUT2_MASTERING_STRUCT_ID_LIMITER_DEFAULT;
            full->limiter_states.descriptor.size = sizeof(AudioOut2MasteringLimiterStates);
            return 0;
        }
        case AUDIO_OUT2_MASTERING_STATES_ID_V2: {
            auto* full = static_cast<AudioOut2MasteringStatesV2*>(static_cast<void*>(state));
            std::memset(full, 0, sizeof(*full));
            full->states_header.states_id = AUDIO_OUT2_MASTERING_STATES_ID_V2;
            full->compressor_states.descriptor.id = AUDIO_OUT2_MASTERING_STRUCT_ID_COMPRESSOR_V2;
            full->compressor_states.descriptor.size = sizeof(AudioOut2MasteringCompressorStatesV2);
            full->limiter_states.descriptor.id = AUDIO_OUT2_MASTERING_STRUCT_ID_LIMITER_DEFAULT;
            full->limiter_states.descriptor.size = sizeof(AudioOut2MasteringLimiterStates);
            return 0;
        }
        default:
            return SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_STATES_ID;
    }
}

int APS5_VABI sceAudioOut2MasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t output, uint32_t flags) {
    (void)output;
    if (!g_masteringInitialized.load()) return SCE_AUDIO_OUT2_ERROR_MASTERING_NOT_INITIALIZED;
    if (param == nullptr) return SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_API_PARAM;
    if (flags > AUDIO_OUT2_MASTERING_SET_PARAM_FLAGS_MAX) return SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_FLAG;
    return 0;
}

}
