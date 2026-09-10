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

uint32_t* APS5_VABI sceAgcAcbDmaData(CommandBuffer* buf, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address_or_offset, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_offset_or_immediate, uint32_t num_bytes, uint8_t wait_for_previous, uint8_t write_confirm) {
 (void)buf;
 (void)dst;
 (void)dst_cache_policy;
 (void)dst_address_or_offset;
 (void)src;
 (void)src_cache_policy;
 (void)src_address_or_offset_or_immediate;
 (void)num_bytes;
 (void)wait_for_previous;
 (void)write_confirm;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbEventWrite(CommandBuffer* buf, uint8_t event_type, const volatile void* address) {
 (void)buf;
 (void)event_type;
 (void)address;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
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

uint32_t* APS5_VABI sceAgcAcbWaitRegMem(CommandBuffer* buf, uint8_t size, uint8_t compare_function, uint8_t cache_policy, const volatile void* address, uint64_t reference, uint64_t mask, uint32_t poll_cycles) {
 (void)buf;
 (void)size;
 (void)compare_function;
 (void)cache_policy;
 (void)address;
 (void)reference;
 (void)mask;
 (void)poll_cycles;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbWriteData(CommandBuffer* buf, uint8_t dst, uint8_t cache_policy, uint64_t address_or_offset, const void* data, uint32_t num_dwords, uint8_t increment, uint8_t write_confirm) {
 (void)buf;
 (void)dst;
 (void)cache_policy;
 (void)address_or_offset;
 (void)data;
 (void)num_dwords;
 (void)increment;
 (void)write_confirm;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
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
