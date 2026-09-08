#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* sceAgcDcbDrawIndex(CommandBuffer* buf, uint32_t index_count, const volatile void* index_addr, uint64_t modifier){
 (void)buf;
 (void)index_count;
 (void)index_addr;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcDcbDrawIndexGetSize(void){
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcDcbDrawIndexAuto(CommandBuffer* buf, uint32_t index_count, uint64_t modifier){
 (void)buf;
 (void)index_count;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcDcbDrawIndexAutoGetSize(void){
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcDcbDrawIndexIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint64_t modifier){
 (void)buf;
 (void)data_offset_in_bytes;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbDrawIndexIndirectMulti(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t count_indirect, uint32_t max_count_or_count, const volatile void* count_addr, uint32_t stride_in_bytes, uint64_t modifier){
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

uint32_t* sceAgcDcbDrawIndexMultiInstanced(CommandBuffer* buf, uint32_t index_count, const volatile void* index_addr, const volatile void* object_ids, uint32_t instance_count, uint64_t modifier){
 (void)buf;
 (void)index_count;
 (void)index_addr;
 (void)object_ids;
 (void)instance_count;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcDcbDrawIndexMultiInstancedGetSize(void){
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcDcbDrawIndexOffset(CommandBuffer* buf, uint32_t index_offset, uint32_t index_count, uint64_t modifier){
 (void)buf;
 (void)index_offset;
 (void)index_count;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcDcbDrawIndexOffsetGetSize(void){
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcDcbDrawIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint64_t modifier){
 (void)buf;
 (void)data_offset_in_bytes;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcDcbDrawIndirectGetSize(void){
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcDcbSetIndexBuffer(CommandBuffer* buf, uint64_t index_addr){
 (void)buf;
 (void)index_addr;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetIndexCount(CommandBuffer* buf, uint32_t index_count){
 (void)buf;
 (void)index_count;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetIndexSize(CommandBuffer* buf, uint8_t index_size, uint8_t cache_policy){
 (void)buf;
 (void)index_size;
 (void)cache_policy;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetNumInstances(CommandBuffer* buf, uint32_t num_instances){
 (void)buf;
 (void)num_instances;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcDcbSetNumInstancesGetSize(void){
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcDcbSetBaseIndirectArgs(CommandBuffer* buf, uint32_t shader_type, const volatile void* indirect_base_addr){
 (void)buf;
 (void)shader_type;
 (void)indirect_base_addr;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbGetLodStats(CommandBuffer* buf, uint8_t cache_policy, const volatile void* buffer, uint32_t buffer_size_in_bytes, uint32_t reset_count, uint8_t force_reset, uint8_t report_and_reset, uint32_t reporting_interval_in_100k_clocks){
 (void)buf;
 (void)cache_policy;
 (void)buffer;
 (void)buffer_size_in_bytes;
 (void)reset_count;
 (void)force_reset;
 (void)report_and_reset;
 (void)reporting_interval_in_100k_clocks;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}
}
