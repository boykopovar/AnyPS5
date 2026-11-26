#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Apr/include/AmmExportCommands.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstring>

extern "C" {

int APS5_VABI sceKernelCallIndirectBuffer(std::uint32_t priority, void* base, std::uint64_t bytes) {
    CallIndirectBuffer(__func__, priority, base, bytes);
    return 0;
}

int APS5_VABI sceKernelCallIndirectBuffer2(std::uint32_t priority, void* base, std::uint64_t bytes, KernelMapperSubmitResult* result, std::uint32_t* id) {
    const auto submitted = CallIndirectBuffer(__func__, priority, base, bytes);
    if (result) *result = {0, 0};
    if (id) *id = submitted;
    return 0;
}

int APS5_VABI sceKernelCallIndirectBuffer3(std::uint32_t priority, void* base, std::uint64_t bytes, std::uint32_t* id) {
    const auto submitted = CallIndirectBuffer(__func__, priority, base, bytes);
    if (id) *id = submitted;
    return 0;
}

int APS5_VABI sceKernelGiveDirectMemoryToMapper(std::int64_t searchStart, std::int64_t searchEnd, std::size_t size, std::size_t alignment, int usage, std::int64_t* offset) {
    if (size == 0u || ((size | alignment) & MapperGranuleMask) != 0u) return SCE_KERNEL_ERROR_EINVAL;
    return AmmGiveDirectMemory_nid_no_patch(searchStart, searchEnd, size, alignment, usage, offset);
}

int APS5_VABI sceKernelMapperGetParam(std::uint64_t* param) {
    if (!param) return SCE_KERNEL_ERROR_EINVAL;
    std::uint64_t bytes = 0;
    if (!GuestCopyIn_nid_no_patch(&bytes, param, sizeof(bytes))) return SCE_KERNEL_ERROR_EFAULT;
    if (bytes > MapperParamBytes) return SCE_KERNEL_ERROR_EINVAL;
    std::uint64_t values[7]{MapperParamBytes};
    AmmVirtualAddressRanges_nid_no_patch(&values[1], &values[2], &values[3], &values[4]);
    if (!GuestCopyOut_nid_no_patch(param, values, static_cast<std::size_t>(bytes))) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceKernelWaitCommandBufferCompletion(std::uint32_t id) {
    return AmmSubmitted_nid_no_patch(id) ? 0 : SCE_KERNEL_ERROR_ESRCH;
}

int APS5_VABI sceKernelWriteEventQueueOnCompletionCommand(void* dst, std::uint64_t equeue, std::int32_t ident, std::uint64_t data, std::uint64_t* outSize) {
    if (equeue > EqueueLimit) return SCE_KERNEL_ERROR_EINVAL;
    return WriteCommand(dst, Apr::WriteKernelEventQueueCommand{{Apr::Opcode::WriteKernelEventQueue, sizeof(Apr::WriteKernelEventQueueCommand)}, equeue & GuestAddressMask, static_cast<std::uint64_t>(ident), data, 0}, outSize);
}

int APS5_VABI sceKernelWriteMapCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t* outSize) {
    return WriteAmmCommand({protection}, Apr::EncodeAmmMap(Apr::Opcode::AmmMap, address, 0, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteMapCommand2(void* dst, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t prt, std::uint64_t* outSize) {
    if (prt == 0) return sceKernelWriteMapCommand(dst, address, size, static_cast<std::uint32_t>(type), static_cast<std::uint32_t>(protection), outSize);
    if (!InsideAmmMapRange(address, size)) return SCE_KERNEL_ERROR_EINVAL;
    if (type != 0 || protection != 0) NotImplemented_nid_no_patch("sceKernelWriteMapCommand2: a PRT map with a type or protection");
    return WriteCommand(dst, Apr::AmmUnmapCommand{{Apr::Opcode::AmmMapAsPrt, sizeof(Apr::AmmUnmapCommand)}, address & GuestAddressMask, size & AmmSizeMask}, outSize);
}

int APS5_VABI sceKernelWriteMapDirectCommand(void* dst, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t* outSize) {
    return WriteAmmCommand({protection}, Apr::EncodeAmmMap(Apr::Opcode::AmmMapDirect, address, directOffset, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteMapDirectWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection}, Apr::EncodeAmmMap(Apr::Opcode::AmmMapDirect, address, directOffset, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteMapWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection}, Apr::EncodeAmmMap(Apr::Opcode::AmmMap, address, 0, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteModifyMtypeProtectWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t mask, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection, mask}, Apr::EncodeAmmProtect(Apr::Opcode::AmmModifyMtypeProtect, address, size, static_cast<std::int32_t>(type), static_cast<std::int32_t>(protection), static_cast<std::int32_t>(mask)), dst, outSize);
}

int APS5_VABI sceKernelWriteModifyProtectCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t protection, std::uint64_t mask, std::uint64_t* outSize) {
    return WriteAmmCommand({protection, mask}, Apr::EncodeAmmProtect(Apr::Opcode::AmmModifyProtect, address, size, 0, static_cast<std::int32_t>(protection), static_cast<std::int32_t>(mask)), dst, outSize);
}

int APS5_VABI sceKernelWriteModifyProtectWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t protection, std::uint64_t mask, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection, mask}, Apr::EncodeAmmProtect(Apr::Opcode::AmmModifyProtect, address, size, 0, static_cast<std::int32_t>(protection), static_cast<std::int32_t>(mask)), dst, outSize);
}

