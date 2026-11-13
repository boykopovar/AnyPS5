#include "prx/libSceAgc/Command/include/Draw.hpp"
#include "prx/libSceAgc/DcbDraw/include/DrawIndexed.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbDrawIndex(CommandBuffer* buf, std::uint32_t indexCount, const volatile void* indexAddress, std::uint64_t modifier) {
    const auto address = reinterpret_cast<std::uintptr_t>(indexAddress);
    Agc::Command::CheckAddress(address, 1, __func__);
    return Agc::Command::Emit(buf, 0x27u, {indexCount, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), indexCount, Agc::Command::DrawInitiator(modifier, true, __func__)}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndexGetSize() {
    return 24;
}

std::uint32_t* APS5_VABI sceAgcDcbDrawIndexAuto(CommandBuffer* buf, std::uint32_t indexCount, std::uint64_t modifier) {
    return Agc::Command::Emit(buf, 0x2du, {indexCount, Agc::Command::DrawInitiator(modifier, false, __func__)}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndexAutoGetSize() {
    return 12;
}

std::uint32_t* APS5_VABI sceAgcDcbDrawIndexOffset(CommandBuffer* buf, std::uint32_t indexOffset, std::uint32_t indexCount, std::uint64_t modifier) {
    return Agc::Command::Emit(buf, 0x35u, {indexCount == 0 ? 1u : indexCount, indexOffset, indexCount, Agc::Command::DrawInitiator(modifier, true, __func__)}, __func__);
}

uint32_t APS5_VABI sceAgcDcbDrawIndexOffsetGetSize(void) {
    return 20;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint64_t modifier) {
    Agc::Command::Require((data_offset_in_bytes & 3u) == 0, __func__, "misaligned indirect argument offset");
    const auto offsets = Agc::Command::DrawPatchOffsets(modifier, __func__);
    return Agc::Command::Emit(buf, 0x25u, {data_offset_in_bytes, static_cast<std::uint32_t>(offsets), static_cast<std::uint32_t>(offsets >> 32u), Agc::Command::DrawInitiator(modifier, true, __func__)}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndexIndirectGetSize() {
    return 20;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirectMulti(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t count_indirect, uint32_t max_count_or_count, const volatile void* count_addr, uint32_t stride_in_bytes, uint64_t modifier) {
    Agc::Command::CheckBits(count_indirect, 1, __func__);
    Agc::Command::Require((data_offset_in_bytes & 3u) == 0 && (stride_in_bytes & 3u) == 0 && stride_in_bytes >= 20, __func__, "invalid indirect draw offset or stride");
    const auto address = reinterpret_cast<std::uintptr_t>(count_addr);
    if (count_indirect != 0) {
        Agc::Command::CheckAddress(address, 4, __func__);
    } else {
        Agc::Command::Require(address == 0, __func__, "count address supplied for a direct draw count");
    }
    const auto offsets = Agc::Command::DrawPatchOffsets(modifier, __func__);
    const auto low = static_cast<std::uint32_t>(modifier);
    const auto control = Agc::Command::DrawIndexLocation(modifier) | ((low & 0x10u) << 23u) | (count_indirect << 30u) | ((low & 8u) << 28u);
    const auto initiator = Agc::Command::DrawInitiator(modifier, true, __func__);
    auto* packet = Agc::Command::Allocate(buf, 16, __func__);
    packet[0] = Agc::Command::Header(0x79u, 3, 4);
    packet[1] = 0x342u;
    packet[2] = 0xc6000005u;
    packet[3] = Agc::Command::Header(0x38u, 10);
    packet[4] = data_offset_in_bytes;
    packet[5] = static_cast<std::uint32_t>(offsets);
    packet[6] = static_cast<std::uint32_t>(offsets >> 32u);
    packet[7] = control;
    packet[8] = max_count_or_count;
    packet[9] = static_cast<std::uint32_t>(address);
    packet[10] = static_cast<std::uint32_t>(address >> 32u);
    packet[11] = stride_in_bytes;
    packet[12] = initiator;
    packet[13] = Agc::Command::Header(0x79u, 3, 4);
    packet[14] = 0x342u;
    packet[15] = 0xc6000000u;
    return packet;
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndexIndirectMultiGetSize() {
    return 64;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexMultiInstanced(CommandBuffer* buf, uint32_t index_count, const volatile void* index_addr, const volatile void* object_ids, uint32_t instance_count, uint64_t) {
    const auto geometry = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(index_addr));
    const auto objects = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(object_ids));
    auto* packet = Agc::Command::Allocate(buf, 15, __func__);
    packet[0] = Agc::Command::Header(0x79u, 3, 4);
    packet[1] = 0x342u;
    packet[2] = 0xc6000006u;
    packet[3] = Agc::Command::Header(0x3au, 9);
    packet[4] = index_count;
    packet[5] = static_cast<std::uint32_t>(geometry);
    packet[6] = static_cast<std::uint32_t>(geometry >> 32u);
    packet[7] = instance_count == 0 ? 1u : instance_count;
    packet[8] = static_cast<std::uint32_t>(objects);
    packet[9] = static_cast<std::uint32_t>(objects >> 32u);
    packet[10] = instance_count;
    packet[11] = 0x80u;
    packet[12] = Agc::Command::Header(0x79u, 3, 4);
    packet[13] = 0x342u;
    packet[14] = 0xc6000000u;
    return packet;
}

uint32_t APS5_VABI sceAgcDcbDrawIndexMultiInstancedGetSize(void) {
    return 60;
}

}
