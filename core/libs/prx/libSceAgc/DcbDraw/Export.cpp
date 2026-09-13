#include "prx/libSceAgc/Command/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbDrawIndex(CommandBuffer* buf, uint32_t index_count, const volatile void* index_addr, uint64_t modifier) {
 (void)buf;
 (void)index_count;
 (void)index_addr;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndirectMulti(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t count_indirect, uint32_t max_count_or_count, const volatile void* count_addr, uint32_t stride_in_bytes, uint64_t modifier) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)count_indirect;
 (void)max_count_or_count;
 (void)count_addr;
 (void)stride_in_bytes;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbDrawIndirectMultiGetSize() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDcbDrawIndexIndirectMultiGetSize() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDcbDrawIndexIndirectGetSize() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDcbDrawIndexGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexAuto(CommandBuffer* buf, uint32_t index_count, uint64_t modifier) {
 (void)buf;
 (void)index_count;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbDrawIndexAutoGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint64_t modifier) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirectMulti(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t count_indirect, uint32_t max_count_or_count, const volatile void* count_addr, uint32_t stride_in_bytes, uint64_t modifier) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)count_indirect;
 (void)max_count_or_count;
 (void)count_addr;
 (void)stride_in_bytes;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
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

uint32_t* APS5_VABI sceAgcDcbDrawIndexOffset(CommandBuffer* buf, uint32_t index_offset, uint32_t index_count, uint64_t modifier) {
 (void)buf;
 (void)index_offset;
 (void)index_count;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbDrawIndexOffsetGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbDrawIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint64_t modifier) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbDrawIndirectGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbSetIndexBuffer(CommandBuffer* buf, std::uint64_t indexAddress) {
    Agc::Command::CheckAddress(indexAddress, 2, __func__);
    return Agc::Command::Emit(buf, 0x26u, {static_cast<std::uint32_t>(indexAddress), static_cast<std::uint32_t>(indexAddress >> 32u)}, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetIndexCount(CommandBuffer* buf, uint32_t index_count) {
 (void)buf;
 (void)index_count;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcDcbSetIndexSize(CommandBuffer* buf, std::uint8_t indexSize, std::uint8_t cachePolicy) {
    Agc::Command::Require(indexSize <= 2, __func__, "invalid index element size");
    Agc::Command::CheckBits(cachePolicy, 3, __func__);
    return Agc::Command::Emit(buf, 0x7au, {0x20000243u, 0x400u | indexSize | (static_cast<std::uint32_t>(cachePolicy) << 6u)}, __func__);
}

std::uint32_t* APS5_VABI sceAgcDcbSetNumInstances(CommandBuffer* buf, std::uint32_t numInstances) {
    return Agc::Command::Emit(buf, 0x2fu, {numInstances}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetNumInstancesGetSize() {
    return 8;
}

uint32_t* APS5_VABI sceAgcDcbSetBaseIndirectArgs(CommandBuffer* buf, uint32_t shader_type, const volatile void* indirect_base_addr) {
 (void)buf;
 (void)shader_type;
 (void)indirect_base_addr;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcDcbGetLodStats(CommandBuffer* buf, std::uint8_t cachePolicy, const volatile void* buffer, std::uint32_t bufferSizeInBytes, std::uint32_t resetCount, std::uint8_t forceReset, std::uint8_t reportAndReset, std::uint32_t reportingIntervalIn100kClocks) {
    Agc::Command::CheckBits(cachePolicy, 3, __func__);
    Agc::Command::CheckBits(resetCount, 0xffu, __func__);
    Agc::Command::CheckBits(forceReset, 1, __func__);
    Agc::Command::CheckBits(reportAndReset, 1, __func__);
    Agc::Command::CheckBits(reportingIntervalIn100kClocks, 0xffu, __func__);
    const auto address = reinterpret_cast<std::uintptr_t>(buffer);
    Agc::Command::CheckAddress(address, 64, __func__);
    const auto control = (static_cast<std::uint32_t>(cachePolicy) << 28u) | (static_cast<std::uint32_t>(reportAndReset) << 19u) | (static_cast<std::uint32_t>(forceReset) << 18u) | (resetCount << 10u) | (reportingIntervalIn100kClocks << 2u);
    return Agc::Command::Emit(buf, 0x8eu, {bufferSizeInBytes, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), control}, __func__);
}

}
