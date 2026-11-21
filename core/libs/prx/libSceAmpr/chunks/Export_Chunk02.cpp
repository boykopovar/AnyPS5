extern "C" {
#include "prx/libc/include/Api.hpp"
#include "prx/libSceAmpr/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <ctime>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

static constexpr std::size_t MaxNopPayloadBytes = 60u;
static constexpr int SCE_KERNEL_ERROR_EFAULT = 0x8002000E;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int SCE_KERNEL_ERROR_ESRCH = 0x80020003;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;

namespace {

struct ErrnoGuard {
    int saved{errno};
    ~ErrnoGuard() { errno = saved; }
};

template <typename T>
bool CopyGuestIn(T& dst, const void* src) {
    const ErrnoGuard guard;
    return GuestCopyIn_nid_no_patch(&dst, src, sizeof(T));
}

template <typename T>
bool CopyGuestOut(void* dst, const T& src) {
    const ErrnoGuard guard;
    return GuestCopyOut_nid_no_patch(dst, &src, sizeof(T));
}

bool CopyGuestInBytes(void* dst, const void* src, std::size_t bytes) {
    const ErrnoGuard guard;
    return GuestCopyIn_nid_no_patch(dst, src, bytes);
}

bool CopyGuestOutBytes(void* dst, const void* src, std::size_t bytes) {
    const ErrnoGuard guard;
    return GuestCopyOut_nid_no_patch(dst, src, bytes);
}

bool CopyGuestCString(std::string& dst, const char* src, std::size_t maximum) {
    const ErrnoGuard guard;
    return CopyGuestString_nid_no_patch(src, maximum, dst) == 0;
}

}

int APS5_VABI sceAmprCommandBufferConstructMarker(Apr::CommandBufferObject* buffer, std::uint32_t type, const char* text, const std::uint32_t* color) {
    switch (type) {
        case 1: return AppendMarker(buffer, Apr::Opcode::SetMarker, text);
        case 2: return AppendMarker(buffer, Apr::Opcode::PushMarker, text);
        case 3: return AppendCommand(buffer, Apr::Opcode::PopMarker, Apr::MarkerCommand{});
        case 5: {
            if (!color) return SCE_KERNEL_ERROR_EINVAL;
            std::uint32_t stagedColor = 0;
            if (!CopyGuestIn(stagedColor, color)) return SCE_KERNEL_ERROR_EFAULT;
            return AppendMarker(buffer, Apr::Opcode::SetMarker, text);
        }
        case 6: {
            if (!color) return SCE_KERNEL_ERROR_EINVAL;
            std::uint32_t stagedColor = 0;
            if (!CopyGuestIn(stagedColor, color)) return SCE_KERNEL_ERROR_EFAULT;
            return AppendMarker(buffer, Apr::Opcode::PushMarker, text);
        }
        default: return SCE_KERNEL_ERROR_EINVAL;
    }
}

int APS5_VABI sceAmprCommandBufferConstructNop(Apr::CommandBufferObject* buffer, std::int16_t type, const void* payload, std::uint32_t payloadBytes, const std::uint32_t* word) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (payloadBytes > (word ? 56u : 60u)) return SCE_KERNEL_ERROR_EINVAL;
    if (payloadBytes != 0u && !payload) throw std::invalid_argument("sceAmprCommandBufferConstructNop: null payload");
    const std::uint32_t dwords = (payloadBytes + 3u) / 4u + (word ? 1u : 0u);
    std::array<std::uint8_t, MaxNopPayloadBytes> staged{};
    std::uint32_t stagedWord = 0;
    if (word && !CopyGuestIn(stagedWord, word)) return SCE_KERNEL_ERROR_EFAULT;
    if (payloadBytes != 0u && !CopyGuestInBytes(staged.data(), payload, payloadBytes)) return SCE_KERNEL_ERROR_EFAULT;
    const std::uint32_t offset = buffer->offset;
    const int result = AppendNop(*buffer, Apr::NopHeader(dwords, static_cast<std::uint32_t>(type)), nullptr);
    if (result != 0) return result;
    std::uint8_t* data = buffer->base + offset + Apr::NopHeaderBytes;
    if (word) {
        std::memcpy(data, &stagedWord, sizeof(stagedWord));
        data += sizeof(stagedWord);
    }
    if (payloadBytes != 0u) std::memcpy(data, staged.data(), payloadBytes);
    std::memset(data + payloadBytes, 0, (4u - payloadBytes % 4u) % 4u);
    return 0;
}

