#include "prx/libSceMoveTracker/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceMoveTrackerCalibrateReset() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerCalibrateYaw(std::int32_t handle, const float* direction) {
    if (direction == nullptr) return static_cast<int>(0x80ef0003);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerCameraUpdate(const MoveTrackerCameraFrame* frames) {
    if (frames == nullptr) return static_cast<int>(0x80ef0003);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerCaptureInit(void* imageMemory, std::int32_t capacity, std::int32_t interval, const char* path, std::int32_t mode) {
    if (imageMemory == nullptr || path == nullptr) return static_cast<int>(0x80ef0003);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerCaptureTerm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerControllersUpdate(const MoveTrackerControllerInput* controllers) {
    if (controllers == nullptr) return static_cast<int>(0x80ef0003);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerGetState(std::int32_t handle, std::uint64_t timestamp, MoveTrackerState* state) {
    if (state == nullptr) return static_cast<int>(0x80ef0003);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerGetVersion() {
    return 2;
}

int APS5_VABI sceMoveTrackerGetWorkingMemorySize(std::uint32_t* totalSize, std::uint32_t* processingSize) {
    if (totalSize == nullptr || processingSize == nullptr) return static_cast<int>(0x80ef0003);
    std::uint32_t originalTotal = 0;
    const int savedErrno = errno;
    const bool readableTotal = GuestCopyIn_nid_no_patch(&originalTotal, totalSize, sizeof(originalTotal));
    const std::uint32_t total = 0xa00000;
    const std::uint32_t processing = 0x800000;
    const bool copiedTotal = GuestCopyOut_nid_no_patch(totalSize, &total, sizeof(total));
    const bool copiedProcessing = copiedTotal ? GuestCopyOut_nid_no_patch(processingSize, &processing, sizeof(processing)) : false;
    if (copiedTotal && !copiedProcessing && readableTotal) {
        GuestCopyOut_nid_no_patch(totalSize, &originalTotal, sizeof(originalTotal));
    }
    errno = savedErrno;
    if (!copiedTotal || !copiedProcessing) return static_cast<int>(0x80ef0003);
    return 0;
}

int APS5_VABI sceMoveTrackerInit(void* workMemory, std::uint64_t requiredArgument, std::uint32_t pipeId, std::uint32_t queueId) {
    if (workMemory == nullptr || requiredArgument == 0 || pipeId >= 8 || queueId >= 8) {
        return static_cast<int>(0x80ef0003);
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerPlayGetImages(MoveTrackerCameraFrame* frames) {
    if (frames == nullptr) return static_cast<int>(0x80ef0003);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerPlayGetStates(MoveTrackerPlaybackInfo* info, MoveTrackerState* states) {
    if (info == nullptr || states == nullptr) return static_cast<int>(0x80ef0003);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerPlayStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerPlayStep() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerRecordSave() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerRecordStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMoveTrackerRecordStop() {
    return 0;
}

int APS5_VABI sceMoveTrackerTerm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
