#include "prx/libSceAgc/Patch/include/Dma.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

void PatchAddress(std::uint32_t* cmd, std::uint64_t address, std::size_t first, const char* function) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), 4, function);
    std::uint32_t header = 0;
    Agc::Command::Require(GuestCopyIn_nid_no_patch(&header, cmd, sizeof(header)), function, "packet is not readable");
    Agc::Command::ValidatePacket(&header, 0x50u, 7, function);
    const std::array<std::uint32_t, 2> words{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    Agc::Command::Require(GuestCopyOut_nid_no_patch(cmd + first, words.data(), sizeof(words)), function, "packet is not writable");
}

}

extern "C" {

int APS5_VABI sceAgcDmaDataPatchSetDstAddressOrOffset(std::uint32_t* cmd, std::uint64_t address) {
    PatchAddress(cmd, address, 4, __func__);
    return 0;
}

int APS5_VABI sceAgcDmaDataPatchSetSrcAddressOrOffsetOrImmediate(std::uint32_t* cmd, std::uint64_t address) {
    PatchAddress(cmd, address, 2, __func__);
    return 0;
}

}
