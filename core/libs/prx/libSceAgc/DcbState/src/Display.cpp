#include "prx/libSceAgc/DcbState/include/Display.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetFlip(CommandBuffer* buf, uint32_t video_out_handle, int32_t display_buffer_index, uint32_t flip_mode, int64_t flip_arg) {
    auto* packet = Agc::Command::Allocate(buf, AgcDriver::FlipPacketWords, __func__);
    packet[0] = AgcDriver::FlipPacketHeader;
    packet[1] = video_out_handle;
    packet[2] = static_cast<std::uint32_t>(display_buffer_index);
    packet[3] = flip_mode;
    packet[4] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(flip_arg));
    packet[5] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(flip_arg) >> 32u);
    return packet;
}

uint32_t* APS5_VABI sceAgcDcbPrimeUtcl2(CommandBuffer* buf, const volatile void* address, uint32_t size_in_bytes) {
    return Agc::Command::WritePrimeUtcl2(buf, address, size_in_bytes, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbPrimeUtcl2GetSize() {
    return 20;
}

}
