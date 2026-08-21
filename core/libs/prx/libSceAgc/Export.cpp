#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

uint32_t* sceAgcAcbAcquireMem(CommandBuffer* buf, uint32_t gcr_cntl, const volatile void* base, uint64_t size_bytes, uint32_t poll_cycles) {
 (void)buf;
 (void)gcr_cntl;
 (void)base;
 (void)size_bytes;
 (void)poll_cycles;
 return nullptr;
}

uint32_t sceAgcAcbAcquireMemGetSize(void) {
 return 0;
}

uint32_t* sceAgcAcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t num_dwords) {
 (void)buf;
 (void)address;
 (void)num_dwords;
 return nullptr;
}

uint32_t sceAgcAcbCondExecGetSize(void) {
 return 0;
}

uint32_t* sceAgcAcbCopyData(CommandBuffer* buf, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_immediate, uint8_t item_size, uint8_t write_confirm) {
 (void)buf;
 (void)dst;
 (void)dst_cache_policy;
 (void)dst_address;
 (void)src;
 (void)src_cache_policy;
 (void)src_address_or_immediate;
 (void)item_size;
 (void)write_confirm;
 return nullptr;
}

uint32_t* sceAgcAcbDispatchIndirect(CommandBuffer* buf, const volatile void* indirect_args, uint32_t modifier) {
 (void)buf;
 (void)indirect_args;
 (void)modifier;
 return nullptr;
}

uint32_t* sceAgcAcbDmaData(CommandBuffer* buf, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address_or_offset, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_offset_or_immediate, uint32_t num_bytes, uint8_t wait_for_previous, uint8_t write_confirm) {
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
 return nullptr;
}

uint32_t* sceAgcAcbEventWrite(CommandBuffer* buf, uint8_t event_type, const volatile void* address) {
 (void)buf;
 (void)event_type;
 (void)address;
 return nullptr;
}

uint32_t sceAgcAcbJumpGetSize(void) {
 return 0;
}

uint32_t* sceAgcAcbPopMarker(CommandBuffer* buf) {
 (void)buf;
 return nullptr;
}

uint32_t* sceAgcAcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 return nullptr;
}

uint32_t* sceAgcAcbResetQueue(CommandBuffer* buf, uint32_t op) {
 (void)buf;
 (void)op;
 return nullptr;
}

uint32_t* sceAgcAcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 return nullptr;
}

uint32_t* sceAgcAcbWaitRegMem(CommandBuffer* buf, uint8_t size, uint8_t compare_function, uint8_t cache_policy, const volatile void* address, uint64_t reference, uint64_t mask, uint32_t poll_cycles) {
 (void)buf;
 (void)size;
 (void)compare_function;
 (void)cache_policy;
 (void)address;
 (void)reference;
 (void)mask;
 (void)poll_cycles;
 return nullptr;
}

uint32_t* sceAgcAcbWriteData(CommandBuffer* buf, uint8_t dst, uint8_t cache_policy, uint64_t address_or_offset, const void* data, uint32_t num_dwords, uint8_t increment, uint8_t write_confirm) {
 (void)buf;
 (void)dst;
 (void)cache_policy;
 (void)address_or_offset;
 (void)data;
 (void)num_dwords;
 (void)increment;
 (void)write_confirm;
 return nullptr;
}

uint32_t* sceAgcCbBranch(CommandBuffer* buf, uint8_t mode, uint8_t compare_function, const volatile uint64_t* compare_addr, uint64_t mask, uint64_t reference, uint8_t cache_policy1, const volatile uint32_t* buffer1, uint32_t size_in_dwords1, uint8_t cache_policy2, const volatile uint32_t* buffer2, uint32_t size_in_dwords2) {
 (void)buf;
 (void)mode;
 (void)compare_function;
 (void)compare_addr;
 (void)mask;
 (void)reference;
 (void)cache_policy1;
 (void)buffer1;
 (void)size_in_dwords1;
 (void)cache_policy2;
 (void)buffer2;
 (void)size_in_dwords2;
 return nullptr;
}

uint32_t* sceAgcCbDispatch(CommandBuffer* buf, uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z, uint32_t modifier) {
 (void)buf;
 (void)thread_group_x;
 (void)thread_group_y;
 (void)thread_group_z;
 (void)modifier;
 return nullptr;
}

