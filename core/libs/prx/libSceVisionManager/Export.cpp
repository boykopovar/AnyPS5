#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVisionManager/include/InventoryExports.hpp"
#include "prx/libSceVisionManager/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <algorithm>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x80e70000);
constexpr int ErrorUnavailable = static_cast<int>(0x80e70004);
constexpr int ErrorNotOpen = static_cast<int>(0x80e70025);
constexpr std::uint64_t LibraryVersion = 0x1020000214072421;
constexpr std::uint64_t RegisterUserDataVersion = 0x1017000014043000;

bool CopyIn(void* dst, const void* src, std::size_t size) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = saved;
    return ok;
}

bool CopyOut(void* dst, const void* src, std::size_t size) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = saved;
    return ok;
}

}

extern "C" {

int APS5_VABI sceVisionManagerApplyCalibrateHmdResult() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerCancelRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerClose() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGenerateRegisterUserData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGenerateRegisterUserDataFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGenerateRegisterUserDataInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetCalibrateTrackingLedInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetCalibrateTrackingLedNecessity() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetHitCountOfRegisterUserData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetLibraryVersion(std::uint64_t* version) {
    if (version == nullptr) return ErrorInvalidArgument;
    const std::uint64_t value = LibraryVersion;
    return CopyOut(version, &value, sizeof(value)) ? 0 : ErrorInvalidArgument;
}

int APS5_VABI sceVisionManagerGetNumOfRegisterUserDataSlot(std::uint64_t version, std::uint32_t* count) {
    if (count == nullptr) return ErrorInvalidArgument;
    std::uint32_t value = 0;
    if (version == RegisterUserDataVersion) value = 16;
    else if (version == LibraryVersion) value = 30;
    else return ErrorInvalidArgument;
    return CopyOut(count, &value, sizeof(value)) ? 0 : ErrorInvalidArgument;
}

int APS5_VABI sceVisionManagerGetRegisterUserDataSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetRegisterUserDataVersion() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetResultOfCalibrateHmd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetResultOfCalibrateStereoCamera() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetResultOfCalibrateTrackingLed() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetResultOfFindFace() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetResultOfFindPad(void* context, void* request, void* result) {
    if (context == nullptr || request == nullptr || result == nullptr) return ErrorInvalidArgument;
    return ErrorUnavailable;
}

int APS5_VABI sceVisionManagerGetResultOfFindPadCorrespondRegion(void* context, void* request, void* result) {
    if (context == nullptr || request == nullptr || result == nullptr) return ErrorInvalidArgument;
    return ErrorUnavailable;
}

int APS5_VABI sceVisionManagerGetResultOfFindUser() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerGetResultOfFindUserInRegion() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerGetResultOfFindUserWithoutIdentify() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerGetResultOfRegisterUserInRegion() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerGetStoreCameraImgYPtr() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerGetWorkingMemorySize(std::uint32_t* first, std::uint32_t* second, std::uint32_t* third) {
    if (first == nullptr || second == nullptr || third == nullptr) return ErrorInvalidArgument;
    const std::uint32_t valFirst = 0;
    const std::uint32_t valSecond = 0x1600000;
    const std::uint32_t valThird = 0x4200000;
    const int savedErrno = errno;
    const bool copiedFirst = GuestCopyOut_nid_no_patch(first, &valFirst, sizeof(valFirst));
    const bool copiedSecond = GuestCopyOut_nid_no_patch(second, &valSecond, sizeof(valSecond));
    const bool copiedThird = GuestCopyOut_nid_no_patch(third, &valThird, sizeof(valThird));
    errno = savedErrno;
    if (!copiedFirst || !copiedSecond || !copiedThird) return ErrorInvalidArgument;
    return 0;
}

int APS5_VABI sceVisionManagerInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerLoadSettings() {
    return 0;
}

int APS5_VABI sceVisionManagerOpen() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerRegistryClear() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerRequestCalibrateHmd() {
    return ErrorNotOpen;
}

int APS5_VABI sceVisionManagerRequestCalibrateStereoCamera() {
    return ErrorNotOpen;
}

int APS5_VABI sceVisionManagerRequestCalibrateTrackingLed() {
    return ErrorNotOpen;
}

int APS5_VABI sceVisionManagerRequestClose() {
    return ErrorNotOpen;
}

int APS5_VABI sceVisionManagerRequestFindFace() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerRequestFindPad(void* context, const std::int32_t* parameter) {
    std::int32_t value = 0;
    if (context == nullptr || parameter == nullptr || !CopyIn(&value, parameter, sizeof(value)) || value == 0) return ErrorInvalidArgument;
    return value < 5 ? ErrorUnavailable : ErrorInvalidArgument;
}

int APS5_VABI sceVisionManagerRequestFindPadCorrespondRegion(void* context, const std::int32_t* parameter, std::int32_t region, void* result) {
    std::int32_t value = 0;
    if (context == nullptr || parameter == nullptr || !CopyIn(&value, parameter, sizeof(value)) || value == 0) return ErrorInvalidArgument;
    if (region < 1 || region > 4 || result == nullptr) return ErrorInvalidArgument;
    return value < 5 ? ErrorUnavailable : ErrorInvalidArgument;
}

int APS5_VABI sceVisionManagerRequestFindUser() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerRequestFindUserInRegion() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerRequestFindUserWithoutIdentify() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerRequestHeadDetection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerRequestInitializeRegisterUserData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerRequestOpen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerRequestPadTracking(void* context, const std::int32_t* parameter) {
    static_cast<void>(context);
    std::int32_t value = 0;
    if (parameter == nullptr || !CopyIn(&value, parameter, sizeof(value)) || value == 0) return ErrorInvalidArgument;
    return value < 4 ? ErrorUnavailable : ErrorInvalidArgument;
}

int APS5_VABI sceVisionManagerRequestRecogUtility() {
    return ErrorNotOpen;
}

int APS5_VABI sceVisionManagerRequestRegisterUserInRegion() {
    return static_cast<int>(0x80e70004);
}

int APS5_VABI sceVisionManagerRequestStoreCameraImgY() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerSetCallbackForUpdateCalibrateHmdResult() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerSetCallbackForUpdateCalibrateStereoCameraResult() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerSetCallbackForUpdateCalibrateTrackingLedResult() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerSetCallbackForUpdateCameraFrame() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerSetCallbackForUpdateCtrlCore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerSetCallbackForUpdateRecog() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerSetCallbackForUpdateRunningStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerSetCallbackForUpdateStoreFrame() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVisionManagerWaitResponse() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceVisionManagerUnknown__minus_R5dI6KjYpY_nid_no_patch(const std::uint32_t* input, std::uint32_t* output) {
    if (input == nullptr || output == nullptr) return ErrorInvalidArgument;
    std::uint32_t localInput[2] = {};
    if (!CopyIn(localInput, input, sizeof(localInput))) return ErrorInvalidArgument;
    std::uint32_t localOutput[8] = {};
    const auto largest = std::max(localInput[0], localInput[1]);
    if (largest >= 10) localOutput[0] = largest >= 30 ? 10 : 5;
    return CopyOut(output, localOutput, sizeof(localOutput)) ? 0 : ErrorInvalidArgument;
}
APS5_EXPORT("-R5dI6KjYpY", libSceVisionManagerUnknown__minus_R5dI6KjYpY_nid_no_patch);

int APS5_VABI libSceVisionManagerUnknown_0chNZ7GKUO0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0chNZ7GKUO0", libSceVisionManagerUnknown_0chNZ7GKUO0_nid_no_patch);

int APS5_VABI libSceVisionManagerUnknown_9gGQMrf6sWg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9gGQMrf6sWg", libSceVisionManagerUnknown_9gGQMrf6sWg_nid_no_patch);

int APS5_VABI libSceVisionManagerUnknown_ORAn12NcHYs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ORAn12NcHYs", libSceVisionManagerUnknown_ORAn12NcHYs_nid_no_patch);

int APS5_VABI libSceVisionManagerUnknown_YQiB_minus_svtbcc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YQiB-svtbcc", libSceVisionManagerUnknown_YQiB_minus_svtbcc_nid_no_patch);

int APS5_VABI libSceVisionManagerUnknown_dzN0VRFvA84_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dzN0VRFvA84", libSceVisionManagerUnknown_dzN0VRFvA84_nid_no_patch);

int APS5_VABI libSceVisionManagerUnknown_wu4afKNlup4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wu4afKNlup4", libSceVisionManagerUnknown_wu4afKNlup4_nid_no_patch);

}
