#ifndef CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_REGISTERS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_REGISTERS_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbSetUcRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues);
std::uint32_t* APS5_VABI sceAgcCbSetShRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues);
uint32_t APS5_VABI sceAgcCbSetShRegisterRangeDirectGetSize(uint32_t numValues);
std::uint32_t APS5_VABI sceAgcCbSetUcRegisterRangeDirectGetSize(std::uint32_t numValues);
std::uint32_t* APS5_VABI sceAgcCbSetShRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs);
std::uint32_t APS5_VABI sceAgcCbSetShRegistersDirectGetSize(std::uint32_t numRegs);
std::uint32_t* APS5_VABI sceAgcCbSetUcRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs);
std::uint32_t APS5_VABI sceAgcCbSetUcRegistersDirectGetSize(std::uint32_t numRegs);

}

#endif
