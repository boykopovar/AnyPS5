#include "prx/libSceVideoArbitration/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED = static_cast<int>(0x819E0001);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT = static_cast<int>(0x819E0002);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x819E0003);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_NO_EVENT = static_cast<int>(0x819E0003);
constexpr std::uint64_t VideoArbitrationParamSize = 0x18;

struct VideoArbitrationParam {
    std::uint64_t size;
    std::uint32_t priority;
    std::uint32_t reserved;
    std::uint64_t affinityMask;
};
static_assert(sizeof(VideoArbitrationParam) == VideoArbitrationParamSize);

bool CopyInPreservingErrno(void* destination, const void* source, std::size_t size) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = savedErrno;
    return copied;
}

std::mutex g_mutex;
bool g_initialized = false;
bool g_hasPendingEvent = false;
std::uint32_t g_pendingEvent = 0;
void* g_callback = nullptr;

} // namespace

extern "C" {

int APS5_VABI sceVideoArbitrationAcceptEvent(std::uint32_t event) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED;
    if (!g_hasPendingEvent || g_pendingEvent != event) {
        return SCE_VIDEO_ARBITRATION_ERROR_NO_EVENT;
    }
    g_hasPendingEvent = false;
    return 0;
}

int APS5_VABI sceVideoArbitrationEnable(std::uint64_t reserved, void* callback) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) return SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED;
    if (reserved != 0 || callback == nullptr) {
        return SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT;
    }
    g_callback = callback;
    return 0;
}

int APS5_VABI sceVideoArbitrationInitialize(const void* param) {
    if (param == nullptr) return SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT;
    VideoArbitrationParam localParam{};
    if (!CopyInPreservingErrno(&localParam, param, sizeof(localParam))) {
        return SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT;
    }
    if (localParam.size != VideoArbitrationParamSize) {
        return SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized) return SCE_VIDEO_ARBITRATION_ERROR_ALREADY_INITIALIZED;
    g_initialized = true;
    g_hasPendingEvent = false;
    g_pendingEvent = 0;
    g_callback = nullptr;
    return 0;
}

}
