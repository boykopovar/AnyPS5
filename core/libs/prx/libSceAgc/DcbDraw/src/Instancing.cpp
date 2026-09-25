#include "prx/libSceAgc/DcbDraw/include/Instancing.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetNumInstances(CommandBuffer* buf, std::uint32_t numInstances) {
    return Agc::Command::Emit(buf, 0x2fu, {numInstances}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetNumInstancesGetSize() {
    return 8;
}

uint32_t* APS5_VABI sceAgcDcbSetBaseIndirectArgs(CommandBuffer* buf, uint32_t shader_type, const volatile void* indirect_base_addr) {
    // SET_BASE with base index 1; header bit 1 selects the dispatch (compute) base over the draw base.
    Agc::Command::CheckBits(shader_type, 1, __func__);
    const auto address = reinterpret_cast<std::uintptr_t>(indirect_base_addr);
    Agc::Command::CheckAddress(address, 8, __func__);
    Agc::Command::CheckBits(address, 0xffffffffffffull, __func__);
    auto* packet = Agc::Command::Allocate(buf, 4, __func__);
    packet[0] = Agc::Command::Header(0x11u, 4, shader_type != 0 ? 2u : 0u);
    packet[1] = 1;
    packet[2] = static_cast<std::uint32_t>(address);
    packet[3] = static_cast<std::uint32_t>(address >> 32u);
    return packet;
}

}
