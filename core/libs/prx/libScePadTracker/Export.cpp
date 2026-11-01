#include "prx/libScePadTracker/include/Api.hpp"
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
    *first = 0x800000;
    *second = 0x800000;
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
