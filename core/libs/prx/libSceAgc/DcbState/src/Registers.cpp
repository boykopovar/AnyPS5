#include "prx/libSceAgc/DcbState/include/Registers.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetCxRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
 (void)buf;
 (void)reg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbSetCxRegisterDirectGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbSetCxRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteIndirectRegisters(buf, 0x9fu, regs, numRegs, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetShRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
 (void)buf;
 (void)reg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcDcbSetShRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteIndirectRegisters(buf, 0x63u, regs, numRegs, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetShRegistersIndirectGetSize(std::uint32_t numRegs) {
    Agc::Command::CheckBits(numRegs, 0x3fffu, __func__);
    return 20;
}

uint32_t* APS5_VABI sceAgcDcbSetUcRegisterDirect(CommandBuffer* buf, ShaderRegister reg) {
 (void)buf;
 (void)reg;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcDcbSetUcRegistersIndirect(CommandBuffer* buf, const volatile ShaderRegister* regs, std::uint32_t numRegs) {
    return Agc::Command::WriteIndirectRegisters(buf, 0x64u, regs, numRegs, __func__);
}

}
