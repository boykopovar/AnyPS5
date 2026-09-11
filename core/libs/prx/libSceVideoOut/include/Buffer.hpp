#ifndef CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_OUTPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_OUTPUT_HPP

#include <cstdint>

#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoOutRegisterBuffers2(int handle, int set_index, int buffer_index_start, const VideoOutBuffers* buffers, int buffer_num, const VideoOutBufferAttribute2* attribute, int category, void* option);
void APS5_VABI sceVideoOutSetBufferAttribute2(VideoOutBufferAttribute2* attribute, uint64_t pixel_format, uint32_t tiling_mode, uint32_t width, uint32_t height, uint64_t option, uint32_t dcc_control, uint64_t dcc_cb_register_clear_color);
int APS5_VABI sceVideoOutSubmitChangeBufferAttribute2(int handle, int set_index, const VideoOutBufferAttribute2* attribute, void* option);
int APS5_VABI sceVideoOutSubmitFlip(int handle, int index, int flip_mode, int64_t flip_arg);
int APS5_VABI sceVideoOutUnregisterBuffers(int handle, int set_index);

#endif

}
