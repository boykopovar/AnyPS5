#include <limits>
#include <cerrno>
#include <cstddef>
#include <mutex>
#include <unordered_set>
#include <stdexcept>
#include <cstdint>
#include "prx/libSceCamera/libSceCamera.hpp"
#include "prx/libSceCamera/include/Camera.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_USER_SERVICE_USER_ID_SYSTEM = 0xFF;
constexpr std::uint32_t MAX_FRAME_DATA_SIZE = 584;

std::mutex handlesMutex;
std::unordered_set<std::int32_t> openHandles;
std::int64_t nextHandle = 1;

int CheckStart(std::int32_t handle, const CameraStartParameter* param, CameraStartParameter& local) {
    if (handle < 1 || !param) return SCE_CAMERA_ERROR_PARAM;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&local, param, sizeof(local));
    errno = savedErrno;
    if (!copied || local.sizeThis != sizeof(CameraStartParameter)) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen(handle)) return SCE_CAMERA_ERROR_NOT_OPEN;
    return 0;
}

}

bool Camera::IsOpen() {
    std::lock_guard lock(handlesMutex);
    return !openHandles.empty();
}

bool Camera::IsOpen(std::int32_t handle) {
    std::lock_guard lock(handlesMutex);
    return openHandles.contains(handle);
}

extern "C" {

int APS5_VABI sceCameraOpen(std::int32_t userId, std::int32_t type, std::int32_t index, void*) {
    if (userId != SCE_USER_SERVICE_USER_ID_SYSTEM || type != 0 || index != 0) return SCE_CAMERA_ERROR_PARAM;
    std::lock_guard lock(handlesMutex);
    if (nextHandle > std::numeric_limits<std::int32_t>::max()) throw std::runtime_error("camera handle space exhausted");
    const auto handle = static_cast<std::int32_t>(nextHandle);
    openHandles.insert(handle);
    ++nextHandle;
    return handle;
}

int APS5_VABI sceCameraClose(std::int32_t handle) {
    if (handle < 1) return SCE_CAMERA_ERROR_PARAM;
    std::lock_guard lock(handlesMutex);
    if (openHandles.erase(handle) == 0) return SCE_CAMERA_ERROR_NOT_OPEN;
    return 0;
}

int APS5_VABI sceCameraCloseByHandle(std::int32_t handle) {
    return sceCameraClose(handle);
}

int APS5_VABI sceCameraStart(std::int32_t handle, const CameraStartParameter* param) {
    CameraStartParameter local{};
    if (const int error = CheckStart(handle, param, local)) return error;
    if (local.formatLevel[0] > 0xF || local.formatLevel[1] > 0xF || (local.formatLevel[0] | local.formatLevel[1]) == 0)
        return SCE_CAMERA_ERROR_FORMAT_UNKNOWN;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceCameraStartByHandle(std::int32_t handle, const CameraStartParameter* param) {
    CameraStartParameter local{};
    return CheckStart(handle, param, local);
}

int APS5_VABI sceCameraStop(std::int32_t handle) {
    if (handle < 1) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen(handle)) return SCE_CAMERA_ERROR_NOT_OPEN;
    return 0;
}

int APS5_VABI sceCameraStopByHandle(std::int32_t handle) {
    return sceCameraStop(handle);
}

int APS5_VABI sceCameraGetFrameData(std::int32_t handle, CameraFrameData* frameData) {
    if (!frameData) return SCE_CAMERA_ERROR_PARAM;
    const auto base = reinterpret_cast<std::uintptr_t>(frameData);
    if (base > std::numeric_limits<std::uintptr_t>::max() - sizeof(CameraFrameData)) return SCE_CAMERA_ERROR_PARAM;
    const std::uint32_t status[2] = {UINT32_MAX, UINT32_MAX};
    std::uint32_t sizeThis = 0;
    const int savedErrno = errno;
    const bool wrote = GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(base + offsetof(CameraFrameData, status)), status, sizeof(status));
    const bool read = wrote && GuestCopyIn_nid_no_patch(&sizeThis, reinterpret_cast<const void*>(base + offsetof(CameraFrameData, sizeThis)), sizeof(sizeThis));
    errno = savedErrno;
    if (!read || handle < 1 || sizeThis > MAX_FRAME_DATA_SIZE) return SCE_CAMERA_ERROR_PARAM;
    return SCE_CAMERA_ERROR_NOT_OPEN;
}

int APS5_VABI sceCameraIsValidFrameData(std::int32_t handle, const CameraFrameData* frameData) {
    if (handle < 1 || !frameData) return SCE_CAMERA_ERROR_PARAM;
    CameraFrameData localData{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localData, frameData, sizeof(localData));
    errno = savedErrno;
    if (!copied || localData.sizeThis > MAX_FRAME_DATA_SIZE) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen(handle)) return SCE_CAMERA_ERROR_NOT_OPEN;
    return localData.status[0] == 1 && localData.status[1] == 1;
}

}
