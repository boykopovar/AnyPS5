#include "prx/libScePadTracker/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI scePadTrackerCalibrate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI scePadTrackerInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadTrackerReadState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadTrackerTerm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePadTrackerUpdate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
