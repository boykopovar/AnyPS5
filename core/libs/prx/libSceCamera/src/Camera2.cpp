#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "prx/libSceCamera/libSceCamera.hpp"
#include "prx/libSceCamera/include/Camera.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

constexpr int SCE_CAMERA2_ERROR_PARAM = static_cast<int>(0x8A760000);
constexpr int SCE_CAMERA2_ERROR_NOT_INIT = static_cast<int>(0x8A760002);
constexpr int SCE_CAMERA2_ERROR_CAMERA_OPEN = static_cast<int>(0x8A760015);
constexpr std::int32_t SCE_USER_SERVICE_USER_ID_SYSTEM = 0xFF;
constexpr std::uint32_t UNCHECKED_CATEGORY_START = 0x1000000;
constexpr std::uint32_t UNCHECKED_CATEGORY = 0x20F00;

std::atomic<bool> initialized{false};

std::uint32_t AppCategory() {
    constexpr std::size_t AppInfoBytes = 0x58;
    constexpr std::size_t CategoryOffset = 0x50;
    std::uint8_t info[AppInfoBytes];
    std::uint32_t category = 0;
    if (sceKernelGetAppInfo(getpid_nid_postfix(), info) == 0) std::memcpy(&category, info + CategoryOffset, sizeof(category));
    return category;
}

bool InitializationChecked(std::uint32_t category) {
    return category < UNCHECKED_CATEGORY_START && category != UNCHECKED_CATEGORY;
}

int CheckInitialized() {
    if (InitializationChecked(AppCategory()) && !initialized.load()) return SCE_CAMERA2_ERROR_NOT_INIT;
    return 0;
}

int RejectHandle() {
    if (const int error = CheckInitialized()) return error;
    return SCE_CAMERA2_ERROR_PARAM;
}

}

extern "C" {

int APS5_VABI sceCamera2Initialize(void) {
    if (!initialized.load() && InitializationChecked(AppCategory()))
        throw std::runtime_error("sceCamera2Initialize: the driver mode and Eve checks for this app category");
    initialized.store(true);
    return 0;
}

int APS5_VABI sceCamera2Finalize(void) {
    if (const int error = CheckInitialized()) return error;
    if (Camera::IsOpen()) return SCE_CAMERA2_ERROR_CAMERA_OPEN;
    initialized.store(false);
    return 0;
}

int APS5_VABI sceCamera2Open(std::int32_t userId, std::int32_t type, std::int32_t index, void*) {
    if (const int error = CheckInitialized()) return error;
    if (userId != SCE_USER_SERVICE_USER_ID_SYSTEM || type != 0 || index != 0) return SCE_CAMERA2_ERROR_PARAM;
    throw std::runtime_error("sceCamera2Open: opening the camera device");
}

int APS5_VABI sceCamera2IsAttached(std::int32_t device) {
    if (const int error = CheckInitialized()) return error;
    if (device != 1) return SCE_CAMERA2_ERROR_PARAM;
    throw std::runtime_error("sceCamera2IsAttached: asking the camera driver");
}

int APS5_VABI sceCamera2Close(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetAttribute(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetAttribute(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetAutoExposureGain(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetAutoExposureGain(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetAutoWhiteBalance(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetAutoWhiteBalance(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetConfig(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetConfig(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetContrast(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetContrast(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetExposureGain(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetExposureGain(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetGamma(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetGamma(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetHue(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetHue(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetSaturation(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetSaturation(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetSharpness(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetSharpness(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetWhiteBalance(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetWhiteBalance(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetFieldOfView(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2GetFrameData(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2IsValidFrameData(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2SetVideoSync(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2Start(std::int32_t) {
    return RejectHandle();
}

int APS5_VABI sceCamera2Stop(std::int32_t) {
    return RejectHandle();
}

}
