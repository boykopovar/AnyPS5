#include "prx/libSceAgc/Command/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbBranch(CommandBuffer* buf, std::uint8_t mode, std::uint8_t compareFunction, const volatile std::uint64_t* compareAddress, std::uint64_t mask, std::uint64_t reference, std::uint8_t cachePolicy1, const volatile std::uint32_t* buffer1, std::uint32_t sizeInDwords1, std::uint8_t cachePolicy2, const volatile std::uint32_t* buffer2, std::uint32_t sizeInDwords2) {
    Agc::Command::CheckBits(mode, 3, __func__);
    Agc::Command::CheckBits(compareFunction, 7, __func__);
    Agc::Command::CheckBits(cachePolicy1, 3, __func__);
    Agc::Command::CheckBits(cachePolicy2, 3, __func__);
    Agc::Command::CheckBits(sizeInDwords1, 0xfffffu, __func__);
    Agc::Command::CheckBits(sizeInDwords2, 0xfffffu, __func__);
    const auto address = reinterpret_cast<std::uintptr_t>(compareAddress);
    const auto first = reinterpret_cast<std::uintptr_t>(buffer1);
    const auto second = reinterpret_cast<std::uintptr_t>(buffer2);
    Agc::Command::CheckAddress(address, 8, __func__);
    Agc::Command::CheckAddress(first, 4, __func__);
    Agc::Command::CheckAddress(second, 4, __func__);
    return Agc::Command::Emit(buf, 0x3fu, {mode | (static_cast<std::uint32_t>(compareFunction) << 8u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), static_cast<std::uint32_t>(mask), static_cast<std::uint32_t>(mask >> 32u), static_cast<std::uint32_t>(reference), static_cast<std::uint32_t>(reference >> 32u), static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(first >> 32u), sizeInDwords1 | (static_cast<std::uint32_t>(cachePolicy1) << 28u), static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32u), sizeInDwords2 | (static_cast<std::uint32_t>(cachePolicy2) << 28u)}, __func__);
}

std::uint32_t* APS5_VABI sceAgcCbSetUcRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues) {
    return Agc::Command::WriteRegisterRange(buf, 0x79u, offset, values, numValues, __func__);
}

uint32_t* APS5_VABI sceAgcCbCondWrite(CommandBuffer* buf, uint32_t cond, uint32_t a, uint32_t b, uint32_t c) {
    (void)buf;
    (void)cond;
    (void)a;
    (void)b;
    (void)c;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

std::uint32_t APS5_VABI sceAgcCbBranchGetSize() {
    return 56;
}

std::uint32_t* APS5_VABI sceAgcCbDispatch(CommandBuffer* buf, std::uint32_t threadGroupX, std::uint32_t threadGroupY, std::uint32_t threadGroupZ, std::uint32_t modifier) {
    Agc::Command::CheckBits(modifier, 0xa038u, __func__);
    return Agc::Command::Emit(buf, 0x15u, {threadGroupX, threadGroupY, threadGroupZ, modifier | 0x41u}, __func__);
}

uint32_t APS5_VABI sceAgcCbDispatchGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcCbNop_nid_postfix(CommandBuffer* buf, std::uint32_t sizeInDwords) {
    return Agc::Command::WriteNop(buf, sizeInDwords, __func__);
}

uint32_t APS5_VABI sceAgcCbNopGetSize(uint32_t size_in_dwords) {
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcCbQueueEndOfPipeActionGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcCbReleaseMem(CommandBuffer* buf, uint8_t action, uint16_t gcr_cntl, uint8_t dst, uint8_t cache_policy, const volatile Label* address, uint8_t data_sel, uint64_t data, uint16_t gds_offset, uint16_t gds_size, uint8_t interrupt, uint32_t interrupt_ctx_id) {
 (void)buf;
 (void)action;
 (void)gcr_cntl;
 (void)dst;
 (void)cache_policy;
 (void)address;
 (void)data_sel;
 (void)data;
 (void)gds_offset;
 (void)gds_size;
 (void)interrupt;
 (void)interrupt_ctx_id;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcCbSetShRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues) {
    return Agc::Command::WriteRegisterRange(buf, 0x76u, offset, values, numValues, __func__);
}

uint32_t APS5_VABI sceAgcCbSetShRegisterRangeDirectGetSize(uint32_t num_values) {
 (void)num_values;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcCbSetShRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteRegisters(buf, 0x76u, regs, numRegs, true, __func__);
}
std::uint32_t* APS5_VABI sceAgcCbSetUcRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteRegisters(buf, 0x79u, regs, numRegs, false, __func__);
}

}
