#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceCamera/include/InventoryExports.hpp"
#include "prx/libSceCamera/include/Camera.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstdint>

namespace {

constexpr int SCE_CAMERA_ERROR_INVALID_APP_CATEGORY = static_cast<int>(0x802E0011u);
constexpr std::int32_t SCE_USER_SERVICE_USER_ID_SYSTEM = 0xFF;

int CheckUsageInfo(void* info) {
    if (!info) return SCE_CAMERA_ERROR_PARAM;
    std::uint32_t sizeThis = 0;
    const int savedErrno = errno;
    const bool readOk = GuestCopyIn_nid_no_patch(&sizeThis, info, sizeof(sizeThis));
    errno = savedErrno;
    if (!readOk || sizeThis < 12u || sizeThis > 16u) return SCE_CAMERA_ERROR_PARAM;
    return 0;
}

int CheckOpenedOutput(void* info) {
    if (!info) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_INIT;
    return 0;
}

bool IsForceDriverModeCategoryAllowed(std::uint32_t category) {
    if (category < 0x10000u) return true;
    switch (category) {
    case 0x10300u:
    case 0x1F000u:
    case 0x1F300u:
    case 0x20000u:
    case 0x2F000u:
    case 0x200F100u:
        return true;
    default:
        return false;
    }
}

}

extern "C" {

int APS5_VABI sceCameraGetUsageInfo(std::int32_t, void* info) {
    if (const int error = CheckUsageInfo(info)) return error;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCameraGetUsageInfoWithoutCountProcess(std::int32_t, void* info) {
    if (const int error = CheckUsageInfo(info)) return error;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCameraOpenByModuleIdWithMode(std::int32_t userId, std::int32_t type, std::int32_t index, void*, std::uint32_t, std::uint32_t) {
    if (userId != SCE_USER_SERVICE_USER_ID_SYSTEM || type != 0 || index != 0) return SCE_CAMERA_ERROR_PARAM;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCameraSetForceDriverMode(std::int32_t mode) {
    std::uint32_t category = 0xFFFFFFFFu;
    if (!GetAppCategoryType_nid_no_patch(&category)) {
        category = 0xFFFFFFFFu;
    }
    if (!IsForceDriverModeCategoryAllowed(category)) return SCE_CAMERA_ERROR_INVALID_APP_CATEGORY;
    if (mode != 0 && mode != 1) return SCE_CAMERA_ERROR_PARAM;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceCameraIsVrCaptureEnable(std::int32_t* enabled) {
    if (!enabled) return SCE_CAMERA_ERROR_PARAM;
    const std::int32_t disabled = 0;
    const int savedErrno = errno;
    const bool writeOk = GuestCopyOut_nid_no_patch(enabled, &disabled, sizeof(disabled));
    errno = savedErrno;
    return writeOk ? 0 : SCE_CAMERA_ERROR_PARAM;
}

}

extern "C" {

int APS5_VABI libSceCameraUnknown_AnqFx_minus_nAxZM_nid_no_patch(void) {
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_INIT;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AnqFx-nAxZM", libSceCameraUnknown_AnqFx_minus_nAxZM_nid_no_patch);

int APS5_VABI libSceCameraUnknown_azeU8qML4VM_nid_no_patch(void* info) {
    if (const int error = CheckOpenedOutput(info)) return error;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("azeU8qML4VM", libSceCameraUnknown_azeU8qML4VM_nid_no_patch);

int APS5_VABI libSceCameraUnknown_kgRnfleNYNo_nid_no_patch(void* info) {
    if (const int error = CheckOpenedOutput(info)) return error;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("kgRnfleNYNo", libSceCameraUnknown_kgRnfleNYNo_nid_no_patch);

}
