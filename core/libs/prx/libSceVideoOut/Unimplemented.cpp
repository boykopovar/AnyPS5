#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include <stdexcept>

extern "C" {

int APS5_VABI sceVideoOutSetFlipMaster(std::int32_t handle, std::int32_t masterHandle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutClearFlipMaster(std::int32_t handle) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorEnable(int32_t, int32_t, const void*) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorDisable(int32_t, int32_t) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorSetImageAddress(int32_t, int32_t, const void*) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutCursorSetPosition(int32_t, int32_t, uint32_t, uint32_t) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDbgVideoOutAddOutputModeVr60Privilege() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