uint32_t sceAgcCbDispatchGetSize(void) {
 return 0;
}

uint32_t* sceAgcCbNop_nid_postfix(CommandBuffer* buf, uint32_t size_in_dwords) {
 (void)buf;
 (void)size_in_dwords;
 return nullptr;
}

uint32_t sceAgcCbNopGetSize(uint32_t size_in_dwords) {
 (void)size_in_dwords;
 return 0;
}

uint32_t sceAgcCbQueueEndOfPipeActionGetSize(void) {
 return 0;
}

uint32_t* sceAgcCbReleaseMem(CommandBuffer* buf, uint8_t action, uint16_t gcr_cntl, uint8_t dst, uint8_t cache_policy, const volatile Label* address, uint8_t data_sel, uint64_t data, uint16_t gds_offset, uint16_t gds_size, uint8_t interrupt, uint32_t interrupt_ctx_id) {
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
 return nullptr;
}

uint32_t* sceAgcCbSetShRegisterRangeDirect(CommandBuffer* buf, uint32_t offset, const uint32_t* values, uint32_t num_values) {
 (void)buf;
 (void)offset;
 (void)values;
 (void)num_values;
 return nullptr;
}

uint32_t sceAgcCbSetShRegisterRangeDirectGetSize(uint32_t num_values) {
 (void)num_values;
 return 0;
}

uint32_t* sceAgcCbSetShRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, uint32_t num_regs) {
 (void)buf;
 (void)regs;
 (void)num_regs;
 return nullptr;
}

int sceAgcCondExecPatchSetCommandAddress(uint32_t* cmd, const volatile uint32_t* command) {
 (void)cmd;
 (void)command;
 return 0;
}

int sceAgcCondExecPatchSetEnd(uint32_t* cmd, const volatile uint32_t* buffer) {
 (void)cmd;
 (void)buffer;
 return 0;
}

int sceAgcCreateInterpolantMapping(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
 (void)regs;
 (void)gs;
 (void)ps;
 return 0;
}

int sceAgcCreatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, const Shader* hs, const Shader* gs, uint32_t prim_type) {
 (void)cx_regs;
 (void)uc_regs;
 (void)hs;
 (void)gs;
 (void)prim_type;
 return 0;
}

int sceAgcCreateShader(Shader** dst, void* header, const volatile void* code) {
 (void)dst;
 (void)header;
 (void)code;
 return 0;
}

uint32_t* sceAgcDcbAcquireMem(CommandBuffer* buf, uint8_t engine, uint32_t cb_db_op, uint32_t gcr_cntl, const volatile void* base, uint64_t size_bytes, uint32_t poll_cycles) {
 (void)buf;
 (void)engine;
 (void)cb_db_op;
 (void)gcr_cntl;
 (void)base;
 (void)size_bytes;
 (void)poll_cycles;
 return nullptr;
}

uint32_t sceAgcDcbAcquireMemGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t num_dwords) {
 (void)buf;
 (void)address;
 (void)num_dwords;
 return nullptr;
}

uint32_t sceAgcDcbCondExecGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbContextStateOp(CommandBuffer* buf, uint32_t operation) {
 (void)buf;
 (void)operation;
 return nullptr;
}

uint64_t sceAgcDcbContextStateOpGetSize(uint32_t operation) {
 (void)operation;
 return 0;
}

uint32_t* sceAgcDcbCopyData(CommandBuffer* buf, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_immediate, uint8_t item_size, uint8_t write_confirm) {
 (void)buf;
 (void)dst;
 (void)dst_cache_policy;
 (void)dst_address;
 (void)src;
 (void)src_cache_policy;
 (void)src_address_or_immediate;
 (void)item_size;
 (void)write_confirm;
 return nullptr;
}

uint32_t* sceAgcDcbDispatchIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t flags) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)flags;
 return nullptr;
}

uint32_t sceAgcDcbDispatchIndirectGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbDmaData(CommandBuffer* buf, uint8_t engine, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address_or_offset, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_offset_or_immediate, uint32_t num_bytes, uint8_t wait_for_previous, uint8_t write_confirm, uint8_t block_engine) {
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
 return nullptr;
}

