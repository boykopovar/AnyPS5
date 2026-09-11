#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Output.hpp"

extern "C" {

int APS5_VABI sceVideoOutOpen(int user_id, int bus_type, int index, const void* param) {
    (void)user_id;
    (void)bus_type;
    (void)index;
    (void)param;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutClose(int handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutConfigureOutput(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved) {
    (void)handle;
    (void)mode;
    (void)options;
    (void)reserved_ptr;
    (void)reserved;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutIsOutputSupported(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved) {
    (void)handle;
    (void)mode;
    (void)options;
    (void)reserved_ptr;
    (void)reserved;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutInitializeOutputOptions(VideoOutOutputOptions* options) {
    (void)options;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutSetFlipRate(int handle, int rate) {
    (void)handle;
    (void)rate;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutSetWindowModeMargins(int handle, int top, int bottom) {
    (void)handle;
    (void)top;
    (void)bottom;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutGetFlipStatus(int handle, VideoOutFlipStatus* status) {
    (void)handle;
    (void)status;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status) {
    (void)handle;
    (void)status;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutGetVblankStatus(int handle, VideoOutVblankStatus* status) {
    (void)handle;
    (void)status;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutIsFlipPending(int handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutWaitVblank(int handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
