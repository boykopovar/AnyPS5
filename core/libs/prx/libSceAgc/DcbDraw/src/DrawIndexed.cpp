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
 NotImplemented_nid_no_patch(__func__);
 return 0;
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
    return Agc::Command::Emit(buf, 0x38u, {data_offset_in_bytes, static_cast<std::uint32_t>(offsets), static_cast<std::uint32_t>(offsets >> 32u), control, max_count_or_count, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), stride_in_bytes, Agc::Command::DrawInitiator(modifier, true, __func__)}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbDrawIndexIndirectMultiGetSize() {
    return 40;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexMultiInstanced(CommandBuffer* buf, uint32_t index_count, const volatile void* index_addr, const volatile void* object_ids, uint32_t instance_count, uint64_t modifier) {
 (void)buf;
 (void)index_count;
 (void)index_addr;
 (void)object_ids;
 (void)instance_count;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbDrawIndexMultiInstancedGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
