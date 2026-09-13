#include "prx/libSceAgc/Command/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetCxRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
 (void)buf;
 (void)reg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbSetBaseDispatchIndirectArgsGetSize() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDcbQueueEndOfShaderActionGetSize() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t APS5_VABI sceAgcDcbGetLodStatsGetSize() {
    return 20;
}

std::uint32_t APS5_VABI sceAgcDcbSetShRegistersIndirectGetSize(std::uint32_t numRegs) {
    Agc::Command::CheckBits(numRegs, 0x3fffu, __func__);
    return 20;
}

uint32_t APS5_VABI sceAgcDcbSetBaseDrawIndirectArgsGetSize() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t APS5_VABI sceAgcDcbContextStateOpGetSize(uint32_t operation) {
 (void)operation;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDcbSetCxRegisterDirectGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbSetCxRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteIndirectRegisters(buf, 0x9fu, regs, numRegs, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetShRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
 (void)buf;
 (void)reg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcDcbSetShRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteIndirectRegisters(buf, 0x63u, regs, numRegs, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetUcRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
 (void)buf;
 (void)reg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcDcbSetUcRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteIndirectRegisters(buf, 0x64u, regs, numRegs, __func__);
}

uint32_t* APS5_VABI sceAgcDcbContextStateOp(CommandBuffer* buf, uint32_t operation) {
 (void)buf;
 (void)operation;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbSetFlip(CommandBuffer* buf, uint32_t video_out_handle, int32_t display_buffer_index, uint32_t flip_mode, int64_t flip_arg) {
 (void)buf;
 (void)video_out_handle;
 (void)display_buffer_index;
 (void)flip_mode;
 (void)flip_arg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

APS5_EXPORT("23LRUSvYu1M", sceAgcInit);
int APS5_VABI sceAgcInit(uint32_t* state, uint32_t ver) {
    (void)state;
    (void)ver;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


APS5_EXPORT("qj7QZpgr9Uw", sceAgcDcbContextStateAnotherOp);
uint32_t* APS5_VABI sceAgcDcbContextStateAnotherOp(CommandBuffer* buf, uint32_t operation) {
    (void)buf;
    (void)operation;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

// unconfirmed signature
uint32_t* APS5_VABI sceAgcDcbPrimeUtcl2(CommandBuffer* buf, const volatile void* address, uint32_t size_in_bytes) {
    (void)buf;
    (void)address;
    (void)size_in_bytes;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbSetPredication(CommandBuffer* buf, uint8_t condition, uint8_t op, uint8_t wait_op, const volatile void* address, uint32_t count_in_dwords) {
 (void)buf;
 (void)condition;
 (void)op;
 (void)wait_op;
 (void)address;
 (void)count_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbPopMarker(CommandBuffer* buf) {
 (void)buf;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbSetWorkloadComplete(CommandBuffer* buf, uint32_t stream_id, uint32_t workload_id) {
 (void)buf;
 (void)stream_id;
 (void)workload_id;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbSetWorkloadsActive(CommandBuffer* buf, uint32_t stream_id, const uint32_t* workload_ids, uint32_t workload_count) {
 (void)buf;
 (void)stream_id;
 (void)workload_ids;
 (void)workload_count;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}
}
