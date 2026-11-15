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
    const auto address = reinterpret_cast<std::uintptr_t>(indirect_base_addr);
    Agc::Command::Require((address & 1u) == 0, __func__, "an odd indirect base address is not measured");
    auto* packet = Agc::Command::Allocate(buf, 4, __func__);
    packet[0] = Agc::Command::Header(0x11u, 4) | (shader_type << 1u);
    packet[1] = 1;
    packet[2] = static_cast<std::uint32_t>(address) & ~6u;
    packet[3] = static_cast<std::uint32_t>(address >> 32u);
    return packet;
}

}
