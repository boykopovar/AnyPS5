#include <cstdint>
#include "SceTypes.hpp"

#include "prx/libSceVideoOut/include/Buffer.hpp"

extern "C" {

int APS5_VABI sceVideoOutRegisterBuffers2(int handle, int set_index, int buffer_index_start, const VideoOutBuffers* buffers, int buffer_num, const VideoOutBufferAttribute2* attribute, int category, void* option) {
    (void)handle;
    (void)set_index;
    (void)buffer_index_start;
    (void)buffers;
    (void)buffer_num;
    (void)attribute;
    (void)category;
    (void)option;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceVideoOutSetBufferAttribute2(VideoOutBufferAttribute2* attribute, uint64_t pixel_format, uint32_t tiling_mode, uint32_t width, uint32_t height, uint64_t option, uint32_t dcc_control, uint64_t dcc_cb_register_clear_color) {
    (void)attribute;
    (void)pixel_format;
    (void)tiling_mode;
    (void)width;
    (void)height;
    (void)option;
    (void)dcc_control;
    (void)dcc_cb_register_clear_color;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoOutSubmitChangeBufferAttribute2(int handle, int set_index, const VideoOutBufferAttribute2* attribute, void* option) {
    (void)handle;
    (void)set_index;
    (void)attribute;
    (void)option;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutSubmitFlip(int handle, int index, int flip_mode, int64_t flip_arg) {
    (void)handle;
    (void)index;
    (void)flip_mode;
    (void)flip_arg;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoOutUnregisterBuffers(int handle, int set_index) {
    (void)handle;
    (void)set_index;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
