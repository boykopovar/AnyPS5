#include "prx/libSceAgc/DcbState/include/Display.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetFlip(CommandBuffer* buf, uint32_t video_out_handle, int32_t display_buffer_index, uint32_t flip_mode, int64_t flip_arg) {
 (void)buf;
 (void)video_out_handle;
 (void)display_buffer_index;
 (void)flip_mode;
 (void)flip_arg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbPrimeUtcl2(CommandBuffer* buf, const volatile void* address, uint32_t size_in_bytes) {
    (void)buf;
    (void)address;
    (void)size_in_bytes;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

}
