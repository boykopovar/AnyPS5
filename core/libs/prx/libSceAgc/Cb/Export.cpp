#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* sceAgcCbBranch(CommandBuffer* buf, uint8_t mode, uint8_t compare_function, const volatile uint64_t* compare_addr, uint64_t mask, uint64_t reference, uint8_t cache_policy1, const volatile uint32_t* buffer1, uint32_t size_in_dwords1, uint8_t cache_policy2, const volatile uint32_t* buffer2, uint32_t size_in_dwords2){
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
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* sceAgcCbDispatch(CommandBuffer* buf, uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z, uint32_t modifier){
 (void)buf;
 (void)thread_group_x;
 (void)thread_group_y;
 (void)thread_group_z;
 (void)modifier;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcCbDispatchGetSize(void){
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcCbNop_nid_postfix(CommandBuffer* buf, uint32_t size_in_dwords){
 (void)buf;
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcCbNopGetSize(uint32_t size_in_dwords){
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t sceAgcCbQueueEndOfPipeActionGetSize(void){
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcCbReleaseMem(CommandBuffer* buf, uint8_t action, uint16_t gcr_cntl, uint8_t dst, uint8_t cache_policy, const volatile Label* address, uint8_t data_sel, uint64_t data, uint16_t gds_offset, uint16_t gds_size, uint8_t interrupt, uint32_t interrupt_ctx_id){
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

uint32_t* sceAgcCbSetShRegisterRangeDirect(CommandBuffer* buf, uint32_t offset, const uint32_t* values, uint32_t num_values){
 (void)buf;
 (void)offset;
 (void)values;
 (void)num_values;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t sceAgcCbSetShRegisterRangeDirectGetSize(uint32_t num_values){
 (void)num_values;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* sceAgcCbSetShRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, uint32_t num_regs){
 (void)buf;
 (void)regs;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}
std::uint32_t* sceAgcCbSetUcRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    (void)buf;
    (void)regs;
    (void)numRegs;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
