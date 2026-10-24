#include <atomic>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libSceCamera/include/Camera.hpp"

namespace {

constexpr std::int32_t SCE_USER_SERVICE_USER_ID_SYSTEM = 0xFF;
constexpr std::uint32_t MAX_FRAME_DATA_SIZE = 584;

struct CameraStartParameter {
    std::uint32_t sizeThis;
    std::uint32_t formatLevel[2];
    void* startOption;
};

struct CameraFrameData {
    std::uint32_t sizeThis;
    std::uint32_t readMode;
    std::uint32_t framePosition[2][4][4];
    void* framePointerList[2][4];
    std::uint32_t frameSize[2][4];
    std::uint32_t status[2];
};

std::atomic<std::int32_t> openHandles{0};

int CheckStart(std::int32_t handle, const CameraStartParameter* param) {
    if (handle < 1 || !param || param->sizeThis != sizeof(CameraStartParameter)) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_OPEN;
    return 0;
}

}

bool Camera::IsOpen() {
    return openHandles.load() != 0;
}

extern "C" {

int APS5_VABI sceCameraOpen(std::int32_t userId, std::int32_t type, std::int32_t index, void*) {
    if (userId != SCE_USER_SERVICE_USER_ID_SYSTEM || type != 0 || index != 0) return SCE_CAMERA_ERROR_PARAM;
    return ++openHandles;
}

int APS5_VABI sceCameraClose(std::int32_t handle) {
    if (handle < 1) return SCE_CAMERA_ERROR_PARAM;
    std::int32_t open = openHandles.load();
    do {
        if (open == 0) return SCE_CAMERA_ERROR_NOT_OPEN;
    } while (!openHandles.compare_exchange_weak(open, open - 1));
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
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_OPEN;
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
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_OPEN;
    return frameData->status[0] == 1 && frameData->status[1] == 1;
}

}
