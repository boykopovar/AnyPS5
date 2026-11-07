#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_REGISTERS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_REGISTERS_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetCfRegisterDirect(CommandBuffer* buf, ShaderRegister reg);
std::uint32_t* APS5_VABI sceAgcDcbSetCfRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues);
uint32_t* APS5_VABI sceAgcDcbSetCxRegisterDirect(CommandBuffer* buf, ShaderRegister reg);
uint32_t APS5_VABI sceAgcDcbSetCxRegisterDirectGetSize(void);
std::uint32_t* APS5_VABI sceAgcDcbSetCxRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs);
std::uint32_t APS5_VABI sceAgcDcbSetCxRegistersIndirectGetSize(std::uint32_t numRegs);
uint32_t* APS5_VABI sceAgcDcbSetShRegisterDirect(CommandBuffer* buf, ShaderRegister reg);
std::uint32_t APS5_VABI sceAgcDcbSetShRegisterDirectGetSize();
std::uint32_t* APS5_VABI sceAgcDcbSetShRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs);
std::uint32_t APS5_VABI sceAgcDcbSetShRegistersIndirectGetSize(std::uint32_t numRegs);
uint32_t* APS5_VABI sceAgcDcbSetUcRegisterDirect(CommandBuffer* buf, ShaderRegister reg);
std::uint32_t APS5_VABI sceAgcDcbSetUcRegisterDirectGetSize();
std::uint32_t* APS5_VABI sceAgcDcbSetUcRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs);
std::uint32_t APS5_VABI sceAgcDcbSetUcRegistersIndirectGetSize(std::uint32_t numRegs);

}

#endif
