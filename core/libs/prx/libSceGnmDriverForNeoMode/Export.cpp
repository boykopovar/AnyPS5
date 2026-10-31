#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceGnmDriverForNeoMode/include/InventoryExports.hpp"
#include "prx/libSceGnmDriverForNeoMode/include/Api.hpp"
#include <cstdint>
#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceGnmDriverForNeoMode/include/InitSequences.hpp"
#include "prx/libSceGnmDriverForNeoMode/include/EmbeddedShaders.hpp"

namespace {

constexpr std::uint32_t InitPacketWords = 0x100;

template <std::size_t Words>
int WriteHardwareState(std::uint32_t* cmdbuf, std::uint32_t size, const std::array<std::uint32_t, Words>& state) {
    if (size < InitPacketWords) return 0;
    auto* at = std::copy(GnmNeo::ClearState.begin(), GnmNeo::ClearState.end(), cmdbuf);
    at = std::copy(state.begin(), state.end(), at);
    const auto padding = static_cast<std::uint32_t>(cmdbuf + InitPacketWords - at - 1);
    at[0] = 0xc0001000u | ((padding - 1) << 16);
    at[1] = 0;
    return InitPacketWords;
}

template <std::size_t Words>
int WriteContextState(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t packetWords, const std::array<std::uint32_t, Words>& state) {
    if (size != packetWords) return 0;
    std::copy(state.begin(), state.end(), cmdbuf);
    return static_cast<int>(packetWords);
}

constexpr std::uint32_t OpSetContextReg = 0x69;
constexpr std::uint32_t OpSetShReg = 0x76;

std::uint32_t* EmitPacket(std::uint32_t* at, std::uint32_t opcode, std::initializer_list<std::uint32_t> body) {
    *at++ = 0xc0000000u | ((static_cast<std::uint32_t>(body.size()) - 1) << 16) | (opcode << 8);
    for (const auto word : body) *at++ = word;
    return at;
}

std::uint32_t AddressLo(const void* code) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code) >> 8);
}

std::uint32_t AddressHi(const void* code) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code) >> 40);
}

constexpr int Failure = static_cast<int>(0x8EEE00FFu);
constexpr int NoInterface = static_cast<int>(0x80000000u);
constexpr int ValidationNotEnabled = static_cast<int>(0x80D13FFFu);

constexpr int CommandFailed = -1;
constexpr std::uint32_t OpNop = 0x10;
constexpr std::uint32_t NeoDrawFlags = 0xe0000000u;
constexpr std::uint32_t AutoIndex = 2;

constexpr std::uint32_t Header(std::uint32_t opcode, std::uint32_t bodyWords, bool compute = false, bool predicated = false) {
    return 0xc0000000u | ((bodyWords - 1) << 16) | (opcode << 8) | (compute ? 2u : 0u) | (predicated ? 1u : 0u);
}

std::uint32_t* Nop(std::uint32_t* at, std::uint32_t bodyWords) {
    at[0] = Header(OpNop, bodyWords);
    at[1] = 0;
    return at + 1 + bodyWords;
}

constexpr std::uint32_t ShaderStageCount = 7;
constexpr std::uint32_t StageVs = 2;
constexpr std::uint32_t StageEs = 4;
constexpr std::uint32_t StageLs = 6;
constexpr std::uint32_t IndirectSgprBase[ShaderStageCount] = {0, 0, 0x4c, 0, 0xcc, 0, 0x14c};
constexpr std::uint32_t DrawIndirectArgsBytes = 0x10;
constexpr std::uint32_t DrawIndexedIndirectArgsBytes = 0x14;

std::uint32_t IndirectSgpr(std::uint32_t stage, std::uint32_t offset) {
    return offset == 0 ? 0 : (offset & 0xffffu) + IndirectSgprBase[stage];
}

bool ValidIndirect(std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr) {
    return stage < ShaderStageCount && vertexSgpr < 0x10 && instanceSgpr < 0x10;
}

bool IndexedIndirectStage(std::uint32_t stage) {
    return stage == StageVs || stage == StageEs || stage == StageLs;
}

constexpr std::uint32_t OpSetUconfigReg = 0x79;
constexpr std::uint32_t IaMultiVgtParam = 0x40000258u;

