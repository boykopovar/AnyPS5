#include "prx/libSceAgc/Patch/include/QueueEndOfPipeAction.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::uint32_t readHeader(const std::uint32_t* cmd, const char* function) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), 4, function);
    std::uint32_t header = 0;
    Agc::Command::Require(GuestCopyIn_nid_no_patch(&header, cmd, sizeof(header)), function, "packet is not readable");
    return header;
}

template <std::size_t Count>
std::array<std::uint32_t, Count> readPacket(const std::uint32_t* cmd, std::uint32_t opcode, const char* function) {
    std::array<std::uint32_t, Count> packet{};
    packet[0] = readHeader(cmd, function);
    Agc::Command::ValidatePacket(packet.data(), opcode, Count, function);
    Agc::Command::Require(GuestCopyIn_nid_no_patch(packet.data() + 1, cmd + 1, (Count - 1) * sizeof(std::uint32_t)), function, "packet is not readable");
    return packet;
}

template <std::size_t Count>
void writeWords(std::uint32_t* cmd, const std::array<std::uint32_t, Count>& packet, std::size_t first, std::size_t count, const char* function) {
    Agc::Command::Require(GuestCopyOut_nid_no_patch(cmd + first, packet.data() + first, count * sizeof(std::uint32_t)), function, "packet is not writable");
}

}

extern "C" {

int APS5_VABI sceAgcQueueEndOfPipeActionPatchAddress(std::uint32_t* cmd, const volatile Label* address) {
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    const auto opcode = (readHeader(cmd, __func__) >> 8u) & 0xffu;
    if (opcode == 0x49u) {
        auto packet = readPacket<8>(cmd, opcode, __func__);
        const auto dataSelect = packet[2] >> 29u;
        Agc::Command::CheckAddress(guestAddress, dataSelect == 2 || dataSelect == 3 ? 8 : 4, __func__);
        packet[3] = static_cast<std::uint32_t>(guestAddress);
        packet[4] = static_cast<std::uint32_t>(guestAddress >> 32u);
        writeWords(cmd, packet, 3, 2, __func__);
    } else {
        auto packet = readPacket<6>(cmd, 0x47u, __func__);
        const auto dataSelect = packet[3] >> 29u;
        Agc::Command::CheckAddress(guestAddress, dataSelect == 2 || dataSelect == 3 ? 8 : 4, __func__);
        Agc::Command::CheckBits(guestAddress, 0xffffffffffffull, __func__);
        packet[2] = static_cast<std::uint32_t>(guestAddress);
        packet[3] = (packet[3] & 0xffff0000u) | static_cast<std::uint32_t>(guestAddress >> 32u);
        writeWords(cmd, packet, 2, 2, __func__);
    }
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchData(uint32_t* cmd, uint64_t data) {
    auto packet = readPacket<8>(cmd, 0x49u, __func__);
    const auto dataSelect = packet[2] >> 29u;
    if (((packet[2] >> 24u) & 7u) == 4u) {
        Agc::Command::Require(data == 0 && (packet[3] | packet[4]) == 0, __func__, "interrupt-only release cannot write data");
    } else if (dataSelect == 5u) {
        Agc::Command::Require(data == 0, __func__, "GDS release cannot use immediate data");
        return 0;
    }
    if (dataSelect == 1u) Agc::Command::CheckBits(data, 0xffffffffu, __func__);
    packet[5] = static_cast<std::uint32_t>(data);
    packet[6] = static_cast<std::uint32_t>(data >> 32u);
    writeWords(cmd, packet, 5, 2, __func__);
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchGcrCntl(std::uint32_t* cmd, std::uint16_t gcrControl) {
    auto packet = readPacket<8>(cmd, 0x49u, __func__);
    Agc::Command::CheckBits(gcrControl, 0xfffu, __func__);
    std::uint32_t control = gcrControl;
    if ((control & 0x300u) == 0x100u) {
        control |= 0x200u;
    }
    packet[1] = (packet[1] & ~(0xfffu << 12u)) | (control << 12u);
    writeWords(cmd, packet, 1, 1, __func__);
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchType(std::uint32_t* cmd, std::uint8_t action) {
    auto packet = readPacket<8>(cmd, 0x49u, __func__);
    Agc::Command::CheckBits(action, 0x3fu, __func__);
    const std::uint32_t eventIndex = action >= 0x2fu ? 6u : 5u;
    packet[1] = (packet[1] & ~0xf3fu) | action | (eventIndex << 8u);
    writeWords(cmd, packet, 1, 1, __func__);
    return 0;
}

}
