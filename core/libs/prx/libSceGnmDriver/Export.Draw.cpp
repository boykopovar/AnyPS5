#include "prx/libSceGnmDriver/include/DrawApi.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
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

}

extern "C" {

int APS5_VABI sceGnmDrawIndex_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t indexCount, std::uint64_t indexAddress, std::uint32_t flags, std::uint32_t) {
    if (!cmdbuf || size != 10 || indexAddress == 0 || (indexAddress & 1) != 0 || (flags & 0x1ffffffeu) != 0) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    cmdbuf[0] = Header(0x27, 5);
    cmdbuf[1] = indexCount;
    cmdbuf[2] = static_cast<std::uint32_t>(indexAddress);
    cmdbuf[3] = static_cast<std::uint32_t>(indexAddress >> 32);
    cmdbuf[4] = indexCount;
    cmdbuf[5] = initiator;
    Nop(cmdbuf + 6, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndexAuto_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t indexCount, std::uint32_t flags) {
    if (!cmdbuf || size != 7 || (flags & 0x1ffffffeu) != 0) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    cmdbuf[0] = Header(0x2d, 2);
    cmdbuf[1] = indexCount;
    cmdbuf[2] = initiator | AutoIndex;
    Nop(cmdbuf + 3, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndexIndirect_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 9 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    cmdbuf[0] = Header(0x25, 4, false, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = IndirectSgpr(stage, vertexSgpr);
    cmdbuf[3] = IndirectSgpr(stage, instanceSgpr);
    cmdbuf[4] = initiator;
    Nop(cmdbuf + 5, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndexIndirectCountMulti_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint64_t countAddress, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    if (!cmdbuf || size != 0x10 || vertexSgpr >= 0x10 || instanceSgpr >= 0x10 || !IndexedIndirectStage(stage)) return CommandFailed;
    auto* at = Nop(cmdbuf, 2);
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
    return 0;
}

int APS5_VABI sceGnmDrawIndexIndirectMulti_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 11 || vertexSgpr >= 0x10 || instanceSgpr >= 0x10 || !IndexedIndirectStage(stage)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    cmdbuf[0] = Header(0x38, 6, false, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = IndirectSgpr(stage, vertexSgpr);
    cmdbuf[3] = IndirectSgpr(stage, instanceSgpr);
    cmdbuf[4] = maxCount;
    cmdbuf[5] = DrawIndexedIndirectArgsBytes;
    cmdbuf[6] = initiator;
    Nop(cmdbuf + 7, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndexOffset_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t indexOffset, std::uint32_t indexCount, std::uint32_t flags) {
    if (!cmdbuf || size != 9) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    cmdbuf[0] = Header(0x35, 4, false, flags & 1);
    cmdbuf[1] = indexCount;
    cmdbuf[2] = indexOffset;
    cmdbuf[3] = indexCount;
    cmdbuf[4] = initiator;
    Nop(cmdbuf + 5, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndirect_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 9 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    cmdbuf[0] = Header(0x24, 4, false, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = IndirectSgpr(stage, vertexSgpr);
    cmdbuf[3] = IndirectSgpr(stage, instanceSgpr);
    cmdbuf[4] = initiator | AutoIndex;
    Nop(cmdbuf + 5, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndirectCountMulti_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint64_t countAddress, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 0x10 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    auto* at = Nop(cmdbuf, 2);
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
    return 0;
}

int APS5_VABI sceGnmDrawIndirectMulti_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 11 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    const std::uint32_t initiator = sceKernelIsNeoMode() != 0 ? flags & NeoDrawFlags : 0u;
    cmdbuf[0] = Header(0x2c, 6, false, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = IndirectSgpr(stage, vertexSgpr);
    cmdbuf[3] = IndirectSgpr(stage, instanceSgpr);
    cmdbuf[4] = maxCount;
    cmdbuf[5] = DrawIndirectArgsBytes;
    cmdbuf[6] = initiator | AutoIndex;
    Nop(cmdbuf + 7, 3);
    return 0;
}

}
