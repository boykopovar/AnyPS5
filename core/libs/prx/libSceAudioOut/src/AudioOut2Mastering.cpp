#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <array>
#include <cerrno>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "AudioOut2Internal.hpp"

namespace {

constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_API_PARAM = static_cast<int>(0x80268201);
constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_STATES_ID = static_cast<int>(0x80268204);
constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_NOT_INITIALIZED = static_cast<int>(0x80268203);
constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_FLAG = static_cast<int>(0x80268205);
constexpr int SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED = static_cast<int>(0x80268006);
constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_POINTER = static_cast<int>(0x8026800C);
constexpr std::uint32_t AUDIO_OUT2_MASTERING_INIT_EX_MODE = 0x80000000u;
constexpr std::uint32_t AUDIO_OUT2_MASTERING_SET_PARAM_FLAGS_MAX = 1;

constexpr int SCE_AUDIO_OUT_ERROR_MASTERING_INVALID_FLAGS = static_cast<int>(0x80260201);
constexpr int SCE_AUDIO_OUT_ERROR_MASTERING_NOT_INITIALIZED = static_cast<int>(0x80260203);
constexpr int SCE_AUDIO_OUT_ERROR_MASTERING_INVALID_STATES_ID = static_cast<int>(0x80260204);

std::atomic<bool> g_masteringInitialized{false};
std::atomic<bool> g_legacyMasteringInitialized{false};

int InitializeMastering(uint32_t flags, uint32_t accepted) {
    if (!AudioOut2IsInitialized_nid_no_patch()) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
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

bool ReadMasteringId(const void* source, std::uint32_t& id) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&id, source, sizeof(id));
    errno = savedErrno;
    return copied;
}

bool ClearMasteringMeter(void* state, std::size_t offset, std::size_t bytes) {
    constexpr std::array<std::byte, 96> zeros{};
    const auto address = reinterpret_cast<std::uintptr_t>(state);
    if (bytes > zeros.size() || address > UINTPTR_MAX - offset) return false;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(address + offset), zeros.data(), bytes);
    errno = savedErrno;
    return copied;
}

static_assert(sizeof(AudioOut2MasteringStates) == 336);
static_assert(offsetof(AudioOut2MasteringStates, compressor_states) + offsetof(AudioOut2MasteringCompressorStates, input_rms) == AUDIO_OUT_MASTERING_COMPRESSOR_INPUT_RMS_OFFSET);
static_assert(offsetof(AudioOut2MasteringStates, compressor_states) + offsetof(AudioOut2MasteringCompressorStates, compression_coeff) == AUDIO_OUT_MASTERING_COMPRESSOR_GAIN_OFFSET);
static_assert(offsetof(AudioOut2MasteringStates, limiter_states) + offsetof(AudioOut2MasteringLimiterStates, input_peak) == AUDIO_OUT_MASTERING_LIMITER_INPUT_PEAK_OFFSET);
static_assert(offsetof(AudioOut2MasteringStates, limiter_states) + offsetof(AudioOut2MasteringLimiterStates, output_peak) == AUDIO_OUT_MASTERING_LIMITER_OUTPUT_PEAK_OFFSET);
static_assert(offsetof(AudioOut2MasteringStates, limiter_states) + offsetof(AudioOut2MasteringLimiterStates, gain_peak) == AUDIO_OUT_MASTERING_LIMITER_GAIN_PEAK_OFFSET);

}

