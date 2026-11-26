#include "prx/libSceGnmDriver/include/DrawApi.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace {

constexpr int CommandFailed = -1;
constexpr std::uint32_t NeoDrawFlags = 0xe0000000u;
constexpr std::uint32_t AutoIndex = 2u;
constexpr std::uint32_t ShaderStageCount = 7u;
constexpr std::uint32_t IndirectSgprBase[ShaderStageCount] = {0u, 0u, 0x4cu, 0u, 0xccu, 0u, 0x14cu};
constexpr std::uint32_t DrawIndirectArgsBytes = 16u;
constexpr std::uint32_t DrawIndexedIndirectArgsBytes = 20u;

constexpr std::uint32_t Header(std::uint32_t opcode, std::uint32_t bodyWords, bool compute = false, bool predicated = false) {
    return 0xc0000000u | ((bodyWords - 1u) << 16u) | (opcode << 8u) | (compute ? 2u : 0u) | (predicated ? 1u : 0u);
}

std::uint32_t* Nop(std::uint32_t* at, std::uint32_t bodyWords) {
    at[0] = Header(0x10u, bodyWords);
    at[1] = 0u;
    return at + 1u + bodyWords;
}

std::uint32_t IndirectSgpr(std::uint32_t stage, std::uint32_t offset) {
    return offset == 0u ? 0u : IndirectSgprBase[stage] + offset;
}

bool ValidIndirect(std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr) {
    return stage < ShaderStageCount && vertexSgpr < 16u && instanceSgpr < 16u;
}

bool IndexedIndirectStage(std::uint32_t stage) {
    return stage == 2u || stage == 4u || stage == 6u;
}

template <std::size_t Words>
bool Emit(std::uint32_t* cmdbuf, const std::array<std::uint32_t, Words>& packet, std::size_t first, std::size_t words) {
    return GuestCopyOut_nid_no_patch(cmdbuf + first, packet.data() + first, words * sizeof(std::uint32_t));
}

}

extern "C" {

int APS5_VABI sceGnmDrawIndex_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t indexCount, std::uint64_t indexAddress, std::uint32_t flags, std::uint32_t) {
    if (!cmdbuf || size != 10 || indexAddress == 0 || (indexAddress & 1) != 0 || (flags & 0x1ffffffeu) != 0) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    std::array<std::uint32_t, 10> packet{};
    packet[0] = Header(0x27, 5);
    packet[1] = indexCount;
    packet[2] = static_cast<std::uint32_t>(indexAddress);
    packet[3] = static_cast<std::uint32_t>(indexAddress >> 32);
    packet[4] = indexCount;
    packet[5] = initiator;
    Nop(packet.data() + 6, 3);
    return Emit(cmdbuf, packet, 0u, 8u) ? 0 : CommandFailed;
}

int APS5_VABI sceGnmDrawIndexAuto_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t indexCount, std::uint32_t flags) {
    if (!cmdbuf || size != 7 || (flags & 0x1ffffffeu) != 0) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    std::array<std::uint32_t, 7> packet{};
    packet[0] = Header(0x2d, 2);
    packet[1] = indexCount;
    packet[2] = initiator | AutoIndex;
    Nop(packet.data() + 3, 3);
    return Emit(cmdbuf, packet, 0u, 5u) ? 0 : CommandFailed;
}

int APS5_VABI sceGnmDrawIndexIndirect_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 9 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    std::array<std::uint32_t, 9> packet{};
    packet[0] = Header(0x25, 4, false, flags & 1);
    packet[1] = dataOffset;
    packet[2] = IndirectSgpr(stage, vertexSgpr);
    packet[3] = IndirectSgpr(stage, instanceSgpr);
    packet[4] = initiator;
    Nop(packet.data() + 5, 3);
    return Emit(cmdbuf, packet, 0u, 7u) ? 0 : CommandFailed;
}

