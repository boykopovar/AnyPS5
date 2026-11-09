#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceShare/include/InventoryExports.hpp"
#include "prx/libSceShare/include/Api.hpp"
#include <cstdint>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Capture and sharing are not emulated; policy and parameter calls are accepted.

static_assert(sizeof(ShareScreenshotParamExtended) == 24);
static_assert(offsetof(ShareScreenshotParamExtended, text) == 16);
static_assert(sizeof(ShareVideoClipParamExtended) == 40);
static_assert(offsetof(ShareVideoClipParamExtended, mode) == 24);
static_assert(offsetof(ShareVideoClipParamExtended, text) == 32);

namespace {

constexpr std::int32_t ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x81960002);
constexpr std::int32_t ERROR_NOT_SUPPORTED = static_cast<std::int32_t>(0x81960007);
constexpr std::int32_t REQUEST_ID_INVALID = -1;

bool WriteRequestIdInvalid(std::int32_t* requestId) {
    const std::int32_t invalid = REQUEST_ID_INVALID;
    const auto savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(requestId, &invalid, sizeof(invalid));
    errno = savedErrno;
    return copied;
}

}

extern "C" {

int APS5_VABI sceShareCaptureScreenshot(const void* param, int32_t* req_id) {
    (void)param;
    if (req_id != nullptr && !WriteRequestIdInvalid(req_id)) return ERROR_INVALID_PARAM;
    return ERROR_NOT_SUPPORTED;
}

int APS5_VABI sceShareCaptureVideoClip(const void* param, int32_t* req_id) {
    (void)param;
    if (req_id != nullptr && !WriteRequestIdInvalid(req_id)) return ERROR_INVALID_PARAM;
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
    const ShareCurrentStatus zeroStatus{};
    const auto savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(status, &zeroStatus, sizeof(zeroStatus))) {
        errno = savedErrno;
        return ERROR_INVALID_PARAM;
    }
    errno = savedErrno;
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
    ShareScreenshotParamExtended copy{};
    if (param == nullptr || !GuestCopyIn_nid_no_patch(&copy, param, sizeof(copy))) return ERROR_INVALID_PARAM;
    if (copy.size != sizeof(copy)) return ERROR_INVALID_PARAM;
    if (requestId != nullptr && !WriteRequestIdInvalid(requestId)) return ERROR_INVALID_PARAM;
    return ERROR_NOT_SUPPORTED;
}

int APS5_VABI sceShareCaptureVideoClipExtended(const ShareVideoClipParamExtended* param, std::int32_t* requestId) {
    ShareVideoClipParamExtended copy{};
    if (param == nullptr || !GuestCopyIn_nid_no_patch(&copy, param, sizeof(copy))) return ERROR_INVALID_PARAM;
    if (copy.size != sizeof(copy)) return ERROR_INVALID_PARAM;
    if (requestId != nullptr && !WriteRequestIdInvalid(requestId)) return ERROR_INVALID_PARAM;
    if (copy.reserved != 0 || (copy.mode == 0 && copy.value16 != 0)) return ERROR_INVALID_PARAM;
    return ERROR_NOT_SUPPORTED;
}

int APS5_VABI sceShareGetRunningStatus(uint32_t* status) {
    if (status == nullptr) {
        return ERROR_INVALID_PARAM;
    }
    const std::uint32_t zeroStatus = 0;
    const auto savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(status, &zeroStatus, sizeof(zeroStatus))) {
        errno = savedErrno;
        return ERROR_INVALID_PARAM;
    }
    errno = savedErrno;
    return 0;
}

int APS5_VABI sceShareSetContentParamForApplicationTitle(const char* applicationTitle) {
    if (applicationTitle == nullptr) return ERROR_INVALID_PARAM;
    std::string title;
    if (CopyGuestString_nid_no_patch(applicationTitle, 5120, title) != 0 || title.empty()) return ERROR_INVALID_PARAM;
    return 0;
}

}


extern "C" {

int APS5_VABI sceShareFeatureExPermit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareFeatureExProhibit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareRegisterUploadCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceShareUnregisterUploadCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceShareUnknown_YPIiGz4uKOU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YPIiGz4uKOU", libSceShareUnknown_YPIiGz4uKOU_nid_no_patch);

int APS5_VABI libSceShareUnknown_txixe0fiPKM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("txixe0fiPKM", libSceShareUnknown_txixe0fiPKM_nid_no_patch);

}
