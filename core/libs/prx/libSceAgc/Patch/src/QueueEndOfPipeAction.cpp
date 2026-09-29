#include "prx/libSceAgc/Patch/include/QueueEndOfPipeAction.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcQueueEndOfPipeActionPatchAddress(std::uint32_t* cmd, const volatile Label* address) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), 4, __func__);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    const auto opcode = (cmd[0] >> 8u) & 0xffu;
    if (opcode == 0x49u) {
        Agc::Command::ValidatePacket(cmd, opcode, 8, __func__);
        const auto dataSelect = cmd[2] >> 29u;
        Agc::Command::CheckAddress(guestAddress, dataSelect == 2 || dataSelect == 3 ? 8 : 4, __func__);
        cmd[3] = static_cast<std::uint32_t>(guestAddress);
        cmd[4] = static_cast<std::uint32_t>(guestAddress >> 32u);
    } else {
        Agc::Command::ValidatePacket(cmd, 0x47u, 6, __func__);
        const auto dataSelect = cmd[3] >> 29u;
        Agc::Command::CheckAddress(guestAddress, dataSelect == 2 || dataSelect == 3 ? 8 : 4, __func__);
        Agc::Command::CheckBits(guestAddress, 0xffffffffffffull, __func__);
        cmd[2] = static_cast<std::uint32_t>(guestAddress);
        cmd[3] = (cmd[3] & 0xffff0000u) | static_cast<std::uint32_t>(guestAddress >> 32u);
    }
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchData(uint32_t* cmd, uint64_t data) {
    // RELEASE_MEM as built by sceAgcCbReleaseMem: [2] data select << 29, [5..6] data.
    Agc::Command::ValidatePacket(cmd, 0x49u, 8, __func__);
    if ((cmd[2] >> 29u) == 1u) Agc::Command::CheckBits(data, 0xffffffffu, __func__);
    cmd[5] = static_cast<std::uint32_t>(data);
    cmd[6] = static_cast<std::uint32_t>(data >> 32u);
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchGcrCntl(std::uint32_t* cmd, std::uint16_t gcrControl) {
    Agc::Command::ValidatePacket(cmd, 0x49u, 8, __func__);
    Agc::Command::CheckBits(gcrControl, 0xfffu, __func__);
    std::uint32_t control = gcrControl;
    if ((control & 0x300u) == 0x100u) {
        control |= 0x200u;
    }
    cmd[1] = (cmd[1] & ~(0xfffu << 12u)) | (control << 12u);
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchType(std::uint32_t* cmd, std::uint8_t action) {
    Agc::Command::ValidatePacket(cmd, 0x49u, 8, __func__);
    Agc::Command::CheckBits(action, 0x3fu, __func__);
    const std::uint32_t eventIndex = action >= 0x2fu ? 6u : 5u;
    cmd[1] = (cmd[1] & ~0xf3fu) | action | (eventIndex << 8u);
    return 0;
}

}
