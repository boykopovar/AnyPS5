#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbAcquireMem(CommandBuffer* buf, uint8_t engine, uint32_t cb_db_op, uint32_t gcr_cntl, const volatile void* base, uint64_t size_bytes, uint32_t poll_cycles) {
 (void)buf;
 (void)engine;
 (void)cb_db_op;
 (void)gcr_cntl;
 (void)base;
 (void)size_bytes;
 (void)poll_cycles;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbAcquireMemGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t num_dwords) {
 (void)buf;
 (void)address;
 (void)num_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbCondExecGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbCopyData(CommandBuffer* buf, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_immediate, uint8_t item_size, uint8_t write_confirm) {
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

uint32_t* APS5_VABI sceAgcDcbDmaData(CommandBuffer* buf, uint8_t engine, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address_or_offset, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_offset_or_immediate, uint32_t num_bytes, uint8_t wait_for_previous, uint8_t write_confirm, uint8_t block_engine) {
 (void)buf;
 (void)engine;
 (void)dst;
 (void)dst_cache_policy;
 (void)dst_address_or_offset;
 (void)src;
 (void)src_cache_policy;
 (void)src_address_or_offset_or_immediate;
 (void)num_bytes;
 (void)wait_for_previous;
 (void)write_confirm;
 (void)block_engine;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbDispatchIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t flags) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbDispatchIndirectGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbEventWrite(CommandBuffer* buf, uint8_t event_type, const volatile void* address) {
 (void)buf;
 (void)event_type;
 (void)address;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbJump(CommandBuffer* buf, uint8_t mode, uint8_t cache_policy, const uint32_t* target, uint32_t size_in_dwords) {
 (void)buf;
 (void)mode;
 (void)cache_policy;
 (void)target;
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbJumpGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, uint32_t op, uint32_t state) {
 (void)buf;
 (void)op;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbRewind(CommandBuffer* buf, uint32_t initial_state) {
 (void)buf;
 (void)initial_state;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbRewindGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbStallCommandBufferParser(CommandBuffer* buf) {
 (void)buf;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbWaitRegMem(CommandBuffer* buf, uint8_t size, uint8_t compare_function, uint8_t op, uint8_t cache_policy, const volatile void* address, uint64_t reference, uint64_t mask, uint32_t poll_cycles) {
 (void)buf;
 (void)size;
 (void)compare_function;
 (void)op;
 (void)cache_policy;
 (void)address;
 (void)reference;
 (void)mask;
 (void)poll_cycles;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbWaitOnAddressGetSize(uint32_t size) {
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer* buf, uint32_t video_out_handle, uint32_t display_buffer_index) {
 (void)buf;
 (void)video_out_handle;
 (void)display_buffer_index;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbWriteData(CommandBuffer* buf, uint8_t dst, uint8_t cache_policy, uint64_t address_or_offset, const void* data, uint32_t num_dwords, uint8_t increment, uint8_t write_confirm) {
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

uint32_t APS5_VABI sceAgcDcbWriteDataGetSize(uint32_t num_dwords) {
 (void)num_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}
std::uint64_t APS5_VABI sceAgcDcbEventWriteGetSize(std::uint8_t eventType) {
    (void)eventType;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI sceAgcDcbCopyDataGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
