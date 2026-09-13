#include "prx/libSceAgc/Patch/include/IndirectRegisters.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

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

}