extern "C" {

int APS5_VABI sceAudioOutMasteringInit(uint32_t flags) {
    if (flags == 1) return SCE_AUDIO_OUT_ERROR_MASTERING_INVALID_FLAGS;
    if (flags != 0) NotImplemented_nid_no_patch(__func__);
    g_legacyMasteringInitialized.store(true);
    return 0;
}

int APS5_VABI sceAudioOutMasteringTerm(void) {
    return g_legacyMasteringInitialized.exchange(false) ? 0 : SCE_AUDIO_OUT_ERROR_MASTERING_NOT_INITIALIZED;
}

int APS5_VABI sceAudioOutMasteringGetState(AudioOut2MasteringStatesHeader* state) {
    if (state == nullptr) APS5_INVALID_ARG_EX;
    std::uint32_t id = 0;
    if (!ReadMasteringId(state, id)) APS5_INVALID_ARG_EX;
    if (id != AUDIO_OUT_MASTERING_STATES_ID_DEFAULT) return SCE_AUDIO_OUT_ERROR_MASTERING_INVALID_STATES_ID;
    if (!g_legacyMasteringInitialized.load()) return SCE_AUDIO_OUT_ERROR_MASTERING_NOT_INITIALIZED;
    if (!ClearMasteringMeter(state, AUDIO_OUT_MASTERING_COMPRESSOR_INPUT_RMS_OFFSET, 96) ||
        !ClearMasteringMeter(state, AUDIO_OUT_MASTERING_COMPRESSOR_GAIN_OFFSET, 96) ||
        !ClearMasteringMeter(state, AUDIO_OUT_MASTERING_LIMITER_INPUT_PEAK_OFFSET, 32) ||
        !ClearMasteringMeter(state, AUDIO_OUT_MASTERING_LIMITER_OUTPUT_PEAK_OFFSET, 32) ||
        !ClearMasteringMeter(state, AUDIO_OUT_MASTERING_LIMITER_GAIN_PEAK_OFFSET, 32)) APS5_INVALID_ARG_EX;
    return 0;
}

int APS5_VABI sceAudioOutMasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t flags) {
    if (param == nullptr) APS5_INVALID_ARG_EX;
    if (flags != 0) NotImplemented_nid_no_patch(__func__);
    std::uint32_t id = 0;
    if (!ReadMasteringId(param, id)) APS5_INVALID_ARG_EX;
    if (id != AUDIO_OUT_MASTERING_PARAMS_ID_DEFAULT) NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2MasteringInit(uint32_t flags) {
    return InitializeMastering(flags, 0);
}

int APS5_VABI sceAudioOut2MasteringInitEx(uint32_t flags) {
    return InitializeMastering(flags, AUDIO_OUT2_MASTERING_INIT_EX_MODE);
}

int APS5_VABI sceAudioOut2MasteringTerm(void) {
    if (!AudioOut2IsInitialized_nid_no_patch()) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
    if (!g_masteringInitialized.exchange(false)) return SCE_AUDIO_OUT2_ERROR_MASTERING_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceAudioOut2MasteringGetState(AudioOut2MasteringStatesHeader* state, uint32_t output, AudioOut2UserHandle user) {
    (void)output;
    (void)user;
    if (!AudioOut2IsInitialized_nid_no_patch()) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
    if (!g_masteringInitialized.load()) return SCE_AUDIO_OUT2_ERROR_MASTERING_NOT_INITIALIZED;
    if (state == nullptr) return SCE_AUDIO_OUT2_ERROR_INVALID_POINTER;
    std::uint32_t id = 0;
    if (!ReadMasteringId(state, id)) return SCE_AUDIO_OUT2_ERROR_INVALID_POINTER;
    const int savedErrno = errno;
    bool copied = false;
    switch (id) {
        case AUDIO_OUT2_MASTERING_STATES_ID_DEFAULT: {
            AudioOut2MasteringStates full{};
            full.states_header.states_id = AUDIO_OUT2_MASTERING_STATES_ID_DEFAULT;
            full.compressor_states.descriptor.id = AUDIO_OUT2_MASTERING_STRUCT_ID_COMPRESSOR_DEFAULT;
            full.compressor_states.descriptor.size = sizeof(AudioOut2MasteringCompressorStates);
            full.limiter_states.descriptor.id = AUDIO_OUT2_MASTERING_STRUCT_ID_LIMITER_DEFAULT;
            full.limiter_states.descriptor.size = sizeof(AudioOut2MasteringLimiterStates);
            copied = GuestCopyOut_nid_no_patch(state, &full, sizeof(full));
            break;
        }
        case AUDIO_OUT2_MASTERING_STATES_ID_V2: {
            AudioOut2MasteringStatesV2 full{};
            full.states_header.states_id = AUDIO_OUT2_MASTERING_STATES_ID_V2;
            full.compressor_states.descriptor.id = AUDIO_OUT2_MASTERING_STRUCT_ID_COMPRESSOR_V2;
            full.compressor_states.descriptor.size = sizeof(AudioOut2MasteringCompressorStatesV2);
            full.limiter_states.descriptor.id = AUDIO_OUT2_MASTERING_STRUCT_ID_LIMITER_DEFAULT;
            full.limiter_states.descriptor.size = sizeof(AudioOut2MasteringLimiterStates);
            copied = GuestCopyOut_nid_no_patch(state, &full, sizeof(full));
            break;
        }
        default:
            errno = savedErrno;
            return SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_STATES_ID;
    }
    errno = savedErrno;
    return copied ? 0 : SCE_AUDIO_OUT2_ERROR_INVALID_POINTER;
}

int APS5_VABI sceAudioOut2MasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t output, uint32_t flags) {
    (void)output;
    if (!AudioOut2IsInitialized_nid_no_patch()) return SCE_AUDIO_OUT2_ERROR_NOT_INITIALIZED;
    if (!g_masteringInitialized.load()) return SCE_AUDIO_OUT2_ERROR_MASTERING_NOT_INITIALIZED;
    if (param == nullptr) return SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_API_PARAM;
    if (flags > AUDIO_OUT2_MASTERING_SET_PARAM_FLAGS_MAX) return SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_FLAG;
    return 0;
}

}