int APS5_VABI sceAmprCommandBufferConstructor(Apr::CommandBufferObject* buffer) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    ReleaseExpandedCommandBuffer(buffer);
    const Apr::CommandBufferObject fresh{Apr::BufferType::Generic, 0, 0, 0, 0, nullptr};
    if (!CopyGuestOut(buffer, fresh)) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceAmprCommandBufferDestructor(Apr::CommandBufferObject* buffer) {
    if (buffer) ReleaseExpandedCommandBuffer(buffer);
    return 0;
}

void* APS5_VABI sceAmprCommandBufferGetBufferBaseAddress(const Apr::CommandBufferObject* buffer) {
    if (!buffer) return nullptr;
    Apr::CommandBufferObject local{};
    return CopyGuestIn(local, buffer) ? local.base : nullptr;
}

uint32_t APS5_VABI sceAmprCommandBufferGetCurrentOffset(const Apr::CommandBufferObject* buffer) {
    if (!buffer) return 0;
    Apr::CommandBufferObject local{};
    return CopyGuestIn(local, buffer) ? local.offset : 0;
}

uint32_t APS5_VABI sceAmprCommandBufferGetNumCommands(const Apr::CommandBufferObject* buffer) {
    if (!buffer) return 0;
    Apr::CommandBufferObject local{};
    return CopyGuestIn(local, buffer) ? local.numCommands : 0;
}

uint32_t APS5_VABI sceAmprCommandBufferGetSize(const Apr::CommandBufferObject* buffer) {
    if (!buffer) return 0;
    Apr::CommandBufferObject local{};
    return CopyGuestIn(local, buffer) ? local.size : 0;
}

uint32_t APS5_VABI sceAmprCommandBufferGetType(const Apr::CommandBufferObject* buffer) {
    if (!buffer) return 0;
    Apr::CommandBufferObject local{};
    return CopyGuestIn(local, buffer) ? static_cast<uint32_t>(local.type) : 0;
}

int APS5_VABI sceAmprCommandBufferNop(Apr::CommandBufferObject* buffer, std::uint32_t dwords) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (dwords == 0u || dwords > 16u) return SCE_KERNEL_ERROR_EINVAL;
    return AppendNop(*buffer, Apr::NopHeader(dwords - 1u), nullptr);
}

int APS5_VABI sceAmprCommandBufferNopWithData(Apr::CommandBufferObject* buffer, std::uint32_t dwords, const std::uint32_t* data) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (dwords > 15u) return SCE_KERNEL_ERROR_EINVAL;
    return AppendNop(*buffer, Apr::NopHeader(dwords), data);
}

int APS5_VABI sceAmprCommandBufferPopMarker(Apr::CommandBufferObject* buffer) {
    return AppendCommand(buffer, Apr::Opcode::PopMarker, Apr::MarkerCommand{});
}

int APS5_VABI sceAmprCommandBufferPushMarker(Apr::CommandBufferObject* buffer, const char* text) {
    return AppendMarker(buffer, Apr::Opcode::PushMarker, text);
}

int APS5_VABI sceAmprCommandBufferPushMarkerWithColor(Apr::CommandBufferObject* buffer, const char* text, std::uint32_t color) {
    (void)color;
    return AppendMarker(buffer, Apr::Opcode::PushMarker, text);
}

int APS5_VABI sceAmprCommandBufferReset(Apr::CommandBufferObject* buffer) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    Apr::CommandBufferObject localBuffer{};
    if (!CopyGuestIn(localBuffer, buffer)) return SCE_KERNEL_ERROR_EFAULT;
    localBuffer.offset = 0;
    localBuffer.numCommands = 0;
    if (!CopyGuestOutBytes(&buffer->offset, &localBuffer.offset, sizeof(std::uint32_t) * 2)) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceAmprCommandBufferSetBuffer(Apr::CommandBufferObject* buffer, void* memory, uint32_t size) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    Apr::CommandBufferObject local{};
    if (!CopyGuestIn(local, buffer)) return SCE_KERNEL_ERROR_EFAULT;
    ReleaseExpandedCommandBuffer(buffer);
    if (!CopyGuestIn(local, buffer)) return SCE_KERNEL_ERROR_EFAULT;
    ReleaseExpandedCommandBufferByOriginalBase(memory);
    local.base = static_cast<uint8_t*>(memory);
    local.size = size;
    local.offset = 0;
    local.numCommands = 0;
    if (!CopyGuestOut(buffer, local)) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceAmprCommandBufferSetMarker(Apr::CommandBufferObject* buffer, const char* text) {
    return AppendMarker(buffer, Apr::Opcode::SetMarker, text);
}

