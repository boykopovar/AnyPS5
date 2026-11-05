#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_INDIRECTREGISTERS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_INDIRECTREGISTERS_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcSetCxRegIndirectPatchAddRegisters(std::uint32_t* cmd, std::uint32_t numRegs);
int APS5_VABI sceAgcSetCxRegIndirectPatchSetAddress(std::uint32_t* cmd, const volatile ShaderRegister* regs);
int APS5_VABI sceAgcSetCxRegIndirectPatchSetNumRegisters(std::uint32_t* cmd, std::uint32_t numRegs);
int APS5_VABI sceAgcSetShRegIndirectPatchAddRegisters(std::uint32_t* cmd, std::uint32_t numRegs);
int APS5_VABI sceAgcSetShRegIndirectPatchSetAddress(std::uint32_t* cmd, const volatile ShaderRegister* regs);
int APS5_VABI sceAgcSetShRegIndirectPatchSetNumRegisters(std::uint32_t* cmd, std::uint32_t numRegs);
int APS5_VABI sceAgcSetUcRegIndirectPatchAddRegisters(std::uint32_t* cmd, std::uint32_t numRegs);
int APS5_VABI sceAgcSetUcRegIndirectPatchSetAddress(std::uint32_t* cmd, const volatile ShaderRegister* regs);
int APS5_VABI sceAgcSetUcRegIndirectPatchSetNumRegisters(std::uint32_t* cmd, std::uint32_t numRegs);

}

#endif
