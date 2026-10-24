#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Capture and sharing are not emulated; policy and parameter calls are accepted.

struct ShareScreenshotParamExtended {
    std::uint64_t size;
    std::uint32_t source;
    std::uint32_t mode;
    const char* text;
};

struct ShareVideoClipParamExtended {
    std::uint64_t size;
    std::uint64_t value8;
    std::uint64_t value16;
    std::uint32_t mode;
    std::uint32_t reserved;
    const char* text;
};

static_assert(sizeof(ShareScreenshotParamExtended) == 24);
static_assert(offsetof(ShareScreenshotParamExtended, text) == 16);
static_assert(sizeof(ShareVideoClipParamExtended) == 40);
static_assert(offsetof(ShareVideoClipParamExtended, mode) == 24);
static_assert(offsetof(ShareVideoClipParamExtended, text) == 32);

namespace {

constexpr std::int32_t ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x81960002);
constexpr std::int32_t ERROR_NOT_SUPPORTED = static_cast<std::int32_t>(0x81960007);
constexpr std::int32_t REQUEST_ID_INVALID = -1;

}

extern "C" {

int APS5_VABI sceShareCaptureScreenshot(const void* param, int32_t* req_id) {
    (void)param;
    if (req_id != nullptr) {
        *req_id = REQUEST_ID_INVALID;
    }
    return ERROR_NOT_SUPPORTED;
}

int APS5_VABI sceShareCaptureVideoClip(const void* param, int32_t* req_id) {
    (void)param;
    if (req_id != nullptr) {
        *req_id = REQUEST_ID_INVALID;
    }
    return ERROR_NOT_SUPPORTED;
}

int APS5_VABI sceShareFeaturePermit(uint32_t feature_flags) {
    (void)feature_flags;
    return 0;
}

int APS5_VABI sceShareFeatureProhibit(uint32_t feature_flags) {
    (void)feature_flags;
    return 0;
}

int APS5_VABI sceShareGetCurrentStatus(uint32_t feature_flag, ShareCurrentStatus* status) {
    if (feature_flag == 0 || status == nullptr) {
        return ERROR_INVALID_PARAM;
    }
    std::memset(status, 0, sizeof(*status));
    return 0;
}

int APS5_VABI sceShareInitialize(size_t heap_size, int thread_priority, uint64_t affinity_mask) {
    (void)heap_size;
    (void)thread_priority;
    (void)affinity_mask;
    return 0;
}

int APS5_VABI sceShareOpenMenuForContent(const void* content_id) {
    (void)content_id;
    return ERROR_NOT_SUPPORTED;
}

int APS5_VABI sceShareRegisterContentEventCallback(void* callback, void* user_data) {
    (void)callback;
    (void)user_data;
    return 0;
}

int APS5_VABI sceShareSetCaptureSource(uint32_t feature_flags, const void* tap_point) {
    (void)feature_flags;
    (void)tap_point;
    return 0;
}

int APS5_VABI sceShareSetContentParam(const char* content_param) {
    (void)content_param;
    return 0;
}

int APS5_VABI sceShareSetScreenshotOverlayImage(const char* file_path, int32_t margin_x, int32_t margin_y, int32_t origin) {
    (void)file_path;
    (void)margin_x;
    (void)margin_y;
    (void)origin;
    return 0;
}

int APS5_VABI sceShareTerminate(void) {
    return 0;
}

int APS5_VABI sceShareUnregisterContentEventCallback(void* callback) {
    (void)callback;
    return 0;
}

int APS5_VABI sceShareCaptureScreenshotExtended(const ShareScreenshotParamExtended* param, std::int32_t* requestId) {
    if (param == nullptr || param->size != sizeof(*param)) return ERROR_INVALID_PARAM;
    if (requestId != nullptr) *requestId = REQUEST_ID_INVALID;
    return ERROR_NOT_SUPPORTED;
}

int APS5_VABI sceShareCaptureVideoClipExtended(const ShareVideoClipParamExtended* param, std::int32_t* requestId) {
    if (param == nullptr || param->size != sizeof(*param)) return ERROR_INVALID_PARAM;
    if (requestId != nullptr) *requestId = REQUEST_ID_INVALID;
    if (param->reserved != 0 || (param->mode == 0 && param->value16 != 0)) return ERROR_INVALID_PARAM;
    return ERROR_NOT_SUPPORTED;
}

int APS5_VABI sceShareGetRunningStatus(uint32_t* status) {
    if (status == nullptr) {
        return ERROR_INVALID_PARAM;
    }
    *status = 0;
    return 0;
}

int APS5_VABI sceShareSetContentParamForApplicationTitle(const char* applicationTitle) {
    if (applicationTitle == nullptr) return ERROR_INVALID_PARAM;
    std::size_t length = 0;
    while (length < 5120 && applicationTitle[length] != 0) ++length;
    if (length == 0 || length == 5120) return ERROR_INVALID_PARAM;
    return 0;
}

}
