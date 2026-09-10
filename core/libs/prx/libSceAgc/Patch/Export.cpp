#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDmaDataPatchSetDstAddressOrOffset(uint32_t* cmd, uint64_t dst_address_or_offset) {
 (void)cmd;
 (void)dst_address_or_offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDmaDataPatchSetSrcAddressOrOffsetOrImmediate(uint32_t* cmd, uint64_t src_address_or_offset_or_immediate) {
 (void)cmd;
 (void)src_address_or_offset_or_immediate;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcJumpPatchSetTarget(uint32_t* cmd, const volatile uint32_t* target, uint32_t size_in_dwords) {
 (void)cmd;
 (void)target;
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchAddress(uint32_t* cmd, const volatile Label* address) {
 (void)cmd;
 (void)address;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcSetNop(CommandBuffer* buf, uint32_t size_dw) {
 (void)buf;
 (void)size_dw;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchData(uint32_t* cmd, uint32_t context_id, uint32_t data_sel, uint64_t data) {
 (void)cmd;
 (void)context_id;
 (void)data_sel;
 (void)data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcRewindPatchSetRewindState(uint32_t* cmd, uint8_t state) {
 (void)cmd;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcWaitRegMemPatchAddress(uint32_t* cmd, const volatile void* address) {
 (void)cmd;
 (void)address;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcWaitRegMemPatchReference(uint32_t* cmd, uint64_t reference) {
 (void)cmd;
 (void)reference;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcCondExecPatchSetCommandAddress(uint32_t* cmd, const volatile uint32_t* command) {
 (void)cmd;
 (void)command;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcCondExecPatchSetEnd(uint32_t* cmd, const volatile uint32_t* buffer) {
 (void)cmd;
 (void)buffer;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetCxRegIndirectPatchAddRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetCxRegIndirectPatchSetAddress(uint32_t* cmd, const volatile ShaderRegister* regs) {
 (void)cmd;
 (void)regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetCxRegIndirectPatchSetNumRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetShRegIndirectPatchAddRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetShRegIndirectPatchSetAddress(uint32_t* cmd, const volatile ShaderRegister* regs) {
 (void)cmd;
 (void)regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetShRegIndirectPatchSetNumRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetUcRegIndirectPatchAddRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetUcRegIndirectPatchSetAddress(uint32_t* cmd, const volatile ShaderRegister* regs) {
 (void)cmd;
 (void)regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetUcRegIndirectPatchSetNumRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}
int APS5_VABI sceAgcAsyncCondExecPatchSetCommandAddress(std::uint32_t* cmd, const volatile std::uint32_t* command) {
    (void)cmd;
    (void)command;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcAsyncCondExecPatchSetEnd(std::uint32_t* cmd, const volatile std::uint32_t* buffer) {
    (void)cmd;
    (void)buffer;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
