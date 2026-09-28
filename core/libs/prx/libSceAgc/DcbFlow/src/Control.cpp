#include "prx/libSceAgc/DcbFlow/include/Control.hpp"

#include "prx/libSceAgc/Command/include/Control.hpp"
#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"


extern "C" {

// unknown signature
APS5_EXPORT("zARR5aCmkoY", sceAgcDcbA_zARR5aCmkoY);
void* APS5_VABI sceAgcDcbA_zARR5aCmkoY(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}


uint32_t* APS5_VABI sceAgcDcbJump(CommandBuffer* buf, uint8_t mode, uint8_t cache_policy, const uint32_t* target, uint32_t size_in_dwords) {
    // mode 1 chains (the target replaces the rest of this buffer), mode 0 calls and returns after the target.
    return Agc::Command::WriteJump(buf, mode, cache_policy, target, size_in_dwords, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbJumpGetSize() {
    return 16;
}

std::uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, std::uint32_t op, std::uint32_t state) {
    Agc::Command::CheckBits(op, 0xfffu, __func__);
    Agc::Command::CheckBits(state, 0xfu, __func__);
    return Agc::Command::Emit(buf, 0x12u, {state}, __func__);
}

uint32_t* APS5_VABI sceAgcDcbRewind(CommandBuffer* buf, uint32_t initial_state) {
    return Agc::Command::WriteRewind(buf, initial_state, __func__);
}

uint32_t APS5_VABI sceAgcDcbRewindGetSize(void) {
    return 8; // REWIND: header + 1 dword
}

uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer* buf, uint32_t video_out_handle, uint32_t display_buffer_index) {
    // Same packet sceAgcDriverWaitUntilSafeForRendering produces; the executor specifically recognizes
    // AgcDriver::RenderingWaitPacketHeader (see Driver.cpp) and blocks on the display-buffer reuse fence.
    // A plain NOP with these arguments (upstream's previous stub) is not matched by that check.
    auto* packet = Agc::Command::Allocate(buf, AgcDriver::RenderingWaitPacketWords, __func__);
    packet[0] = AgcDriver::RenderingWaitPacketHeader;
    packet[1] = video_out_handle;
    packet[2] = display_buffer_index;
    packet[3] = 0;
    return packet;
}

}
