#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* sceAgcDcbSetCxRegisterDirect(CommandBuffer* buf, ShaderRegister reg){
 (void)buf;
 (void)reg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcDcbSetCxRegisterDirectGetSize(void){
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcDcbSetCxRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, uint32_t num_regs){
 (void)buf;
 (void)regs;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetShRegisterDirect(CommandBuffer* buf, ShaderRegister reg){
 (void)buf;
 (void)reg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetShRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, uint32_t num_regs){
 (void)buf;
 (void)regs;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetUcRegisterDirect(CommandBuffer* buf, ShaderRegister reg){
 (void)buf;
 (void)reg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetUcRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, uint32_t num_regs){
 (void)buf;
 (void)regs;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbContextStateOp(CommandBuffer* buf, uint32_t operation){
 (void)buf;
 (void)operation;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint64_t sceAgcDcbContextStateOpGetSize(uint32_t operation){
 (void)operation;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcDcbSetFlip(CommandBuffer* buf, uint32_t video_out_handle, int32_t display_buffer_index, uint32_t flip_mode, int64_t flip_arg){
 (void)buf;
 (void)video_out_handle;
 (void)display_buffer_index;
 (void)flip_mode;
 (void)flip_arg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetPredication(CommandBuffer* buf, uint8_t condition, uint8_t op, uint8_t wait_op, const volatile void* address, uint32_t count_in_dwords){
 (void)buf;
 (void)condition;
 (void)op;
 (void)wait_op;
 (void)address;
 (void)count_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color){
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbPopMarker(CommandBuffer* buf){
 (void)buf;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color){
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetWorkloadComplete(CommandBuffer* buf, uint32_t stream_id, uint32_t workload_id){
 (void)buf;
 (void)stream_id;
 (void)workload_id;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcDcbSetWorkloadsActive(CommandBuffer* buf, uint32_t stream_id, const uint32_t* workload_ids, uint32_t workload_count){
 (void)buf;
 (void)stream_id;
 (void)workload_ids;
 (void)workload_count;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}
}