int APS5_VABI sceGnmDrawIndexIndirectCountMulti_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint64_t countAddress, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    if (!cmdbuf || size != 0x10 || vertexSgpr >= 0x10 || instanceSgpr >= 0x10 || !IndexedIndirectStage(stage)) return CommandFailed;
    std::array<std::uint32_t, 16> packet{};
    auto* at = Nop(packet.data(), 2);
    at[0] = Header(0x9d, 9, false, flags & 1);
    at[1] = dataOffset;
    at[2] = IndirectSgpr(stage, vertexSgpr);
    at[3] = IndirectSgpr(stage, instanceSgpr);
    at[4] = countAddress != 0 ? 1u << 30 : 0;
    at[5] = maxCount;
    at[6] = static_cast<std::uint32_t>(countAddress);
    at[7] = static_cast<std::uint32_t>(countAddress >> 32);
    at[8] = DrawIndexedIndirectArgsBytes;
    at[9] = initiator;
    Nop(at + 10, 2);
    return Emit(cmdbuf, packet, 0u, 2u) && Emit(cmdbuf, packet, 3u, 12u) ? 0 : CommandFailed;
}

int APS5_VABI sceGnmDrawIndexIndirectMulti_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 11 || vertexSgpr >= 0x10 || instanceSgpr >= 0x10 || !IndexedIndirectStage(stage)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    std::array<std::uint32_t, 11> packet{};
    packet[0] = Header(0x38, 6, false, flags & 1);
    packet[1] = dataOffset;
    packet[2] = IndirectSgpr(stage, vertexSgpr);
    packet[3] = IndirectSgpr(stage, instanceSgpr);
    packet[4] = maxCount;
    packet[5] = DrawIndexedIndirectArgsBytes;
    packet[6] = initiator;
    Nop(packet.data() + 7, 3);
    return Emit(cmdbuf, packet, 0u, 9u) ? 0 : CommandFailed;
}

int APS5_VABI sceGnmDrawIndexOffset_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t indexOffset, std::uint32_t indexCount, std::uint32_t flags) {
    if (!cmdbuf || size != 9) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    std::array<std::uint32_t, 9> packet{};
    packet[0] = Header(0x35, 4, false, flags & 1);
    packet[1] = indexCount;
    packet[2] = indexOffset;
    packet[3] = indexCount;
    packet[4] = initiator;
    Nop(packet.data() + 5, 3);
    return Emit(cmdbuf, packet, 0u, 7u) ? 0 : CommandFailed;
}

int APS5_VABI sceGnmDrawIndirect_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 9 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    std::array<std::uint32_t, 9> packet{};
    packet[0] = Header(0x24, 4, false, flags & 1);
    packet[1] = dataOffset;
    packet[2] = IndirectSgpr(stage, vertexSgpr);
    packet[3] = IndirectSgpr(stage, instanceSgpr);
    packet[4] = initiator | AutoIndex;
    Nop(packet.data() + 5, 3);
    return Emit(cmdbuf, packet, 0u, 7u) ? 0 : CommandFailed;
}

int APS5_VABI sceGnmDrawIndirectCountMulti_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint64_t countAddress, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 0x10 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    std::array<std::uint32_t, 16> packet{};
    auto* at = Nop(packet.data(), 2);
    at[0] = Header(0x9c, 9, false, flags & 1);
    at[1] = dataOffset;
    at[2] = IndirectSgpr(stage, vertexSgpr);
    at[3] = IndirectSgpr(stage, instanceSgpr);
    at[4] = countAddress != 0 ? 1u << 30 : 0;
    at[5] = maxCount;
    at[6] = static_cast<std::uint32_t>(countAddress);
    at[7] = static_cast<std::uint32_t>(countAddress >> 32);
    at[8] = DrawIndirectArgsBytes;
    at[9] = initiator | AutoIndex;
    Nop(at + 10, 2);
    return Emit(cmdbuf, packet, 0u, 2u) && Emit(cmdbuf, packet, 3u, 12u) ? 0 : CommandFailed;
}

int APS5_VABI sceGnmDrawIndirectMulti_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 11 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    std::array<std::uint32_t, 11> packet{};
    packet[0] = Header(0x2c, 6, false, flags & 1);
    packet[1] = dataOffset;
    packet[2] = IndirectSgpr(stage, vertexSgpr);
    packet[3] = IndirectSgpr(stage, instanceSgpr);
    packet[4] = maxCount;
    packet[5] = DrawIndirectArgsBytes;
    packet[6] = initiator | AutoIndex;
    Nop(packet.data() + 7, 3);
    return Emit(cmdbuf, packet, 0u, 9u) ? 0 : CommandFailed;
}

}
