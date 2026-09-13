#include "prx/libSceAgc/Command/Memory.hpp"
#include "prx/libSceAgc/Command/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcAcbAcquireMem(CommandBuffer* buf, uint32_t gcr_cntl, const volatile void* base, uint64_t size_bytes, uint32_t poll_cycles) {
 (void)buf;
 (void)gcr_cntl;
 (void)base;
 (void)size_bytes;
 (void)poll_cycles;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcAcbAcquireMemGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcAcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t num_dwords) {
 (void)buf;
 (void)address;
 (void)num_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcAcbCondExecGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcAcbCopyData(CommandBuffer* buf, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_immediate, uint8_t item_size, uint8_t write_confirm) {
 (void)buf;
 (void)dst;
 (void)dst_cache_policy;
 (void)dst_address;
 (void)src;
 (void)src_cache_policy;
 (void)src_address_or_immediate;
 (void)item_size;
 (void)write_confirm;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbDispatchIndirect(CommandBuffer* buf, const volatile void* indirect_args, uint32_t modifier) {
 (void)buf;
 (void)indirect_args;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcAcbDmaData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t dstCachePolicy, std::uint64_t dstAddress, std::uint8_t src, std::uint8_t srcCachePolicy, std::uint64_t srcAddress, std::uint32_t numBytes, std::uint8_t waitForPrevious, std::uint8_t writeConfirm) {
    return Agc::Command::WriteDma(buf, true, 0, dst, dstCachePolicy, dstAddress, src, srcCachePolicy, srcAddress, numBytes, waitForPrevious, writeConfirm, 0, __func__);
}

std::uint32_t* APS5_VABI sceAgcAcbEventWrite(CommandBuffer* buf, std::uint8_t eventType, const volatile void* address) {
    Agc::Command::CheckBits(eventType, 0x3fu, __func__);
    Agc::Command::Require(address == nullptr, __func__, "ACB event address is not supported");
    return Agc::Command::Emit(buf, 0x46u, {eventType | (eventType == 7 ? 0x400u : 0u)}, __func__);
}

uint32_t APS5_VABI sceAgcAcbJumpGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcAcbPopMarker(CommandBuffer* buf) {
 (void)buf;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbResetQueue(CommandBuffer* buf, uint32_t op) {
 (void)buf;
 (void)op;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcAcbWaitRegMem(CommandBuffer* buf, std::uint8_t size, std::uint8_t compareFunction, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles) {
    return Agc::Command::WriteWait(buf, size, compareFunction, 0, cachePolicy, address, reference, mask, pollCycles, __func__);
}

std::uint32_t* APS5_VABI sceAgcAcbWriteData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t numDwords, std::uint8_t increment, std::uint8_t writeConfirm) {
    return Agc::Command::WriteData(buf, true, dst, cachePolicy, address, data, numDwords, increment, writeConfirm, __func__);
}
std::uint64_t APS5_VABI sceAgcAcbCopyDataGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcAcbJump(CommandBuffer* buf, std::uint8_t cachePolicy, const std::uint32_t* target, std::uint32_t sizeInDwords) {
    (void)buf;
    (void)cachePolicy;
    (void)target;
    (void)sizeInDwords;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI sceAgcAcbWaitOnAddressGetSize(std::uint8_t size) {
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
