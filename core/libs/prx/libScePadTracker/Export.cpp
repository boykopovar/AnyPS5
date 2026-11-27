#include "prx/libScePadTracker/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <mutex>

namespace {

constexpr int PAD_TRACKER_ERROR_NOT_INITIALIZED = static_cast<int>(0x80c40001u);
constexpr int PAD_TRACKER_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80c40002u);
constexpr int PAD_TRACKER_ERROR_INVALID_ARG = static_cast<int>(0x80c40003u);
constexpr int PAD_TRACKER_ERROR_HANDLE_NOT_FOUND = static_cast<int>(0x80c40004u);
constexpr std::uintptr_t MemoryAlignment = 0x4000;

std::mutex trackerMutex;
bool trackerInitialized = false;

bool ValidMemory(const void* memory) {
    return memory != nullptr && (reinterpret_cast<std::uintptr_t>(memory) & (MemoryAlignment - 1)) == 0;
}

}

extern "C" {

int APS5_VABI scePadTrackerCalibrate(void) {
    std::lock_guard lock(trackerMutex);
    return trackerInitialized ? 0 : PAD_TRACKER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePadTrackerGetWorkingMemorySize(std::uint32_t* first, std::uint32_t* second) {
    if (first == nullptr || second == nullptr) return static_cast<int>(0x80c40003);
    std::uint32_t originalFirst = 0;
    const int savedErrno = errno;
    const bool readableFirst = GuestCopyIn_nid_no_patch(&originalFirst, first, sizeof(originalFirst));
    const std::uint32_t memorySize = 0x800000;
    const bool copiedFirst = GuestCopyOut_nid_no_patch(first, &memorySize, sizeof(memorySize));
    const bool copiedSecond = copiedFirst ? GuestCopyOut_nid_no_patch(second, &memorySize, sizeof(memorySize)) : false;
    if (copiedFirst && !copiedSecond && readableFirst) {
        GuestCopyOut_nid_no_patch(first, &originalFirst, sizeof(originalFirst));
    }
    errno = savedErrno;
    if (!copiedFirst || !copiedSecond) return static_cast<int>(0x80c40003);
    return 0;
}

int APS5_VABI scePadTrackerInit(void* firstMemory, void* secondMemory, std::int32_t first, std::int32_t second) {
    if (!ValidMemory(firstMemory) || !ValidMemory(secondMemory)) return PAD_TRACKER_ERROR_INVALID_ARG;
    if (first < 0 || first > 6 || second < 0 || second > 7) return PAD_TRACKER_ERROR_INVALID_ARG;
    std::lock_guard lock(trackerMutex);
    if (trackerInitialized) return PAD_TRACKER_ERROR_ALREADY_INITIALIZED;
    trackerInitialized = true;
    return 0;
}

int APS5_VABI scePadTrackerReadState(std::int32_t handle, void* state) {
    std::lock_guard lock(trackerMutex);
    if (!trackerInitialized) return PAD_TRACKER_ERROR_NOT_INITIALIZED;
    if (state == nullptr) return PAD_TRACKER_ERROR_INVALID_ARG;
    if (handle == 0) NotImplemented_nid_no_patch("scePadTrackerReadState for handle 0 before an update");
    return PAD_TRACKER_ERROR_HANDLE_NOT_FOUND;
}

int APS5_VABI scePadTrackerTerm(void) {
    std::lock_guard lock(trackerMutex);
    if (!trackerInitialized) return PAD_TRACKER_ERROR_NOT_INITIALIZED;
    trackerInitialized = false;
    return 0;
}

int APS5_VABI scePadTrackerUpdate() {
    {
        std::lock_guard lock(trackerMutex);
        if (!trackerInitialized) return PAD_TRACKER_ERROR_NOT_INITIALIZED;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
