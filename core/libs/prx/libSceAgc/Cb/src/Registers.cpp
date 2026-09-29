#include "prx/libSceAgc/Cb/include/Registers.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbSetUcRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues) {
    return Agc::Command::WriteRegisterRange(buf, 0x79u, offset, values, numValues, __func__);
}

std::uint32_t* APS5_VABI sceAgcCbSetShRegisterRangeDirect(CommandBuffer* buf, std::uint32_t offset, const std::uint32_t* values, std::uint32_t numValues) {
    return Agc::Command::WriteRegisterRange(buf, 0x76u, offset, values, numValues, __func__);
}

uint32_t APS5_VABI sceAgcCbSetShRegisterRangeDirectGetSize(uint32_t num_values) {
    return (num_values + 2u) * 4u; // header + offset + values
}

std::uint32_t APS5_VABI sceAgcCbSetUcRegisterRangeDirectGetSize(std::uint32_t numValues) {
    return (numValues + 2u) * 4u; // header + offset + values
}

std::uint32_t* APS5_VABI sceAgcCbSetShRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteRegisters(buf, 0x76u, regs, numRegs, true, __func__);
}

std::uint32_t APS5_VABI sceAgcCbSetShRegistersDirectGetSize(std::uint32_t numRegs) {
    (void)numRegs;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcCbSetUcRegistersDirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteRegisters(buf, 0x79u, regs, numRegs, false, __func__);
}

std::uint32_t APS5_VABI sceAgcCbSetUcRegistersDirectGetSize(std::uint32_t numRegs) {
    (void)numRegs;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
