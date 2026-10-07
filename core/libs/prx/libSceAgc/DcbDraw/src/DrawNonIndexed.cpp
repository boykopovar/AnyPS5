#include "prx/libSceAgc/Command/include/Draw.hpp"
#include "prx/libSceAgc/DcbDraw/include/DrawNonIndexed.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbDrawIndirect(CommandBuffer* buf, std::uint32_t dataOffsetInBytes,
                                               std::uint64_t modifier) {
    Agc::Command::Require((dataOffsetInBytes & 3u) == 0, __func__, "misaligned indirect argument offset");
    const auto offsets = Agc::Command::DrawPatchOffsets(modifier, __func__);
    return Agc::Command::Emit(buf, 0x24u,
                              {dataOffsetInBytes, static_cast<std::uint32_t>(offsets),
                               static_cast<std::uint32_t>(offsets >> 32u),
                               Agc::Command::DrawInitiator(modifier, false, __func__)},
                              __func__);
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndirectGetSize() { return 20; }

std::uint32_t* APS5_VABI sceAgcDcbDrawIndirectMulti(CommandBuffer* buf, std::uint32_t dataOffsetInBytes,
                                                    std::uint32_t countIndirect, std::uint32_t maxCountOrCount,
                                                    const volatile void* countAddress, std::uint32_t strideInBytes,
                                                    std::uint64_t modifier) {
    Agc::Command::CheckBits(countIndirect, 1, __func__);
    Agc::Command::Require((dataOffsetInBytes & 3u) == 0 && (strideInBytes & 3u) == 0 && strideInBytes >= 16, __func__,
                          "invalid indirect draw offset or stride");
    const auto address = reinterpret_cast<std::uintptr_t>(countAddress);
    if (countIndirect != 0) {
        Agc::Command::CheckAddress(address, 4, __func__);
    } else {
        Agc::Command::Require(address == 0, __func__, "count address supplied for a direct draw count");
    }
    const auto offsets = Agc::Command::DrawPatchOffsets(modifier, __func__);
    const auto initiator = Agc::Command::DrawInitiator(modifier, false, __func__);
    const auto low = static_cast<std::uint32_t>(modifier);
    const auto control = Agc::Command::DrawIndexLocation(modifier) | ((low & 0x10u) << 23u) | (countIndirect << 30u) |
                         ((low & 8u) << 28u);
    auto* packet = Agc::Command::Allocate(buf, 16, __func__);
    packet[0] = Agc::Command::Header(0x79u, 3, 1);
    packet[1] = 0x342u;
    packet[2] = 0xc6000008u;
    packet[3] = Agc::Command::Header(0x2cu, 10);
    packet[4] = dataOffsetInBytes;
    packet[5] = static_cast<std::uint32_t>(offsets);
    packet[6] = static_cast<std::uint32_t>(offsets >> 32u);
    packet[7] = control;
    packet[8] = maxCountOrCount;
    packet[9] = static_cast<std::uint32_t>(address);
    packet[10] = static_cast<std::uint32_t>(address >> 32u);
    packet[11] = strideInBytes;
    packet[12] = initiator;
    packet[13] = Agc::Command::Header(0x79u, 3, 1);
    packet[14] = 0x342u;
    packet[15] = 0xc6000000u;
    return packet;
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndirectMultiGetSize() { return 64; }
}
