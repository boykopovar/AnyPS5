#include <limits>
#include <mutex>
#include <unordered_set>
#include <stdexcept>
#include <cstdint>
#include "prx/libSceCamera/libSceCamera.hpp"
#include "prx/libSceCamera/include/Camera.hpp"

namespace {

constexpr std::int32_t SCE_USER_SERVICE_USER_ID_SYSTEM = 0xFF;
constexpr std::uint32_t MAX_FRAME_DATA_SIZE = 584;

std::mutex handlesMutex;
std::unordered_set<std::int32_t> openHandles;
std::int64_t nextHandle = 1;

int CheckStart(std::int32_t handle, const CameraStartParameter* param) {
    if (handle < 1 || !param || param->sizeThis != sizeof(CameraStartParameter)) return SCE_CAMERA_ERROR_PARAM;
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
    if (const int error = CheckStart(handle, param)) return error;
    if (param->formatLevel[0] > 0xF || param->formatLevel[1] > 0xF || (param->formatLevel[0] | param->formatLevel[1]) == 0)
        return SCE_CAMERA_ERROR_FORMAT_UNKNOWN;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceCameraStartByHandle(std::int32_t handle, const CameraStartParameter* param) {
    return CheckStart(handle, param);
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
    frameData->status[0] = UINT32_MAX;
    frameData->status[1] = UINT32_MAX;
    if (handle < 1 || frameData->sizeThis > MAX_FRAME_DATA_SIZE) return SCE_CAMERA_ERROR_PARAM;
    return SCE_CAMERA_ERROR_NOT_OPEN;
}

int APS5_VABI sceCameraIsValidFrameData(std::int32_t handle, const CameraFrameData* frameData) {
    if (handle < 1 || !frameData || frameData->sizeThis > MAX_FRAME_DATA_SIZE) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen(handle)) return SCE_CAMERA_ERROR_NOT_OPEN;
    return frameData->status[0] == 1 && frameData->status[1] == 1;
}

}
