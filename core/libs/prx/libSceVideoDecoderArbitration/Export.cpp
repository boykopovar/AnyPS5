#include "prx/libSceVideoDecoderArbitration/include/Api.hpp"
#include "prx/libSceVideoArbitration/libSceVideoArbitration.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81570001);
constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_STATE = static_cast<int>(0x81570002);
constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_OUT_OF_RANGE = static_cast<int>(0x81570004);
constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_INTERNAL = static_cast<int>(0x815700FF);

constexpr int SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED = static_cast<int>(0x819E0001);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT = static_cast<int>(0x819E0002);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x819E0003);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_OUT_OF_RANGE = static_cast<int>(0x819E0004);

constexpr std::uint64_t DecoderArbitrationParamSize = 0x18;
constexpr std::uint32_t DecoderArbitrationValueMin = 0x100;
constexpr std::uint32_t DecoderArbitrationValueMax = 0x2FF;
constexpr std::uint64_t DecoderArbitrationCountMax = 0x7F;
constexpr std::uint32_t DecoderArbitrationEventMax = 1;

struct VideoArbitrationInitParam {
    std::uint64_t size;
    std::uint32_t value;
    std::uint32_t reserved;
    std::uint64_t count;
};
static_assert(sizeof(VideoArbitrationInitParam) == 0x18);

std::mutex g_mutex;
bool g_initialized = false;
void* g_callback = nullptr;

using DecoderCallbackFn = void(APS5_VABI*)(std::uint32_t event);

void APS5_VABI DecoderArbitrationTrampoline(std::uint32_t event) {
    void* callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        callback = g_callback;
    }
    if (callback != nullptr) {
        reinterpret_cast<DecoderCallbackFn>(callback)(event);
    }
}

int MapArbitrationError(int rc) {
    if (rc == 0) return 0;
    if (rc == SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_STATE;
    if (rc == SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    if (rc == SCE_VIDEO_ARBITRATION_ERROR_ALREADY_INITIALIZED) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_STATE;
    if (rc == SCE_VIDEO_ARBITRATION_ERROR_OUT_OF_RANGE) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_OUT_OF_RANGE;
    return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INTERNAL;
}

}

extern "C" {

int APS5_VABI sceVideoDecoderArbitrationAcceptEvent(std::uint32_t event) {
    if (event > DecoderArbitrationEventMax) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    const int rc = sceVideoArbitrationAcceptEvent(event);
    if (rc == SCE_VIDEO_ARBITRATION_ERROR_ALREADY_INITIALIZED) return 0;
    return MapArbitrationError(rc);
}

int APS5_VABI sceVideoDecoderArbitrationEnable(std::uint64_t reserved, void* callback) {
    if (reserved != 0 || callback == nullptr) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_callback != nullptr) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_STATE;
    g_callback = callback;
    const int rc = sceVideoArbitrationEnable(0, reinterpret_cast<void*>(&DecoderArbitrationTrampoline));
    if (rc != 0) {
        g_callback = nullptr;
        return MapArbitrationError(rc);
    }
    return 0;
}

int APS5_VABI sceVideoDecoderArbitrationEnableSuspendMode() {
    return 0;
}

int APS5_VABI sceVideoDecoderArbitrationInitialize(const void* param) {
    if (param == nullptr) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    const auto* bytes = static_cast<const std::uint8_t*>(param);
    const int savedErrno = errno;
    std::uint64_t size = 0;
    const bool sizeOk = GuestCopyIn_nid_no_patch(&size, bytes, sizeof(size));
    errno = savedErrno;
    if (!sizeOk || size != DecoderArbitrationParamSize) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_STATE;
    std::uint32_t value = 0;
    const bool valueOk = GuestCopyIn_nid_no_patch(&value, bytes + 8, sizeof(value));
    errno = savedErrno;
    if (!valueOk) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    }
    if (value < DecoderArbitrationValueMin || value > DecoderArbitrationValueMax) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_OUT_OF_RANGE;
    }
    std::uint64_t count = 0;
    const bool countOk = GuestCopyIn_nid_no_patch(&count, bytes + 16, sizeof(count));
    errno = savedErrno;
    if (!countOk || count == 0 || count > DecoderArbitrationCountMax) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    }
    const VideoArbitrationInitParam localParam{DecoderArbitrationParamSize, value, 0, count};
    const int rc = sceVideoArbitrationInitialize(&localParam);
    if (rc != 0) {
        return MapArbitrationError(rc);
    }
    g_initialized = true;
    return 0;
}

}
