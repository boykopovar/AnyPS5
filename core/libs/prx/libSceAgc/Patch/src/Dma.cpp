#include "prx/libSceAgc/Patch/include/Dma.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
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

}