std::uint32_t* UpdatePsRegisters(std::uint32_t* cmdbuf, const std::uint32_t* regs) {
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {10, regs[2], regs[3]});
    at = EmitPacket(at, OpNop, {0xc01e01c4u, regs[4], regs[5]});
    at = EmitPacket(at, OpNop, {0xc01e01b3u, regs[6], regs[7]});
    at = EmitPacket(at, OpNop, {0xc01e01b6u, regs[8]});
    at = EmitPacket(at, OpNop, {0xc01e01b8u, regs[9]});
    at = EmitPacket(at, OpNop, {0xc01e0203u, regs[10]});
    return EmitPacket(at, OpNop, {0xc01e008fu, regs[11]});
}

} // namespace

extern "C" {

int APS5_VABI sceGnmBeginWorkload(std::uint32_t stream, std::uint64_t* workload) {
    if (!workload) return 3;
    *workload = stream < 0x10 ? 1 : 0;
    return stream < 0x10 ? 0 : 1;
}

int APS5_VABI sceGnmComputeWaitOnAddress(std::uint32_t* cmdbuf, std::uint32_t size, std::uint64_t address, std::uint32_t mask, std::uint32_t compare, std::uint32_t reference) {
    if (!cmdbuf || size != 14) return CommandFailed;
    auto* at = Nop(cmdbuf, 3);
    const bool memory = address > 0xffff;
    at[0] = Header(0x3c, 6);
    at[1] = (memory ? 1u << 4 : 0u) | (compare & 7);
    at[2] = static_cast<std::uint32_t>(address & (memory ? 0xfffffffcu : 0xffffu));
    at[3] = static_cast<std::uint32_t>(address >> 32);
    at[4] = reference;
    at[5] = mask;
    at[6] = 10;
    Nop(at + 7, 2);
    return 0;
}

int APS5_VABI sceGnmComputeWaitSemaphore(std::uint32_t* cmdbuf, std::uint32_t size, std::uint64_t semaphoreAddress, std::uint32_t flags) {
    if (!cmdbuf || size != 10) return CommandFailed;
    auto* at = Nop(cmdbuf, 3);
    at[0] = Header(0x39, 2);
    at[1] = static_cast<std::uint32_t>(semaphoreAddress);
    at[2] = (flags == 1 ? 0x100000u : 0u) - 0x20000000u + static_cast<std::uint32_t>(semaphoreAddress >> 32);
    Nop(at + 3, 2);
    return 0;
}

int APS5_VABI sceGnmCreateWorkloadStream(std::uint64_t name, std::uint32_t* stream) {
    if (name == 0 || !stream) return 3;
    *stream = 1;
    return 0;
}

int APS5_VABI sceGnmDebugModuleReset(void) {
    return Failure;
}

int APS5_VABI sceGnmDebugReset(void) {
    return Failure;
}

int APS5_VABI sceGnmDestroyWorkloadStream(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDingDongForWorkload(std::uint32_t vqueueId, std::uint32_t nextOffsetDw, std::uint64_t workloadId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDispatchDirect(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t threadsX, std::uint32_t threadsY, std::uint32_t threadsZ, std::uint32_t flags) {
    if (!cmdbuf || size != 9 || static_cast<std::int32_t>(threadsX | threadsY | threadsZ) < 0) return CommandFailed;
    cmdbuf[0] = Header(0x15, 4, true, flags & 1);
    cmdbuf[1] = threadsX;
    cmdbuf[2] = threadsY;
    cmdbuf[3] = threadsZ;
    cmdbuf[4] = (flags & 0x18) + 1;
    Nop(cmdbuf + 5, 3);
    return 0;
}

int APS5_VABI sceGnmDispatchIndirect(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t flags) {
    if (!cmdbuf || size != 7) return CommandFailed;
    cmdbuf[0] = Header(0x16, 2, true, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = (flags & 0x18) + 1;
    Nop(cmdbuf + 3, 3);
    return 0;
}

int APS5_VABI sceGnmDispatchIndirectOnMec(std::uint32_t* cmdbuf, std::uint32_t size, std::uint64_t args, std::uint32_t modifier) {
    if (!cmdbuf || size != 8 || args == 0 || (args & 3) != 0) return CommandFailed;
    cmdbuf[0] = Header(0x16, 3, true, modifier & 1);
    cmdbuf[1] = static_cast<std::uint32_t>(args);
    cmdbuf[2] = static_cast<std::uint32_t>(args >> 32);
    cmdbuf[3] = (modifier & 0x18) | 1;
    cmdbuf[4] = Header(OpNop, 3);
    cmdbuf[5] = 0;
    return 0;
}

int APS5_VABI sceGnmDrawIndex(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t indexCount, std::uint64_t indexAddress, std::uint32_t flags, std::uint32_t) {
    if (!cmdbuf || size != 10 || indexAddress == 0 || (indexAddress & 1) != 0 || (flags & 0x1ffffffeu) != 0) return CommandFailed;
    cmdbuf[0] = Header(0x27, 5);
    cmdbuf[1] = indexCount;
    cmdbuf[2] = static_cast<std::uint32_t>(indexAddress);
    cmdbuf[3] = static_cast<std::uint32_t>(indexAddress >> 32);
    cmdbuf[4] = indexCount;
    cmdbuf[5] = flags & NeoDrawFlags;
    Nop(cmdbuf + 6, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndexAuto(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t indexCount, std::uint32_t flags) {
    if (!cmdbuf || size != 7 || (flags & 0x1ffffffeu) != 0) return CommandFailed;
    cmdbuf[0] = Header(0x2d, 2);
    cmdbuf[1] = indexCount;
    cmdbuf[2] = (flags & NeoDrawFlags) | AutoIndex;
    Nop(cmdbuf + 3, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndexIndirect(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 9 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    cmdbuf[0] = Header(0x25, 4, false, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = IndirectSgpr(stage, vertexSgpr);
    cmdbuf[3] = IndirectSgpr(stage, instanceSgpr);
    cmdbuf[4] = flags & NeoDrawFlags;
    Nop(cmdbuf + 5, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndexIndirectCountMulti(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint64_t countAddress, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
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
    at[9] = flags & NeoDrawFlags;
    Nop(at + 10, 2);
    return 0;
}

int APS5_VABI sceGnmDrawIndexIndirectMulti(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 11 || vertexSgpr >= 0x10 || instanceSgpr >= 0x10 || !IndexedIndirectStage(stage)) return CommandFailed;
    cmdbuf[0] = Header(0x38, 6, false, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = IndirectSgpr(stage, vertexSgpr);
    cmdbuf[3] = IndirectSgpr(stage, instanceSgpr);
    cmdbuf[4] = maxCount;
    cmdbuf[5] = DrawIndexedIndirectArgsBytes;
    cmdbuf[6] = flags & NeoDrawFlags;
    Nop(cmdbuf + 7, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndexMultiInstanced(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndexOffset(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t indexOffset, std::uint32_t indexCount, std::uint32_t flags) {
    if (!cmdbuf || size != 9) return CommandFailed;
    cmdbuf[0] = Header(0x35, 4, false, flags & 1);
    cmdbuf[1] = indexCount;
    cmdbuf[2] = indexOffset;
    cmdbuf[3] = indexCount;
    cmdbuf[4] = flags & NeoDrawFlags;
    Nop(cmdbuf + 5, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndirect(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 9 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    cmdbuf[0] = Header(0x24, 4, false, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = IndirectSgpr(stage, vertexSgpr);
    cmdbuf[3] = IndirectSgpr(stage, instanceSgpr);
    cmdbuf[4] = (flags & NeoDrawFlags) | AutoIndex;
    Nop(cmdbuf + 5, 3);
    return 0;
}

int APS5_VABI sceGnmDrawIndirectCountMulti(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint64_t countAddress, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 0x10 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
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
    at[9] = (flags & NeoDrawFlags) | AutoIndex;
    Nop(at + 10, 2);
    return 0;
}

int APS5_VABI sceGnmDrawIndirectMulti(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t maxCount, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr, std::uint32_t flags) {
    if (!cmdbuf || size != 11 || !ValidIndirect(stage, vertexSgpr, instanceSgpr)) return CommandFailed;
    cmdbuf[0] = Header(0x2c, 6, false, flags & 1);
    cmdbuf[1] = dataOffset;
    cmdbuf[2] = IndirectSgpr(stage, vertexSgpr);
    cmdbuf[3] = IndirectSgpr(stage, instanceSgpr);
    cmdbuf[4] = maxCount;
    cmdbuf[5] = DrawIndirectArgsBytes;
    cmdbuf[6] = (flags & NeoDrawFlags) | AutoIndex;
    Nop(cmdbuf + 7, 3);
    return 0;
}

int APS5_VABI sceGnmDrawInitDefaultHardwareState(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteHardwareState(cmdbuf, size, GnmNeo::HardwareState);
}

int APS5_VABI sceGnmDrawInitDefaultHardwareState200(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteHardwareState(cmdbuf, size, GnmNeo::HardwareState200);
}

int APS5_VABI sceGnmDrawInitDefaultHardwareState350(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteHardwareState(cmdbuf, size, GnmNeo::HardwareState350);
}

int APS5_VABI sceGnmDrawInitToDefaultContextState(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteContextState(cmdbuf, size, 0x20, GnmNeo::ContextState);
}

int APS5_VABI sceGnmDrawInitToDefaultContextState400(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteContextState(cmdbuf, size, 0x100, GnmNeo::ContextState400);
}

int APS5_VABI sceGnmDrawInitToDefaultContextStateInternalCommand(std::uint32_t* cmdbuf, std::uint32_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawInitToDefaultContextStateInternalSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawOpaqueAuto(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterface(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForGpuDebugger(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForReplay(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForResourceRegistration(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForValidation(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalVirtualQuery(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDriverTraceInProgress(void) {
    return 0;
}

int APS5_VABI sceGnmEndWorkload(std::uint64_t workload) {
    if (workload == 0) return 2;
    return ((workload >> 56) & 0xff) > 0xf ? 2 : 0;
}

int APS5_VABI sceGnmFindResources(void) {
    return Failure;
}

int APS5_VABI sceGnmFindResourcesPublic(void) {
    return Failure;
}

int APS5_VABI sceGnmGetCoredumpAddress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetCoredumpMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetCoredumpProtectionFaultTimestamp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetDbgGcHandle(void) {
    return -1;
}

int APS5_VABI sceGnmGetGpuBlockStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetGpuCoreClockFrequency(void) {
    return 911000000;
}

int APS5_VABI sceGnmGetGpuInfoStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetLastWaitedAddress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetNumTcaUnits(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetOffChipTessellationBufferSize(void) {
    return 0x800000;
}

int APS5_VABI sceGnmGetOwnerName(void) {
    return Failure;
}

int APS5_VABI sceGnmGetPhysicalCounterFromVirtualized(void) {
    return Failure;
}

int APS5_VABI sceGnmGetProtectionFaultTimeStamp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetResourceBaseAddressAndSizeInBytes(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceName(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceRegistrationBuffers(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceShaderGuid(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceType(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceUserData(void) {
    return Failure;
}

int APS5_VABI sceGnmGetShaderProgramBaseAddress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetShaderStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGpuPaDebugEnter(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGpuPaDebugLeave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmInsertDingDongMarker(std::uint32_t* cmdbuf, std::uint32_t size) {
    if (!cmdbuf || size != 4) return CommandFailed;
    cmdbuf[0] = Header(OpNop, 3);
    cmdbuf[1] = 0;
    cmdbuf[2] = 0;
    cmdbuf[3] = 0;
    return 0;
}

int APS5_VABI sceGnmInsertThreadTraceMarker(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmIsCoredumpValid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmLogicalTcaUnitToPhysical(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmMapComputeQueueWithPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmPaDisableFlipCallbacks(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmPaEnableFlipCallbacks(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmPaHeartbeat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmQueryResourceRegistrationUserMemoryRequirements(void) {
    return Failure;
}

int APS5_VABI sceGnmRaiseUserExceptionEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmRegisterGdsResource(void) {
    return Failure;
}

int APS5_VABI sceGnmRegisterGnmLiveCallbackConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmRegisterOwner(void* handle, const char* name) {
    return Failure;
}

int APS5_VABI sceGnmRegisterOwnerForSystem(void) {
    return Failure;
}

int APS5_VABI sceGnmRegisterResource(void) {
    return Failure;
}

int APS5_VABI sceGnmRequestFlipAndSubmitDoneForWorkload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmResetVgtControl(std::uint32_t* cmdbuf, std::uint32_t size) {
    if (!cmdbuf || size != 3) return CommandFailed;
    cmdbuf[0] = Header(OpSetUconfigReg, 2);
    cmdbuf[1] = IaMultiVgtParam;
    cmdbuf[2] = 0x6d007fu;
    return 0;
}

int APS5_VABI sceGnmSdmaClose(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaConstFill(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaCopyLinear(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaCopyTiled(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaCopyWindow(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaFlush(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaGetMinCmdSize(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaOpen(void) {
    return Failure;
}

int APS5_VABI sceGnmSetCsShaderWithModifier(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier) {
    if (!cmdbuf || size <= 0x18 || !regs || (modifier & 0xfffffc3fu) != 0 || regs[1] != 0) return CommandFailed;
    const std::uint32_t rsrc1 = modifier == 0 ? regs[2] : (regs[2] & 0xfffffc3fu) | modifier;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {0x20c, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {0x212, rsrc1, regs[3]});
    at = EmitPacket(at, OpSetShReg, {0x207, regs[4], regs[5], regs[6]});
    Nop(at, 11);
    return 0;
}

int APS5_VABI sceGnmSetEmbeddedPsShader(std::uint32_t* cmdbuf, std::uint32_t, std::uint32_t shaderId, std::uint32_t) {
    if (shaderId > 1) return Failure;
    const bool black = shaderId == 0;
    const auto* code = black ? GnmNeo::BlackPs.data() : GnmNeo::DepthOnlyPs.data();
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, AddressLo(code), AddressHi(code)});
    at = EmitPacket(at, OpSetShReg, {10, black ? 0xc0000u : 0x2000u, black ? 4u : 0u});
    at = EmitPacket(at, OpSetContextReg, {0x1c4, 0, black ? 4u : 2u});
    at = EmitPacket(at, OpSetContextReg, {0x1b3, 2, 2});
    at = EmitPacket(at, OpSetContextReg, {0x1b6, 0});
    at = EmitPacket(at, OpSetContextReg, {0x1b8, 0});
    at = EmitPacket(at, OpSetContextReg, {0x203, 0x10});
    at = EmitPacket(at, OpSetContextReg, {0x8f, black ? 0xfu : 3u});
    at[0] = 0xc00a1000u;
    at[1] = 0;
    return 0;
}

int APS5_VABI sceGnmSetEmbeddedVsShader(std::uint32_t* cmdbuf, std::uint32_t, std::uint32_t shaderId, std::uint32_t) {
    if (shaderId != 0) return Failure;
    const auto* code = GnmNeo::FullscreenVs.data();
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {0x48, AddressLo(code), AddressHi(code)});
    at = EmitPacket(at, OpSetShReg, {0x4a, 0xc0000, 4});
    at = EmitPacket(at, OpSetContextReg, {0x207, 0});
    at = EmitPacket(at, OpSetContextReg, {0x1b1, 0});
    at = EmitPacket(at, OpSetContextReg, {0x1c3, 4});
    at[0] = 0xc00a1000u;
    at[1] = 0;
    return 0;
}

int APS5_VABI sceGnmSetPsShader350(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmdbuf || size <= 0x27) return CommandFailed;
    if (!regs) {
        auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, 0, 0});
        at = EmitPacket(at, OpSetContextReg, {0x203, 0});
        at = EmitPacket(at, OpSetContextReg, {0x8f, 0xf});
        Nop(at, 0x1d);
        return 0;
    }
    if (regs[1] != 0) return CommandFailed;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {10, regs[2], regs[3]});
    at = EmitPacket(at, OpSetContextReg, {0x1c4, regs[4], regs[5]});
    at = EmitPacket(at, OpSetContextReg, {0x1b3, regs[6], regs[7]});
    at = EmitPacket(at, OpSetContextReg, {0x1b6, regs[8]});
    at = EmitPacket(at, OpSetContextReg, {0x1b8, regs[9]});
    at = EmitPacket(at, OpSetContextReg, {0x203, regs[10]});
    at = EmitPacket(at, OpSetContextReg, {0x8f, regs[11]});
    Nop(at, 11);
    return 0;
}

int APS5_VABI sceGnmSetResourceRegistrationUserMemory(void) {
    return Failure;
}

int APS5_VABI sceGnmSetResourceUserData(void) {
    return Failure;
}

int APS5_VABI sceGnmSetSpiEnableSqCounters(void) {
    return Failure;
}

int APS5_VABI sceGnmSetSpiEnableSqCountersForUnitInstance(void) {
    return Failure;
}

int APS5_VABI sceGnmSetVgtControl(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t primGroupSizeMinusOne, std::uint32_t partialVsWaveMode, std::uint32_t switchOnlyOnEop) {
    if (!cmdbuf || size != 3 || primGroupSizeMinusOne >= 0x100 || (switchOnlyOnEop | partialVsWaveMode) >= 2) return CommandFailed;
    const std::uint32_t switchOnEop = static_cast<std::uint32_t>(switchOnlyOnEop != 0) << 20;
    const std::uint32_t switchOnEoi = static_cast<std::uint32_t>(switchOnlyOnEop == 0) << 19;
    const std::uint32_t value = switchOnlyOnEop != 0
        ? ((partialVsWaveMode & 1) << 16) | primGroupSizeMinusOne | switchOnEop | switchOnEoi | 0x40000u
        : (primGroupSizeMinusOne & 0x1cffffu) | switchOnEop | switchOnEoi | 0x50000u;
    cmdbuf[0] = Header(OpSetUconfigReg, 2);
    cmdbuf[1] = IaMultiVgtParam;
    cmdbuf[2] = value | 0x600000u;
    return 0;
}

int APS5_VABI sceGnmSetWaveLimitMultiplier(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSetWaveLimitMultipliers(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSubmitAndFlipCommandBuffersForWorkload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSubmitCommandBuffersForWorkload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmUnregisterAllResourcesForOwner(void) {
    return Failure;
}

int APS5_VABI sceGnmUnregisterOwnerAndResources(void) {
    return Failure;
}

int APS5_VABI sceGnmUnregisterResource(void) {
    return Failure;
}

int APS5_VABI sceGnmUpdateGsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmdbuf || size < 0x1d || !regs || regs[1] != 0) return CommandFailed;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {0x88, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {0x8a, regs[2], regs[3]});
    at = EmitPacket(at, OpNop, {0xc01e02e5u, regs[4]});
    at = EmitPacket(at, OpNop, {0xc01e029bu, regs[5]});
    at = EmitPacket(at, OpNop, {0xc01e02e4u, regs[6]});
    Nop(at, 11);
    return 0;
}

int APS5_VABI sceGnmUpdateHsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs, std::uint32_t lsHsConfig) {
    if (!cmdbuf || size <= 0x1c || !regs || regs[1] != 0) return CommandFailed;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {0x108, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {0x10a, regs[2], regs[3]});
    at = EmitPacket(at, OpNop, {0xc01e0286u, regs[5], regs[6]});
    at = EmitPacket(at, OpNop, {0xc01e02dbu, regs[4]});
    at = EmitPacket(at, OpNop, {0xc01e02d6u, lsHsConfig});
    Nop(at, 11);
    return 0;
}

int APS5_VABI sceGnmUpdatePsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmdbuf || size <= 0x27) return CommandFailed;
    if (!regs) {
        auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, 0, 0});
        at = EmitPacket(at, OpNop, {0xc01e0203u, 0});
        Nop(at, 0x20);
        return 0;
    }
    if (regs[1] != 0) return CommandFailed;
    Nop(UpdatePsRegisters(cmdbuf, regs), 11);
    return 0;
}

int APS5_VABI sceGnmUpdatePsShader350(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmdbuf || size <= 0x27) return CommandFailed;
    if (!regs) {
        auto* at = EmitPacket(cmdbuf, OpSetShReg, {8, 0, 0});
        at = EmitPacket(at, OpNop, {0xc01e0203u, 0});
        at = EmitPacket(at, OpSetContextReg, {0x8f, 0xf});
        Nop(at, 0x1d);
        return 0;
    }
    if (regs[1] != 0) return CommandFailed;
    Nop(UpdatePsRegisters(cmdbuf, regs), 11);
    return 0;
}

int APS5_VABI sceGnmUpdateVsShader(std::uint32_t* cmdbuf, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier) {
    if (!cmdbuf || size <= 0x1c || !regs || (modifier & 0xfcfffc3fu) != 0 || regs[1] != 0) return CommandFailed;
    const std::uint32_t rsrc1 = modifier == 0 ? regs[2] : (regs[2] & 0xfcfffc3fu) | modifier;
    auto* at = EmitPacket(cmdbuf, OpSetShReg, {0x48, regs[0], 0});
    at = EmitPacket(at, OpSetShReg, {0x4a, rsrc1, regs[3]});
    at = EmitPacket(at, OpNop, {0xc01e0207u, regs[6]});
    at = EmitPacket(at, OpNop, {0xc01e01b1u, regs[4]});
    at = EmitPacket(at, OpNop, {0xc01e01c3u, regs[5]});
    Nop(at, 11);
    return 0;
}

int APS5_VABI sceGnmValidateDisableDiagnostics(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateDisableDiagnostics2(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateDispatchCommandBuffers(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateDrawCommandBuffers(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateGetDiagnosticInfo(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateGetDiagnostics(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateGetVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmValidateOnSubmitEnabled(void) {
    return 0;
}

int APS5_VABI sceGnmValidateResetState(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidationRegisterMemoryCheckCallback(void) {
    return ValidationNotEnabled;
}

}

extern "C" {

int APS5_VABI libSceGnmDriverForNeoModeUnknown__plus_RaJBCVJZVM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+RaJBCVJZVM", libSceGnmDriverForNeoModeUnknown__plus_RaJBCVJZVM_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown__minus_bNK8kTFf6s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-bNK8kTFf6s", libSceGnmDriverForNeoModeUnknown__minus_bNK8kTFf6s_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_1TRGZJsC5Y4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1TRGZJsC5Y4", libSceGnmDriverForNeoModeUnknown_1TRGZJsC5Y4_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_2T1zOhnddFQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2T1zOhnddFQ", libSceGnmDriverForNeoModeUnknown_2T1zOhnddFQ_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_5R1E24FRI4w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5R1E24FRI4w", libSceGnmDriverForNeoModeUnknown_5R1E24FRI4w_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_6a3_plus_Twrkogk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6a3+Twrkogk", libSceGnmDriverForNeoModeUnknown_6a3_plus_Twrkogk_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_7LTGukH_plus_M1A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7LTGukH+M1A", libSceGnmDriverForNeoModeUnknown_7LTGukH_plus_M1A_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_8_plus_itHHi1ea0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8+itHHi1ea0", libSceGnmDriverForNeoModeUnknown_8_plus_itHHi1ea0_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_HEOIaxbuVTA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HEOIaxbuVTA", libSceGnmDriverForNeoModeUnknown_HEOIaxbuVTA_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_KKsYtugjKLw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KKsYtugjKLw", libSceGnmDriverForNeoModeUnknown_KKsYtugjKLw_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_MLr_plus_Fyrxf_plus_8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MLr+Fyrxf+8", libSceGnmDriverForNeoModeUnknown_MLr_plus_Fyrxf_plus_8_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_QP7vDGU0xDQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QP7vDGU0xDQ", libSceGnmDriverForNeoModeUnknown_QP7vDGU0xDQ_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_QWuQed5Mus4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QWuQed5Mus4", libSceGnmDriverForNeoModeUnknown_QWuQed5Mus4_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_R3TYO7Tdv5o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("R3TYO7Tdv5o", libSceGnmDriverForNeoModeUnknown_R3TYO7Tdv5o_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_UGePHM7rmgA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("UGePHM7rmgA", libSceGnmDriverForNeoModeUnknown_UGePHM7rmgA_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_epUMKkeaze8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("epUMKkeaze8", libSceGnmDriverForNeoModeUnknown_epUMKkeaze8_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_gQNwGezNDgE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gQNwGezNDgE", libSceGnmDriverForNeoModeUnknown_gQNwGezNDgE_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_sKhoi2ectC0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("sKhoi2ectC0", libSceGnmDriverForNeoModeUnknown_sKhoi2ectC0_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_ut57TBmRQN0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ut57TBmRQN0", libSceGnmDriverForNeoModeUnknown_ut57TBmRQN0_nid_no_patch);

int APS5_VABI libSceGnmDriverForNeoModeUnknown_v7QcBXR48L8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("v7QcBXR48L8", libSceGnmDriverForNeoModeUnknown_v7QcBXR48L8_nid_no_patch);

}