int APS5_VABI sceAmprCommandBufferSetMarkerWithColor(Apr::CommandBufferObject* buffer, const char* text, const std::uint32_t* color) {
    if (!color) return SCE_KERNEL_ERROR_EINVAL;
    std::uint32_t stagedColor = 0;
    if (!CopyGuestIn(stagedColor, color)) return SCE_KERNEL_ERROR_EFAULT;
    return AppendMarker(buffer, Apr::Opcode::SetMarker, text);
}

int APS5_VABI sceAmprCommandBufferWaitOnAddress_04_00(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint64_t reference, std::uint8_t compare, std::uint8_t flush) {
    if (!ValidWriteAddress(address) || compare > 6u || flush > 1u) return SCE_KERNEL_ERROR_EINVAL;
    return AppendCommand(buffer, Apr::Opcode::WaitOnAddress, Apr::WaitCommand{{}, reinterpret_cast<std::uint64_t>(address), reference, ~0ull, 0, compare});
}

int APS5_VABI sceAmprCommandBufferWaitOnCounter_04_00(Apr::CommandBufferObject* buffer, std::uint8_t counter, std::uint8_t access, std::uint64_t reference, std::uint8_t compare, std::uint8_t maskOperation, std::uint64_t mask, std::uint8_t flush) {
    if (!ValidWaitOnCounter_04_00(access, compare, maskOperation, flush)) return SCE_KERNEL_ERROR_EINVAL;
    const std::uint64_t applied = maskOperation ? mask : ~0ull;
    return AppendCommand(buffer, Apr::Opcode::WaitOnCounter, Apr::WaitCommand{{}, 0, reference, applied, counter, compare, static_cast<Apr::CounterAccess>(access), 0});
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromCounterPair_04_00(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint8_t counter, std::uint64_t atStart) {
    return RecordWriteAddressFromCounterPair(buffer, address, counter, atStart == 0u);
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromCounter_04_00(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint8_t counter, std::uint64_t atStart) {
    return RecordWriteAddressFromCounter(buffer, address, counter, atStart == 0u);
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromTimeCounter_04_00(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint64_t atStart) {
    return RecordWriteAddressFromTimeCounter(buffer, address, atStart == 0u);
}

int APS5_VABI sceAmprCommandBufferWriteAddress_04_00(Apr::CommandBufferObject* buffer, uint64_t* address, uint64_t value, uint32_t flags) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if ((reinterpret_cast<std::uintptr_t>(address) & 7u) != 0u) return SCE_KERNEL_ERROR_EINVAL;
    Apr::WriteAddressCommand command{};
    command.header = {Apr::Opcode::WriteAddress, sizeof(command)};
    command.address = reinterpret_cast<uint64_t>(address);
    command.value = value;
    command.flags = flags;
    return Append(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprCommandBufferWriteCounter_04_00(Apr::CommandBufferObject* buffer, std::uint8_t counter, std::uint8_t access, std::uint64_t value, std::uint8_t operation, std::uint8_t atStart) {
    if (!ValidWriteCounter_04_00(counter, access, operation)) return SCE_KERNEL_ERROR_EINVAL;
    if (atStart == 0u && InMap(buffer)) return SCE_KERNEL_ERROR_EPERM;
    return AppendCommand(buffer, Apr::Opcode::WriteCounter, Apr::WriteCounterCommand{{}, counter, static_cast<Apr::CounterAccess>(access), static_cast<Apr::CounterOperation>(operation), 0, value});
}

int APS5_VABI sceAmprCommandBufferWriteKernelEventQueue_04_00(Apr::CommandBufferObject* buffer, std::uint64_t equeue, std::int32_t ident, std::uint64_t data, std::uint64_t atStart) {
    return RecordWriteKernelEventQueue(buffer, equeue, ident, data, atStart == 0u);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeMapBegin(std::uint64_t address, std::uint64_t size, std::uint32_t type, std::uint32_t protection) {
    (void)type;
    if (!ValidMap(address, 0, size, static_cast<std::int32_t>(protection))) return MeasureInvalid;
    return sizeof(Apr::AmmMapCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeMapDirectBegin(std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::uint32_t type, std::uint32_t protection) {
    (void)type;
    if (!ValidMap(address, directOffset, size, static_cast<std::int32_t>(protection))) return MeasureInvalid;
    return sizeof(Apr::AmmMapCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeMapEnd() {
    return sizeof(Apr::MapEndCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeNop(std::uint32_t dwords) {
    if (dwords == 0u || dwords > 16u) return MeasureInvalid;
    return NopBytes(dwords);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeNopWithData(std::uint32_t dwords) {
    if (dwords == 0u || dwords > 16u) return MeasureInvalid;
    return NopBytes(dwords);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizePopMarker() {
    return sizeof(Apr::MarkerCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizePushMarker(const char* text) {
    return MeasureMarker(text);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizePushMarkerWithColor(const char* text, std::uint32_t color) {
    (void)color;
    return MeasureMarker(text);
}

uint32_t APS5_VABI sceAmprMeasureCommandSizeReadFile(void) {
    return sizeof(Apr::ReadFileCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeReadFileGather(std::uint64_t size, std::uint64_t offset) {
    if (!ValidReadLength(size) || !ValidReadOffset(offset)) return MeasureInvalid;
    return sizeof(Apr::CompactReadFileGatherCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeReadFileGatherScatter(void* destination, std::uint64_t size, std::uint64_t offset) {
    if (!ValidRead(destination, size, offset)) return MeasureInvalid;
    return sizeof(Apr::ReadFileCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeReadFileScatter(void* destination, std::uint64_t size) {
    if (!ValidRead(destination, size, 0)) return MeasureInvalid;
    return sizeof(Apr::ReadFileCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeResetGatherScatterState() {
    return sizeof(Apr::ResetGatherScatterStateCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeSetMarker(const char* text) {
    return MeasureMarker(text);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeSetMarkerWithColor(const char* text, std::uint32_t color) {
    (void)color;
    return MeasureMarker(text);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnAddress_04_00(volatile std::uint64_t* address, std::uint64_t, std::uint8_t compare, std::uint8_t flush) {
    if (!ValidWriteAddress(address) || compare > 6u || flush > 1u) return MeasureInvalid;
    return sizeof(Apr::WaitCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnCounter_04_00(std::uint8_t, std::uint8_t access, std::uint64_t, std::uint8_t compare, std::uint8_t maskOperation, std::uint64_t, std::uint8_t flush) {
    if (!ValidWaitOnCounter_04_00(access, compare, maskOperation, flush)) return MeasureInvalid;
    return sizeof(Apr::WaitCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounterPair_04_00(volatile std::uint64_t* address, std::uint8_t counter) {
    if (!ValidWriteAddress(address) || (counter & 1u) != 0u) return MeasureInvalid;
    return sizeof(Apr::WriteAddressFromCounterCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounter_04_00(volatile std::uint64_t* address, std::uint8_t counter) {
    (void)counter;
    if (!ValidWriteAddress(address)) return MeasureInvalid;
    return sizeof(Apr::WriteAddressFromCounterCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromTimeCounter_04_00(volatile std::uint64_t* address) {
    if (!ValidWriteAddress(address)) return MeasureInvalid;
    return sizeof(Apr::WriteAddressFromCounterCommand);
}

uint32_t APS5_VABI sceAmprMeasureCommandSizeWriteAddress_04_00(void) {
    return sizeof(Apr::WriteAddressCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteCounter_04_00(std::uint8_t counter, std::uint8_t access, std::uint64_t, std::uint8_t operation) {
    if (!ValidWriteCounter_04_00(counter, access, operation)) return MeasureInvalid;
    return sizeof(Apr::WriteCounterCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteKernelEventQueue_04_00(std::uint64_t, std::int32_t, std::uint64_t) {
    return sizeof(Apr::WriteKernelEventQueueCommand);
}

int APS5_VABI sceAmprAmmCommandBufferConstructor(Apr::CommandBufferObject* buffer) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    ReleaseExpandedCommandBuffer(buffer);
    return 0;
}

int APS5_VABI sceAmprAmmCommandBufferDestructor(Apr::CommandBufferObject* buffer) {
    if (buffer) ReleaseExpandedCommandBuffer(buffer);
    return 0;
}

int APS5_VABI sceAmprAmmCommandBufferMap(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    return RecordAmmMap(buffer, Apr::Opcode::AmmMap, address, 0, size, type, protection);
}

int APS5_VABI sceAmprAmmCommandBufferMapWithGpuMaskId(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return RecordAmmMap(buffer, Apr::Opcode::AmmMap, address, 0, size, type, protection);
}

int APS5_VABI sceAmprAmmCommandBufferMapDirect(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    return RecordAmmMap(buffer, Apr::Opcode::AmmMapDirect, address, directOffset, size, type, protection);
}

int APS5_VABI sceAmprAmmCommandBufferMapDirectWithGpuMaskId(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::int32_t type, std::int32_t protection, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return RecordAmmMap(buffer, Apr::Opcode::AmmMapDirect, address, directOffset, size, type, protection);
}

int APS5_VABI sceAmprAmmCommandBufferUnmap(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    const auto command = Apr::EncodeAmmUnmap(address, size);
    if (!command) return SCE_KERNEL_ERROR_EINVAL;
    return AppendAmm(buffer, &*command, sizeof(*command));
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMap(std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    (void)type;
    return ValidMap(address, 0, size, protection) ? std::int64_t{sizeof(Apr::AmmMapCommand)} : AmmMeasureInvalid;
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMapWithGpuMaskId(std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return sceAmprAmmMeasureAmmCommandSizeMap(address, size, type, protection);
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMapDirect(std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    (void)type;
    return ValidMap(address, directOffset, size, protection) ? std::int64_t{sizeof(Apr::AmmMapCommand)} : AmmMeasureInvalid;
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMapDirectWithGpuMaskId(std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::int32_t type, std::int32_t protection, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return sceAmprAmmMeasureAmmCommandSizeMapDirect(address, directOffset, size, type, protection);
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeUnmap(std::uint64_t address, std::uint64_t size) {
    return ValidMapRange(address, size) ? std::int64_t{sizeof(Apr::AmmUnmapCommand)} : AmmMeasureInvalid;
}

int APS5_VABI sceAmprAmmCommandBufferRemap(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::int32_t protection) {
    return RecordAmmRemap(buffer, Apr::Opcode::AmmRemap, address, source, size, protection);
}

int APS5_VABI sceAmprAmmCommandBufferRemapWithGpuMaskId(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::int32_t protection, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return RecordAmmRemap(buffer, Apr::Opcode::AmmRemap, address, source, size, protection);
}

int APS5_VABI sceAmprAmmCommandBufferMultiMap(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::int32_t protection) {
    return RecordAmmRemap(buffer, Apr::Opcode::AmmMultiMap, address, alias, size, protection);
}

int APS5_VABI sceAmprAmmCommandBufferMultiMapWithGpuMaskId(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::int32_t protection, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return RecordAmmRemap(buffer, Apr::Opcode::AmmMultiMap, address, alias, size, protection);
}

int APS5_VABI sceAmprAmmCommandBufferModifyProtect(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size, std::int32_t protection, std::int32_t mask) {
    return RecordAmmProtect(buffer, Apr::Opcode::AmmModifyProtect, address, size, 0, protection, mask);
}

int APS5_VABI sceAmprAmmCommandBufferModifyProtectWithGpuMaskId(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size, std::int32_t protection, std::int32_t mask, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return RecordAmmProtect(buffer, Apr::Opcode::AmmModifyProtect, address, size, 0, protection, mask);
}

int APS5_VABI sceAmprAmmCommandBufferModifyMtypeProtect(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t mask) {
    return RecordAmmProtect(buffer, Apr::Opcode::AmmModifyMtypeProtect, address, size, type, protection, mask);
}

int APS5_VABI sceAmprAmmCommandBufferModifyMtypeProtectWithGpuMaskId(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t mask, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return RecordAmmProtect(buffer, Apr::Opcode::AmmModifyMtypeProtect, address, size, type, protection, mask);
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeRemap(std::uint64_t address, std::uint64_t source, std::uint64_t size, std::int32_t protection) {
    return ValidRemap(address, source, size, protection) ? std::int64_t{sizeof(Apr::AmmRemapCommand)} : AmmMeasureInvalid;
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeRemapWithGpuMaskId(std::uint64_t address, std::uint64_t source, std::uint64_t size, std::int32_t protection, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return sceAmprAmmMeasureAmmCommandSizeRemap(address, source, size, protection);
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMultiMap(std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::int32_t protection) {
    return ValidRemap(address, alias, size, protection) ? std::int64_t{sizeof(Apr::AmmRemapCommand)} : AmmMeasureInvalid;
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMultiMapWithGpuMaskId(std::uint64_t address, std::uint64_t alias, std::uint64_t size, std::int32_t protection, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return sceAmprAmmMeasureAmmCommandSizeMultiMap(address, alias, size, protection);
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyProtect(std::uint64_t address, std::uint64_t size, std::int32_t protection, std::int32_t mask) {
    return ValidProtect(address, size, protection, mask) ? std::int64_t{sizeof(Apr::AmmProtectCommand)} : AmmMeasureInvalid;
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyProtectWithGpuMaskId(std::uint64_t address, std::uint64_t size, std::int32_t protection, std::int32_t mask, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return sceAmprAmmMeasureAmmCommandSizeModifyProtect(address, size, protection, mask);
}

}
