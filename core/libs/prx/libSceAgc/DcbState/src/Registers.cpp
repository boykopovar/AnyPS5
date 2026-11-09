#include "prx/libSceAgc/DcbState/include/Registers.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetCfRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
    return Agc::Command::WriteRegisterRange(buf, 0x68u, reg.offset, &reg.value, 1u, __func__);
}

std::uint32_t* APS5_VABI sceAgcDcbSetCfRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues) {
    return Agc::Command::WriteRegisterRange(buf, 0x68u, offset, values, numValues, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetCxRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
    return Agc::Command::WriteRegisterRange(buf, 0x69u, reg.offset, &reg.value, 1u, __func__);
}

uint32_t APS5_VABI sceAgcDcbSetCxRegisterDirectGetSize(void) {
    return 12;
}

std::uint32_t* APS5_VABI sceAgcDcbSetCxRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteIndirectRegisters(buf, 0x9fu, regs, numRegs, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetCxRegistersIndirectGetSize(std::uint32_t numRegs) {
    static_cast<void>(numRegs);
    return 20;
}

uint32_t* APS5_VABI sceAgcDcbSetShRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
    return Agc::Command::WriteRegisterRange(buf, 0x76u, reg.offset, &reg.value, 1u, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetShRegisterDirectGetSize() {
    return 12;
}

std::uint32_t* APS5_VABI sceAgcDcbSetShRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteIndirectRegisters(buf, 0x63u, regs, numRegs, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetShRegistersIndirectGetSize(std::uint32_t numRegs) {
    static_cast<void>(numRegs);
    return 20;
}

uint32_t* APS5_VABI sceAgcDcbSetUcRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
    return Agc::Command::WriteRegisterRange(buf, 0x79u, reg.offset, &reg.value, 1u, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetUcRegisterDirectGetSize() {
    return 12;
}

std::uint32_t* APS5_VABI sceAgcDcbSetUcRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteIndirectRegisters(buf, 0x64u, regs, numRegs, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetUcRegistersIndirectGetSize(std::uint32_t numRegs) {
    static_cast<void>(numRegs);
    return 20;
}

}