uint32_t* sceAgcDcbDrawIndex(CommandBuffer* buf, uint32_t index_count, const volatile void* index_addr, uint64_t modifier) {
 (void)buf;
 (void)index_count;
 (void)index_addr;
 (void)modifier;
 return nullptr;
}

uint32_t* sceAgcDcbDrawIndexAuto(CommandBuffer* buf, uint32_t index_count, uint64_t modifier) {
 (void)buf;
 (void)index_count;
 (void)modifier;
 return nullptr;
}

uint32_t sceAgcDcbDrawIndexAutoGetSize(void) {
 return 0;
}

uint32_t sceAgcDcbDrawIndexGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbDrawIndexIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint64_t modifier) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)modifier;
 return nullptr;
}

uint32_t* sceAgcDcbDrawIndexIndirectMulti(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t count_indirect, uint32_t max_count_or_count, const volatile void* count_addr, uint32_t stride_in_bytes, uint64_t modifier) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)count_indirect;
 (void)max_count_or_count;
 (void)count_addr;
 (void)stride_in_bytes;
 (void)modifier;
 return nullptr;
}

uint32_t* sceAgcDcbDrawIndexMultiInstanced(CommandBuffer* buf, uint32_t index_count, const volatile void* index_addr, const volatile void* object_ids, uint32_t instance_count, uint64_t modifier) {
 (void)buf;
 (void)index_count;
 (void)index_addr;
 (void)object_ids;
 (void)instance_count;
 (void)modifier;
 return nullptr;
}

uint32_t sceAgcDcbDrawIndexMultiInstancedGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbDrawIndexOffset(CommandBuffer* buf, uint32_t index_offset, uint32_t index_count, uint64_t modifier) {
 (void)buf;
 (void)index_offset;
 (void)index_count;
 (void)modifier;
 return nullptr;
}

uint32_t sceAgcDcbDrawIndexOffsetGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbDrawIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint64_t modifier) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)modifier;
 return nullptr;
}

uint32_t sceAgcDcbDrawIndirectGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbEventWrite(CommandBuffer* buf, uint8_t event_type, const volatile void* address) {
 (void)buf;
 (void)event_type;
 (void)address;
 return nullptr;
}

uint32_t* sceAgcDcbGetLodStats(CommandBuffer* buf, uint8_t cache_policy, const volatile void* buffer, uint32_t buffer_size_in_bytes, uint32_t reset_count, uint8_t force_reset, uint8_t report_and_reset, uint32_t reporting_interval_in_100k_clocks) {
 (void)buf;
 (void)cache_policy;
 (void)buffer;
 (void)buffer_size_in_bytes;
 (void)reset_count;
 (void)force_reset;
 (void)report_and_reset;
 (void)reporting_interval_in_100k_clocks;
 return nullptr;
}

uint32_t* sceAgcDcbJump(CommandBuffer* buf, uint8_t mode, uint8_t cache_policy, const uint32_t* target, uint32_t size_in_dwords) {
 (void)buf;
 (void)mode;
 (void)cache_policy;
 (void)target;
 (void)size_in_dwords;
 return nullptr;
}

uint32_t sceAgcDcbJumpGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbPopMarker(CommandBuffer* buf) {
 (void)buf;
 return nullptr;
}

uint32_t* sceAgcDcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 return nullptr;
}

uint32_t* sceAgcDcbResetQueue(CommandBuffer* buf, uint32_t op, uint32_t state) {
 (void)buf;
 (void)op;
 (void)state;
 return nullptr;
}

uint32_t* sceAgcDcbRewind(CommandBuffer* buf, uint32_t initial_state) {
 (void)buf;
 (void)initial_state;
 return nullptr;
}

uint32_t sceAgcDcbRewindGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbSetBaseIndirectArgs(CommandBuffer* buf, uint32_t shader_type, const volatile void* indirect_base_addr) {
 (void)buf;
 (void)shader_type;
 (void)indirect_base_addr;
 return nullptr;
}

uint32_t* sceAgcDcbSetCxRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
 (void)buf;
 (void)reg;
 return nullptr;
}

uint32_t sceAgcDcbSetCxRegisterDirectGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbSetCxRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, uint32_t num_regs) {
 (void)buf;
 (void)regs;
 (void)num_regs;
 return nullptr;
}

