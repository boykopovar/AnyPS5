#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceGnmDispatchDirect(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t threadsX, std::uint32_t threadsY, std::uint32_t threadsZ, std::uint32_t flags);
int APS5_VABI sceGnmDispatchIndirect(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t flags);
int APS5_VABI sceGnmDispatchIndirectOnMec(std::uint32_t* cmdbuf, std::uint32_t size, std::uint64_t args, std::uint32_t modifier);
int APS5_VABI sceGnmSetCsShaderWithModifier(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier);
int APS5_VABI sceGnmSetPsShader350(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs);
int APS5_VABI sceGnmUpdateGsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs);
int APS5_VABI sceGnmUpdateHsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs, std::uint32_t lsHsConfig);
int APS5_VABI sceGnmUpdatePsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs);
int APS5_VABI sceGnmUpdatePsShader350(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs);
int APS5_VABI sceGnmUpdateVsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier);

}
