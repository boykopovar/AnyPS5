#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Capture and sharing are not emulated; policy and parameter calls are accepted.

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

}