uint32_t* sceAgcDcbSetFlip(CommandBuffer* buf, uint32_t video_out_handle, int32_t display_buffer_index, uint32_t flip_mode, int64_t flip_arg) {
 (void)buf;
 (void)video_out_handle;
 (void)display_buffer_index;
 (void)flip_mode;
 (void)flip_arg;
 return nullptr;
}

uint32_t* sceAgcDcbSetIndexBuffer(CommandBuffer* buf, uint64_t index_addr) {
 (void)buf;
 (void)index_addr;
 return nullptr;
}

uint32_t* sceAgcDcbSetIndexCount(CommandBuffer* buf, uint32_t index_count) {
 (void)buf;
 (void)index_count;
 return nullptr;
}

uint32_t* sceAgcDcbSetIndexSize(CommandBuffer* buf, uint8_t index_size, uint8_t cache_policy) {
 (void)buf;
 (void)index_size;
 (void)cache_policy;
 return nullptr;
}

uint32_t* sceAgcDcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 return nullptr;
}

uint32_t* sceAgcDcbSetNumInstances(CommandBuffer* buf, uint32_t num_instances) {
 (void)buf;
 (void)num_instances;
 return nullptr;
}

uint32_t sceAgcDcbSetNumInstancesGetSize(void) {
 return 0;
}

uint32_t* sceAgcDcbSetPredication(CommandBuffer* buf, uint8_t condition, uint8_t op, uint8_t wait_op, const volatile void* address, uint32_t count_in_dwords) {
 (void)buf;
 (void)condition;
 (void)op;
 (void)wait_op;
 (void)address;
 (void)count_in_dwords;
 return nullptr;
}

uint32_t* sceAgcDcbSetShRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
 (void)buf;
 (void)reg;
 return nullptr;
}

uint32_t* sceAgcDcbSetShRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, uint32_t num_regs) {
 (void)buf;
 (void)regs;
 (void)num_regs;
 return nullptr;
}

uint32_t* sceAgcDcbSetUcRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
 (void)buf;
 (void)reg;
 return nullptr;
}

uint32_t* sceAgcDcbSetUcRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, uint32_t num_regs) {
 (void)buf;
 (void)regs;
 (void)num_regs;
 return nullptr;
}

uint32_t* sceAgcDcbSetWorkloadComplete(CommandBuffer* buf, uint32_t stream_id, uint32_t workload_id) {
 (void)buf;
 (void)stream_id;
 (void)workload_id;
 return nullptr;
}

uint32_t* sceAgcDcbSetWorkloadsActive(CommandBuffer* buf, uint32_t stream_id, const uint32_t* workload_ids, uint32_t workload_count) {
 (void)buf;
 (void)stream_id;
 (void)workload_ids;
 (void)workload_count;
 return nullptr;
}

uint32_t* sceAgcDcbStallCommandBufferParser(CommandBuffer* buf) {
 (void)buf;
 return nullptr;
}

uint32_t sceAgcDcbWaitOnAddressGetSize(uint32_t size) {
 (void)size;
 return 0;
}

uint32_t* sceAgcDcbWaitRegMem(CommandBuffer* buf, uint8_t size, uint8_t compare_function, uint8_t op, uint8_t cache_policy, const volatile void* address, uint64_t reference, uint64_t mask, uint32_t poll_cycles) {
 (void)buf;
 (void)size;
 (void)compare_function;
 (void)op;
 (void)cache_policy;
 (void)address;
 (void)reference;
 (void)mask;
 (void)poll_cycles;
 return nullptr;
}

uint32_t* sceAgcDcbWaitUntilSafeForRendering(CommandBuffer* buf, uint32_t video_out_handle, uint32_t display_buffer_index) {
 (void)buf;
 (void)video_out_handle;
 (void)display_buffer_index;
 return nullptr;
}

uint32_t* sceAgcDcbWriteData(CommandBuffer* buf, uint8_t dst, uint8_t cache_policy, uint64_t address_or_offset, const void* data, uint32_t num_dwords, uint8_t increment, uint8_t write_confirm) {
 (void)buf;
 (void)dst;
 (void)cache_policy;
 (void)address_or_offset;
 (void)data;
 (void)num_dwords;
 (void)increment;
 (void)write_confirm;
 return nullptr;
}

