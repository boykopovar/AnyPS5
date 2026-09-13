#include "prx/libSceAgc/Command/Memory.hpp"
#include "prx/libSceAgc/Command/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDmaDataPatchSetDstAddressOrOffset(std::uint32_t* cmd, std::uint64_t address) {
    Agc::Command::ValidatePacket(cmd, 0x50u, 7, __func__);
    cmd[4] = static_cast<std::uint32_t>(address);
    cmd[5] = static_cast<std::uint32_t>(address >> 32u);
    return 0;
}

int APS5_VABI sceAgcDmaDataPatchSetSrcAddressOrOffsetOrImmediate(std::uint32_t* cmd, std::uint64_t address) {
    Agc::Command::ValidatePacket(cmd, 0x50u, 7, __func__);
    cmd[2] = static_cast<std::uint32_t>(address);
    cmd[3] = static_cast<std::uint32_t>(address >> 32u);
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

std::uint32_t* APS5_VABI sceAgcSetNop(CommandBuffer* buf, std::uint32_t sizeDw) {
    return Agc::Command::WriteNop(buf, sizeDw, __func__);
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

int APS5_VABI sceAgcWaitRegMemPatchAddress(std::uint32_t* cmd, const volatile void* address) {
    auto* wait = Agc::Command::ValidateWait(cmd, __func__);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    const auto alignment = ((wait[0] >> 8u) & 0xffu) == 0x3cu ? 4u : 8u;
    Agc::Command::CheckAddress(guestAddress, alignment, __func__);
    Agc::Command::CheckBits(guestAddress, 0xffffffffffffull, __func__);
    cmd[2] = (cmd[2] & 0xffff0000u) | static_cast<std::uint32_t>(guestAddress >> 32u);
    cmd[3] = static_cast<std::uint32_t>(guestAddress);
    wait[2] = static_cast<std::uint32_t>(guestAddress);
    wait[3] = static_cast<std::uint32_t>(guestAddress >> 32u);
    return 0;
}

int APS5_VABI sceAgcWaitRegMemPatchReference(std::uint32_t* cmd, std::uint64_t reference) {
    auto* wait = Agc::Command::ValidateWait(cmd, __func__);
    Agc::Command::CheckBits(reference, 0xffffffffu, __func__);
    wait[4] = static_cast<std::uint32_t>(reference);
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

int APS5_VABI sceAgcSetCxRegIndirectPatchAddRegisters(std::uint32_t* cmd, std::uint32_t numRegs) {
    Agc::Command::PatchIndirectCount(cmd, 0x9fu, numRegs, __func__);
    return 0;
}

int APS5_VABI sceAgcSetCxRegIndirectPatchSetAddress(std::uint32_t* cmd, const volatile ShaderRegister* regs) {
    Agc::Command::PatchIndirectAddress(cmd, 0x9fu, regs, __func__);
    return 0;
}

int APS5_VABI sceAgcSetCxRegIndirectPatchSetNumRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetShRegIndirectPatchAddRegisters(std::uint32_t* cmd, std::uint32_t numRegs) {
    Agc::Command::PatchIndirectCount(cmd, 0x63u, numRegs, __func__);
    return 0;
}

int APS5_VABI sceAgcSetShRegIndirectPatchSetAddress(std::uint32_t* cmd, const volatile ShaderRegister* regs) {
    Agc::Command::PatchIndirectAddress(cmd, 0x63u, regs, __func__);
    return 0;
}

int APS5_VABI sceAgcSetShRegIndirectPatchSetNumRegisters(uint32_t* cmd, uint32_t num_regs) {
 (void)cmd;
 (void)num_regs;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetUcRegIndirectPatchAddRegisters(std::uint32_t* cmd, std::uint32_t numRegs) {
    Agc::Command::PatchIndirectCount(cmd, 0x64u, numRegs, __func__);
    return 0;
}

int APS5_VABI sceAgcSetUcRegIndirectPatchSetAddress(std::uint32_t* cmd, const volatile ShaderRegister* regs) {
    Agc::Command::PatchIndirectAddress(cmd, 0x64u, regs, __func__);
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
