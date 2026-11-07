#include "prx/libScePadTracker/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_PAD_TRACKER_ERROR_NOT_INITIALIZED = static_cast<int>(0x80c40001);
constexpr int SCE_PAD_TRACKER_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80c40002);
constexpr int SCE_PAD_TRACKER_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80c40003);

std::mutex g_mutex;
bool g_initialized = false;

}

extern "C" {

int APS5_VABI scePadTrackerCalibrate() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PAD_TRACKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadTrackerGetWorkingMemorySize(std::uint32_t* first, std::uint32_t* second) {
    if (first == nullptr || second == nullptr) return SCE_PAD_TRACKER_ERROR_INVALID_ARGUMENT;
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
    if (!copiedFirst || !copiedSecond) return SCE_PAD_TRACKER_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI scePadTrackerInit() {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return SCE_PAD_TRACKER_ERROR_ALREADY_INITIALIZED;
    g_initialized = true;
    return 0;
}

int APS5_VABI scePadTrackerReadState() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PAD_TRACKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadTrackerTerm() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PAD_TRACKER_ERROR_NOT_INITIALIZED;
    g_initialized = false;
    return 0;
}

int APS5_VABI scePadTrackerUpdate() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_PAD_TRACKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