uint32_t sceAgcDcbWriteDataGetSize(uint32_t num_dwords) {
 (void)num_dwords;
 return 0;
}

int sceAgcDmaDataPatchSetDstAddressOrOffset(uint32_t* cmd, uint64_t dst_address_or_offset) {
 (void)cmd;
 (void)dst_address_or_offset;
 return 0;
}

int sceAgcDmaDataPatchSetSrcAddressOrOffsetOrImmediate(uint32_t* cmd, uint64_t src_address_or_offset_or_immediate) {
 (void)cmd;
 (void)src_address_or_offset_or_immediate;
 return 0;
}

int sceAgcGetDataPacketPayloadAddress(uint32_t** addr, uint32_t* cmd, int type) {
 (void)addr;
 (void)cmd;
 (void)type;
 return 0;
}

uint32_t sceAgcGetPacketSize(uint32_t* packet) {
 (void)packet;
 return 0;
}

void* sceAgcGetRegisterDefaults2(uint32_t ver) {
 (void)ver;
 return nullptr;
}

void* sceAgcGetRegisterDefaults2Internal(uint32_t ver) {
 (void)ver;
 return nullptr;
}

int sceAgcInit_nid_postfix(uint32_t* state, uint32_t ver) {
 (void)state;
 (void)ver;
 return 0;
}

int sceAgcJumpPatchSetTarget(uint32_t* cmd, const volatile uint32_t* target, uint32_t size_in_dwords) {
 (void)cmd;
 (void)target;
 (void)size_in_dwords;
 return 0;
}

int sceAgcQueueEndOfPipeActionPatchAddress(uint32_t* cmd, const volatile Label* address) {
 (void)cmd;
 (void)address;
 return 0;
}

int sceAgcQueueEndOfPipeActionPatchData(uint32_t* cmd, uint32_t context_id, uint32_t data_sel, uint64_t data) {
 (void)cmd;
 (void)context_id;
 (void)data_sel;
 (void)data;
 return 0;
}

int sceAgcRewindPatchSetRewindState(uint32_t* cmd, uint8_t state) {
 (void)cmd;
 (void)state;
 return 0;
}

int sceAgcSetCxRegIndirectPatchAddRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 return 0;
}

int sceAgcSetCxRegIndirectPatchSetAddress(uint32_t* cmd, const volatile ShaderRegister* regs) {
 (void)cmd;
 (void)regs;
 return 0;
}

int sceAgcSetCxRegIndirectPatchSetNumRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 return 0;
}

int sceAgcSetPacketPredication(uint32_t* packet, uint32_t predication) {
 (void)packet;
 (void)predication;
 return 0;
}

int sceAgcSetRangePredication(uint32_t* start, const volatile uint32_t* end, uint32_t predication) {
 (void)start;
 (void)end;
 (void)predication;
 return 0;
}

int sceAgcSetShRegIndirectPatchAddRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 return 0;
}

int sceAgcSetShRegIndirectPatchSetAddress(uint32_t* cmd, const volatile ShaderRegister* regs) {
 (void)cmd;
 (void)regs;
 return 0;
}

int sceAgcSetShRegIndirectPatchSetNumRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 return 0;
}

int sceAgcSetUcRegIndirectPatchAddRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 return 0;
}

int sceAgcSetUcRegIndirectPatchSetAddress(uint32_t* cmd, const volatile ShaderRegister* regs) {
 (void)cmd;
 (void)regs;
 return 0;
}

int sceAgcSetUcRegIndirectPatchSetNumRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 return 0;
}

int sceAgcSuspendPoint(void) {
 return 0;
}

int sceAgcUpdatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, uint32_t prim_type) {
 (void)cx_regs;
 (void)uc_regs;
 (void)prim_type;
 return 0;
}

int sceAgcWaitRegMemPatchAddress(uint32_t* cmd, const volatile void* address) {
 (void)cmd;
 (void)address;
 return 0;
}

int sceAgcWaitRegMemPatchReference(uint32_t* cmd, uint64_t reference) {
 (void)cmd;
 (void)reference;
 return 0;
}

}