int APS5_VABI sceKernelWriteMultiMapCommand(void* dst, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::uint64_t protection, std::uint64_t* outSize) {
    return WriteAmmCommand({protection}, Apr::EncodeAmmRemap(Apr::Opcode::AmmMultiMap, address, alias, size, static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteMultiMapWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection}, Apr::EncodeAmmRemap(Apr::Opcode::AmmMultiMap, address, alias, size, static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteRemapCommand(void* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::uint64_t protection, std::uint64_t* outSize) {
    return WriteAmmCommand({protection}, Apr::EncodeAmmRemap(Apr::Opcode::AmmRemap, address, source, size, static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteRemapWithGpuMaskIdCommand(void* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::uint64_t protection, std::uint64_t gpuMaskId, std::uint64_t* outSize) {
    (void)gpuMaskId;
    return WriteAmmCommand({protection}, Apr::EncodeAmmRemap(Apr::Opcode::AmmRemap, address, source, size, static_cast<std::int32_t>(protection)), dst, outSize);
}

int APS5_VABI sceKernelWriteUnmapCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t* outSize) {
    return WriteAmmCommand({}, Apr::EncodeAmmUnmap(address, size), dst, outSize);
}

int APS5_VABI sceKernelWriteUnmapToPrtCommand(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t* outSize) {
    if (!InsideAmmMapRange(address, size)) return SCE_KERNEL_ERROR_EINVAL;
    return WriteCommand(dst, Apr::AmmUnmapCommand{{Apr::Opcode::AmmUnmapToPrt, sizeof(Apr::AmmUnmapCommand)}, address & GuestAddressMask, size & AmmSizeMask}, outSize);
}

int APS5_VABI sceKernelWriteWaitOnAddressCommand(void* dst, std::uint64_t address, std::uint64_t reference, std::int32_t compare, std::int32_t flush, std::uint64_t* outSize) {
    (void)flush;
    if (compare > 3) return SCE_KERNEL_ERROR_EINVAL;
    if (dst && (address & 7u) != 0u) NotImplemented_nid_no_patch("sceKernelWriteWaitOnAddressCommand: a wait on an unaligned address");
    return WriteCommand(dst, Apr::WaitCommand{{Apr::Opcode::WaitOnAddress, sizeof(Apr::WaitCommand)}, address & GuestAddressMask, reference, ~0ull, 0, static_cast<std::uint8_t>(compare & 7), Apr::CounterAccess::Size8, 0}, outSize);
}

int APS5_VABI sceKernelWriteWaitOnCounterCommand(void* dst, std::uint8_t counter, std::uint32_t reference, std::int32_t compare, std::int32_t flush, std::uint64_t* outSize) {
    (void)flush;
    if (compare > 3) return SCE_KERNEL_ERROR_EINVAL;
    return WriteCommand(dst, Apr::WaitCommand{{Apr::Opcode::WaitOnCounter, sizeof(Apr::WaitCommand)}, 0, reference, ~0ull, counter, static_cast<std::uint8_t>(compare & 7), Apr::CounterAccess::Size4, 0}, outSize);
}

int APS5_VABI sceKernelWriteWriteAddressCommand(void* dst, std::uint64_t address, std::uint64_t value, std::uint64_t* outSize) {
    return WriteCommand(dst, Apr::WriteAddressCommand{{Apr::Opcode::WriteAddress, sizeof(Apr::WriteAddressCommand)}, address & GuestAddressMask & ~std::uint64_t{7}, value, 0, 0}, outSize);
}

int APS5_VABI sceKernelWriteWriteCounterCommand(void* dst, std::uint8_t counter, std::uint32_t value, std::uint64_t* outSize) {
    return WriteCommand(dst, Apr::WriteCounterCommand{{Apr::Opcode::WriteCounter, sizeof(Apr::WriteCounterCommand)}, counter, Apr::CounterAccess::Size4, Apr::CounterOperation::Store, 0, value}, outSize);
}

}
