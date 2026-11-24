#include "prx/libSceVideoDecoderArbitration/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoArbitration/include/Api.hpp"

namespace {

constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81570001);
constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x81570002);
constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_NOT_INITIALIZED = static_cast<int>(0x81570003);
constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_OUT_OF_RANGE = static_cast<int>(0x81570004);

constexpr int SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED = static_cast<int>(0x819E0001);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT = static_cast<int>(0x819E0002);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x819E0003);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_NO_EVENT = static_cast<int>(0x819E0003);

constexpr std::uint64_t DecoderArbitrationParamSize = 0x18;
constexpr std::size_t DecoderArbitrationValueOffset = 0x8;
constexpr std::size_t DecoderArbitrationCountOffset = 0x10;
constexpr std::uint32_t DecoderArbitrationValueMin = 0x100;
constexpr std::uint32_t DecoderArbitrationValueMax = 0x2FF;
constexpr std::uint64_t DecoderArbitrationCountMax = 0x7F;
constexpr std::uint32_t DecoderArbitrationEventMax = 1;

std::mutex g_mutex;
bool g_initialized = false;
void* g_callback = nullptr;

} // namespace

extern "C" {

int APS5_VABI sceVideoDecoderArbitrationAcceptEvent(std::uint32_t event) {
    if (event > DecoderArbitrationEventMax) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    const int result = sceVideoArbitrationAcceptEvent(event);
    if (result == 0) return 0;
    if (result == SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED || result == SCE_VIDEO_ARBITRATION_ERROR_NO_EVENT) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_NOT_INITIALIZED;
    }
    if (result == SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    }
    return SCE_VIDEO_DECODER_ARBITRATION_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoDecoderArbitrationEnable(std::uint64_t reserved, void* callback) {
    if (reserved != 0 || callback == nullptr) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_callback != nullptr) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_ALREADY_INITIALIZED;
    g_callback = callback;
    const int result = sceVideoArbitrationEnable(reserved, callback);
    if (result == 0) return 0;
    g_callback = nullptr;
    if (result == SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_NOT_INITIALIZED;
    }
    if (result == SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    }
    return SCE_VIDEO_DECODER_ARBITRATION_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoDecoderArbitrationEnableSuspendMode() {
    return 0;
}

int APS5_VABI sceVideoDecoderArbitrationInitialize(const void* param) {
    if (param == nullptr) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    const auto* bytes = static_cast<const std::uint8_t*>(param);
    std::uint64_t size = 0;
    if (!GuestCopyIn_nid_no_patch(&size, bytes, sizeof(size))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    if (size != DecoderArbitrationParamSize) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_ALREADY_INITIALIZED;
    std::uint32_t value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, bytes + DecoderArbitrationValueOffset, sizeof(value))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    if (value < DecoderArbitrationValueMin || value > DecoderArbitrationValueMax) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_OUT_OF_RANGE;
    }
    std::uint64_t count = 0;
    if (!GuestCopyIn_nid_no_patch(&count, bytes + DecoderArbitrationCountOffset, sizeof(count))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    if (count == 0 || count > DecoderArbitrationCountMax) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    const int result = sceVideoArbitrationInitialize(param);
    if (result == 0) {
        g_initialized = true;
        return 0;
    }
    if (result == SCE_VIDEO_ARBITRATION_ERROR_ALREADY_INITIALIZED) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_ALREADY_INITIALIZED;
    }
    if (result == SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    }
    return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
}

}
