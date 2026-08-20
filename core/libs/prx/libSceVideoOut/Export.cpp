#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceVideoOutAddFlipEvent(KernelEqueue eq, int handle, void* udata) {
 (void)eq;
 (void)handle;
 (void)udata;
 return 0;
}

int sceVideoOutAddOutputModeEvent(KernelEqueue eq, int handle, void* udata) {
 (void)eq;
 (void)handle;
 (void)udata;
 return 0;
}

int sceVideoOutAddPreVblankStartEvent(KernelEqueue eq, int handle, void* udata) {
 (void)eq;
 (void)handle;
 (void)udata;
 return 0;
}

int sceVideoOutAddVblankEvent(KernelEqueue eq, int handle, void* udata) {
 (void)eq;
 (void)handle;
 (void)udata;
 return 0;
}

int sceVideoOutClose(int handle) {
 (void)handle;
 return 0;
}

int sceVideoOutConfigureOutput(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved) {
 (void)handle;
 (void)mode;
 (void)options;
 (void)reserved_ptr;
 (void)reserved;
 return 0;
}

int sceVideoOutDeleteFlipEvent(KernelEqueue eq, int handle) {
 (void)eq;
 (void)handle;
 return 0;
}

int sceVideoOutDeletePreVblankStartEvent(KernelEqueue eq, int handle) {
 (void)eq;
 (void)handle;
 return 0;
}

int sceVideoOutDeleteVblankEvent(KernelEqueue eq, int handle) {
 (void)eq;
 (void)handle;
 return 0;
}

int sceVideoOutGetEventCount(const KernelEvent* ev) {
 (void)ev;
 return 0;
}

int sceVideoOutGetEventData(const KernelEvent* ev, int64_t* data) {
 (void)ev;
 (void)data;
 return 0;
}

int sceVideoOutGetEventId(const KernelEvent* ev) {
 (void)ev;
 return 0;
}

int sceVideoOutGetFlipStatus(int handle, VideoOutFlipStatus* status) {
 (void)handle;
 (void)status;
 return 0;
}

int sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status) {
 (void)handle;
 (void)status;
 return 0;
}

int sceVideoOutGetVblankStatus(int handle, VideoOutVblankStatus* status) {
 (void)handle;
 (void)status;
 return 0;
}

int sceVideoOutInitializeOutputOptions(VideoOutOutputOptions* options) {
 (void)options;
 return 0;
}

int sceVideoOutIsFlipPending(int handle) {
 (void)handle;
 return 0;
}

int sceVideoOutIsOutputSupported(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved) {
 (void)handle;
 (void)mode;
 (void)options;
 (void)reserved_ptr;
 (void)reserved;
 return 0;
}

int sceVideoOutOpen(int user_id, int bus_type, int index, const void* param) {
 (void)user_id;
 (void)bus_type;
 (void)index;
 (void)param;
 return 0;
}

int sceVideoOutRegisterBuffers2(int handle, int set_index, int buffer_index_start, const VideoOutBuffers* buffers, int buffer_num, const VideoOutBufferAttribute2* attribute, int category, void* option) {
 (void)handle;
 (void)set_index;
 (void)buffer_index_start;
 (void)buffers;
 (void)buffer_num;
 (void)attribute;
 (void)category;
 (void)option;
 return 0;
}

void sceVideoOutSetBufferAttribute2(VideoOutBufferAttribute2* attribute, uint64_t pixel_format, uint32_t tiling_mode, uint32_t width, uint32_t height, uint64_t option, uint32_t dcc_control, uint64_t dcc_cb_register_clear_color) {
 (void)attribute;
 (void)pixel_format;
 (void)tiling_mode;
 (void)width;
 (void)height;
 (void)option;
 (void)dcc_control;
 (void)dcc_cb_register_clear_color;
}

int sceVideoOutSetFlipRate(int handle, int rate) {
 (void)handle;
 (void)rate;
 return 0;
}

int sceVideoOutSetWindowModeMargins(int handle, int top, int bottom) {
 (void)handle;
 (void)top;
 (void)bottom;
 return 0;
}

int sceVideoOutSubmitChangeBufferAttribute2(int handle, int set_index, const VideoOutBufferAttribute2* attribute, void* option) {
 (void)handle;
 (void)set_index;
 (void)attribute;
 (void)option;
 return 0;
}

int sceVideoOutSubmitFlip(int handle, int index, int flip_mode, int64_t flip_arg) {
 (void)handle;
 (void)index;
 (void)flip_mode;
 (void)flip_arg;
 return 0;
}

int sceVideoOutUnregisterBuffers(int handle, int set_index) {
 (void)handle;
 (void)set_index;
 return 0;
}

int sceVideoOutWaitVblank(int handle) {
 (void)handle;
 return 0;
}

}
