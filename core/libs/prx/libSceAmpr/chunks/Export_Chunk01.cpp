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

static std::uint32_t RemainingCommandBytes(const Apr::CommandBufferObject& buffer) {
    if (buffer.offset > buffer.size) throw std::invalid_argument("AMPR: command buffer offset exceeds bound memory");
    return buffer.size - buffer.offset;
}

struct ExpandedCommandBuffer {
    std::uint8_t* originalBase;
    std::uint32_t originalSize;
    std::uint8_t* allocation;
};

static std::mutex g_expandedCommandBuffersMutex;
static std::unordered_map<Apr::CommandBufferObject*, ExpandedCommandBuffer> g_expandedCommandBuffers;
static constexpr std::uint32_t MAX_EXPANDED_BUFFER_CAPACITY = 32u * 1024u * 1024u;

static int EnsureCommandCapacity(Apr::CommandBufferObject* buffer, std::uint32_t bytes) {
    RemainingCommandBytes(*buffer);
    const std::uint64_t required = static_cast<std::uint64_t>(buffer->offset) + bytes;
    if (required <= buffer->size) return 0;
    if (required > MAX_EXPANDED_BUFFER_CAPACITY) return SCE_KERNEL_ERROR_EBUSY;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    const std::uint64_t capacity = std::min<std::uint64_t>(
        MAX_EXPANDED_BUFFER_CAPACITY,
        std::max(static_cast<std::uint64_t>(buffer->size) * 2u, required + 256u));
    if (required > capacity) return SCE_KERNEL_ERROR_EBUSY;
    const auto newSize = static_cast<std::uint32_t>(capacity);
    const std::lock_guard lock(g_expandedCommandBuffersMutex);
    auto expanded = g_expandedCommandBuffers.find(buffer);
    if (expanded == g_expandedCommandBuffers.end()) {
        void* raw = nullptr;
        try {
            raw = GuestHeap::GuestHeapAllocate_nid_postfix(newSize);
        } catch (const std::bad_alloc&) {
            return SCE_KERNEL_ERROR_ENOMEM;
        }
        if (!raw) return SCE_KERNEL_ERROR_ENOMEM;
        std::unique_ptr<std::uint8_t, decltype(&GuestHeap::GuestHeapFree_nid_postfix)> allocation(
            static_cast<std::uint8_t*>(raw),
            GuestHeap::GuestHeapFree_nid_postfix);
        std::memcpy(allocation.get(), buffer->base, buffer->offset);
        const auto [inserted, created] = g_expandedCommandBuffers.emplace(
            buffer, ExpandedCommandBuffer{buffer->base, buffer->size, allocation.get()});
        if (!created) throw std::logic_error("AMPR: command buffer expansion state already exists");
        buffer->base = inserted->second.allocation;
        buffer->size = newSize;
        allocation.release();
        return 0;
    }
    void* raw = nullptr;
    try {
        raw = GuestHeap::GuestHeapReallocate_nid_postfix(expanded->second.allocation, newSize);
    } catch (const std::bad_alloc&) {
        return SCE_KERNEL_ERROR_ENOMEM;
    }
    if (!raw) return SCE_KERNEL_ERROR_ENOMEM;
    auto* allocation = static_cast<std::uint8_t*>(raw);
    expanded->second.allocation = allocation;
    buffer->base = allocation;
    buffer->size = newSize;
    return 0;
}

static bool ReleaseExpandedCommandBuffer(Apr::CommandBufferObject* buffer) {
    const std::lock_guard lock(g_expandedCommandBuffersMutex);
    const auto expanded = g_expandedCommandBuffers.find(buffer);
    if (expanded == g_expandedCommandBuffers.end()) return false;
    GuestHeap::GuestHeapFree_nid_postfix(expanded->second.allocation);
    buffer->base = expanded->second.originalBase;
    buffer->size = expanded->second.originalSize;
    buffer->offset = 0;
    buffer->numCommands = 0;
    g_expandedCommandBuffers.erase(expanded);
    return true;
}

static bool ReleaseExpandedCommandBufferByOriginalBase(const void* base) {
    if (!base) return false;
    const std::lock_guard lock(g_expandedCommandBuffersMutex);
    for (auto it = g_expandedCommandBuffers.begin(); it != g_expandedCommandBuffers.end(); ++it) {
        if (it->second.originalBase == base) {
            GuestHeap::GuestHeapFree_nid_postfix(it->second.allocation);
            auto* oldBuffer = it->first;
            oldBuffer->base = it->second.originalBase;
            oldBuffer->size = it->second.originalSize;
            oldBuffer->offset = 0;
            oldBuffer->numCommands = 0;
            g_expandedCommandBuffers.erase(it);
            return true;
        }
    }
    return false;
}

static void* ResolveBufferBase(void* base) {
    if (!base) return nullptr;
    const std::lock_guard lock(g_expandedCommandBuffersMutex);
    for (const auto& [buffer, expanded] : g_expandedCommandBuffers) {
        if (expanded.originalBase == base) return expanded.allocation;
    }
    return base;
}

static int Append(Apr::CommandBufferObject* buffer, const void* command, uint32_t bytes) {
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    const int capacity = EnsureCommandCapacity(buffer, bytes);
    if (capacity != 0) return capacity;
    std::memcpy(buffer->base + buffer->offset, command, bytes);
    buffer->offset += bytes;
    ++buffer->numCommands;
    return 0;
}

template<class TCommand>
static int AppendCommand(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, TCommand command) {
    if (!buffer) return static_cast<int>(0x80020016);
    command.header = {opcode, sizeof(command)};
    return Append(buffer, &command, sizeof(command));
}

static bool ValidCounter(std::uint32_t counter) {
    return counter < 128u;
}

static bool ValidWriteAddress(volatile std::uint64_t* address) {
    return address && (reinterpret_cast<std::uintptr_t>(address) & 7u) == 0u;
}

static bool ValidWait(std::uint32_t compare, std::uint32_t flush) {
    return compare < 4u && flush < 2u;
}

static bool ValidWaitOnCounter_04_00(std::uint32_t access, std::uint32_t compare, std::uint32_t maskOperation, std::uint32_t flush) {
    return access < 8u && compare <= 6u && maskOperation < 2u && flush < 2u;
}

static bool ValidWriteCounter_04_00(std::uint32_t counter, std::uint32_t access, std::uint32_t operation) {
    return (counter & 0x80u) == 0u && access < 8u && operation < 5u;
}

static constexpr std::uint64_t MeasureInvalid = static_cast<std::uint32_t>(SCE_KERNEL_ERROR_EINVAL);

static bool ValidReadLength(std::uint64_t length) {
    return length != 0u && length <= 0x100000000ull;
}

static bool ValidReadOffset(std::uint64_t offset) {
    return offset < 0x10000000000ull;
}

static bool ValidRead(const void* destination, std::uint64_t length, std::uint64_t offset) {
    constexpr std::uint64_t userLimit = 0xF00000000000ull;
    const auto address = reinterpret_cast<std::uint64_t>(destination);
    return ValidReadLength(length) && address <= userLimit && length <= userLimit - address && ValidReadOffset(offset);
}

static int AppendRead(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, std::uint32_t fileId, const void* destination, std::uint64_t size, std::uint64_t offset) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (opcode != Apr::Opcode::ReadFile && (buffer->recording & Apr::ScatterGatherValid) == 0u) return SCE_KERNEL_ERROR_EINVAL;
    Apr::ReadFileCommand command{{opcode, sizeof(command)}, fileId, 0, reinterpret_cast<std::uint64_t>(destination), size, offset};
    const int result = Append(buffer, &command, sizeof(command));
    if (result == 0) buffer->recording |= Apr::ScatterGatherValid;
    return result;
}

static std::uint64_t NopBytes(std::uint32_t dwords) {
    return std::uint64_t{dwords} * 4u;
}

static int AppendNop(Apr::CommandBufferObject& buffer, std::uint32_t header, const std::uint32_t* data) {
    const std::uint64_t bytes = NopBytes(Apr::NopDwords(header));
    if (!buffer.base) return SCE_KERNEL_ERROR_EPERM;
    if (bytes > std::numeric_limits<std::uint32_t>::max()) return SCE_KERNEL_ERROR_EBUSY;
    const auto payloadBytes = static_cast<std::uint32_t>(bytes - sizeof(std::uint32_t));
    std::array<std::uint8_t, MaxNopPayloadBytes> staged{};
    if (data && payloadBytes != 0u && !CopyGuestInBytes(staged.data(), data, payloadBytes)) return SCE_KERNEL_ERROR_EFAULT;
    const int capacity = EnsureCommandCapacity(&buffer, static_cast<std::uint32_t>(bytes));
    if (capacity != 0) return capacity;
    std::uint8_t* destination = buffer.base + buffer.offset;
    std::memcpy(destination, &header, sizeof(header));
    if (data) std::memcpy(destination + sizeof(header), staged.data(), payloadBytes);
    buffer.offset += static_cast<std::uint32_t>(bytes);
    ++buffer.numCommands;
    return 0;
}

static std::uint64_t MarkerBytesFromLength(std::size_t length) {
    return sizeof(Apr::MarkerCommand) + ((length + 8) & ~std::uint64_t{7});
}

static std::uint64_t MeasureMarker(const char* text) {
    if (!text) return static_cast<std::uint32_t>(SCE_KERNEL_ERROR_EINVAL);
    std::string staged;
    if (!CopyGuestCString(staged, text, MAX_EXPANDED_BUFFER_CAPACITY)) return MeasureInvalid;
    return MarkerBytesFromLength(staged.size());
}

static int AppendMarker(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, const char* text) {
    if (!buffer || !text) return SCE_KERNEL_ERROR_EINVAL;
    std::string staged;
    if (!CopyGuestCString(staged, text, MAX_EXPANDED_BUFFER_CAPACITY)) return SCE_KERNEL_ERROR_EFAULT;
    const std::uint64_t bytes = MarkerBytesFromLength(staged.size());
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (bytes > std::numeric_limits<std::uint32_t>::max()) return SCE_KERNEL_ERROR_EBUSY;
    const int capacity = EnsureCommandCapacity(buffer, static_cast<std::uint32_t>(bytes));
    if (capacity != 0) return capacity;
    const Apr::MarkerCommand command{{opcode, static_cast<std::uint32_t>(bytes)}};
    std::uint8_t* destination = buffer->base + buffer->offset;
    std::memset(destination, 0, bytes);
    std::memcpy(destination, &command, sizeof(command));
    std::memcpy(destination + sizeof(command), staged.c_str(), staged.size());
    buffer->offset += static_cast<std::uint32_t>(bytes);
    ++buffer->numCommands;
    return 0;
}

static bool InMap(const Apr::CommandBufferObject* buffer) {
    return buffer && (buffer->recording & Apr::MapActive) != 0u;
}

using Apr::ValidMapRange;
using Apr::ValidMap;
using Apr::ValidAmmProtection;
using Apr::ValidRemap;
using Apr::ValidProtect;

static constexpr std::int64_t AmmMeasureInvalid = SCE_KERNEL_ERROR_EINVAL;

static int AppendAmm(Apr::CommandBufferObject* buffer, const void* command, std::uint32_t bytes) {
    return Append(buffer, command, bytes);
}

static int RecordAmmMap(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    const auto command = Apr::EncodeAmmMap(opcode, address, directOffset, size, type, protection);
    if (!command) return SCE_KERNEL_ERROR_EINVAL;
    return AppendAmm(buffer, &*command, sizeof(*command));
}

static int RecordAmmRemap(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::int32_t protection) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    const auto command = Apr::EncodeAmmRemap(opcode, address, source, size, protection);
    if (!command) return SCE_KERNEL_ERROR_EINVAL;
    return AppendAmm(buffer, &*command, sizeof(*command));
}

static int RecordAmmProtect(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t mask) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    const auto command = Apr::EncodeAmmProtect(opcode, address, size, type, protection, mask);
    if (!command) return SCE_KERNEL_ERROR_EINVAL;
    return AppendAmm(buffer, &*command, sizeof(*command));
}

static constexpr std::int32_t PrtAllocationMask = 1019;

static int RecordPrt(Apr::CommandBufferObject* buffer, const void* command, std::uint32_t bytes) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (bytes > RemainingCommandBytes(*buffer) && !buffer->base) return SCE_KERNEL_ERROR_EBUSY;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    return Append(buffer, command, bytes);
}

static int RecordMapBegin(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    if (!ValidMap(address, directOffset, size, protection)) return SCE_KERNEL_ERROR_EINVAL;
    if (InMap(buffer)) return SCE_KERNEL_ERROR_EPERM;
    const int result = AppendCommand(buffer, opcode, Apr::AmmMapCommand{{}, address, directOffset, size, type, protection});
    if (result == 0) buffer->recording |= Apr::MapActive;
    return result;
}

static int RecordWriteKernelEventQueue(Apr::CommandBufferObject* buffer, std::uint64_t equeue, std::int32_t ident, std::uint64_t data, bool completion) {
    if (!equeue) return SCE_KERNEL_ERROR_EINVAL;
    if (completion && InMap(buffer)) return SCE_KERNEL_ERROR_EPERM;
    return AppendCommand(buffer, Apr::Opcode::WriteKernelEventQueue, Apr::WriteKernelEventQueueCommand{{}, equeue, static_cast<std::uint64_t>(ident), data, 0});
}

static int RecordWriteAddressFromTimeCounter(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, bool completion) {
    if (!ValidWriteAddress(address)) return SCE_KERNEL_ERROR_EINVAL;
    if (completion && InMap(buffer)) return SCE_KERNEL_ERROR_EPERM;
    return AppendCommand(buffer, Apr::Opcode::WriteAddressFromTimeCounter, Apr::WriteAddressFromCounterCommand{{}, reinterpret_cast<std::uint64_t>(address), 0, 0});
}

static int RecordWriteAddressFromCounter(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint8_t counter, bool completion) {
    if (!ValidWriteAddress(address) || !ValidCounter(counter)) return SCE_KERNEL_ERROR_EINVAL;
    if (completion && InMap(buffer)) return SCE_KERNEL_ERROR_EPERM;
    return AppendCommand(buffer, Apr::Opcode::WriteAddressFromCounter, Apr::WriteAddressFromCounterCommand{{}, reinterpret_cast<std::uint64_t>(address), counter, 0});
}

static int RecordWriteAddressFromCounterPair(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint8_t counter, bool completion) {
    if (!ValidWriteAddress(address) || (counter & 0x81u) != 0u) return SCE_KERNEL_ERROR_EINVAL;
    if (completion && InMap(buffer)) return SCE_KERNEL_ERROR_EPERM;
    return AppendCommand(buffer, Apr::Opcode::WriteAddressFromCounterPair, Apr::WriteAddressFromCounterCommand{{}, reinterpret_cast<std::uint64_t>(address), counter, counter + 1u});
}

static_assert(sizeof(AmmUsageStatsData) == 0x18);
static constexpr std::uint16_t AmmPageTablePoolEntries = 512;
static constexpr std::uint32_t AmmRingIdleFlags = 0x7;


int APS5_VABI sceAmprCommandBufferWriteAddressOnCompletion(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint64_t value) {
    if (!ValidWriteAddress(address)) return SCE_KERNEL_ERROR_EINVAL;
    if (InMap(buffer)) return SCE_KERNEL_ERROR_EPERM;
    return AppendCommand(buffer, Apr::Opcode::WriteAddress, Apr::WriteAddressCommand{{}, reinterpret_cast<std::uint64_t>(address), value, 0, 0});
}

int APS5_VABI sceAmprCommandBufferWriteCounterOnCompletion(Apr::CommandBufferObject* buffer, std::uint8_t counter, std::uint32_t value) {
    if (!ValidCounter(counter)) return SCE_KERNEL_ERROR_EINVAL;
    if (InMap(buffer)) return SCE_KERNEL_ERROR_EPERM;
    return AppendCommand(buffer, Apr::Opcode::WriteCounter, Apr::WriteCounterCommand{{}, counter, Apr::CounterAccess::Size4, Apr::CounterOperation::Store, 0, value});
}

int APS5_VABI sceAmprCommandBufferWaitOnAddress(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint64_t reference, std::uint8_t compare, std::uint8_t flush) {
    if ((reinterpret_cast<std::uintptr_t>(address) & 7u) != 0u || !ValidWait(compare, flush)) return SCE_KERNEL_ERROR_EINVAL;
    return AppendCommand(buffer, Apr::Opcode::WaitOnAddress, Apr::WaitCommand{{}, reinterpret_cast<std::uint64_t>(address), reference, ~0ull, 0, compare});
}

int APS5_VABI sceAmprCommandBufferWaitOnCounter(Apr::CommandBufferObject* buffer, std::uint8_t counter, std::uint32_t reference, std::uint8_t compare, std::uint8_t flush) {
    if (!ValidCounter(counter) || !ValidWait(compare, flush)) return SCE_KERNEL_ERROR_EINVAL;
    return AppendCommand(buffer, Apr::Opcode::WaitOnCounter, Apr::WaitCommand{{}, 0, reference, ~0ull, counter, compare, Apr::CounterAccess::Size4, 0});
}

int APS5_VABI sceAmprCommandBufferWriteKernelEventQueueOnCompletion(Apr::CommandBufferObject* buffer, std::uint64_t equeue, std::int32_t ident, std::uint64_t data) {
    return RecordWriteKernelEventQueue(buffer, equeue, ident, data, true);
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromTimeCounterOnCompletion(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address) {
    return RecordWriteAddressFromTimeCounter(buffer, address, true);
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromCounterOnCompletion(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint8_t counter) {
    return RecordWriteAddressFromCounter(buffer, address, counter, true);
}

int APS5_VABI sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(Apr::CommandBufferObject* buffer, volatile std::uint64_t* address, std::uint8_t counter) {
    return RecordWriteAddressFromCounterPair(buffer, address, counter, true);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressOnCompletion() { return sizeof(Apr::WriteAddressCommand); }
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteCounterOnCompletion() { return sizeof(Apr::WriteCounterCommand); }

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromTimeCounterOnCompletion(volatile std::uint64_t* address) {
    if (!ValidWriteAddress(address)) return MeasureInvalid;
    return sizeof(Apr::WriteAddressFromCounterCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounterOnCompletion(volatile std::uint64_t* address, std::uint8_t counter) {
    (void)counter;
    if (!ValidWriteAddress(address)) return MeasureInvalid;
    return sizeof(Apr::WriteAddressFromCounterCommand);
}

std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounterPairOnCompletion(volatile std::uint64_t* address, std::uint8_t counter) {
    if (!ValidWriteAddress(address) || (counter & 1u) != 0u) return MeasureInvalid;
    return sizeof(Apr::WriteAddressFromCounterCommand);
}
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnAddress() { return sizeof(Apr::WaitCommand); }
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnCounter() { return sizeof(Apr::WaitCommand); }
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteKernelEventQueueOnCompletion() { return sizeof(Apr::WriteKernelEventQueueCommand); }

int APS5_VABI sceAmprAprCommandBufferConstructor(Apr::CommandBufferObject* buffer, uint64_t* gatherState, uint64_t* scatterState) {
    if (!buffer || !gatherState || !scatterState) return SCE_KERNEL_ERROR_EINVAL;
    ReleaseExpandedCommandBuffer(buffer);
    buffer->type = Apr::BufferType::Apr;
    const std::uint64_t zero = 0;
    if (!CopyGuestOut(gatherState, zero) || !CopyGuestOut(scatterState, zero)) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceAmprAprCommandBufferDestructor(Apr::CommandBufferObject* buffer, uint64_t* gatherState, uint64_t* scatterState) {
    if (buffer) ReleaseExpandedCommandBuffer(buffer);
    (void)gatherState;
    (void)scatterState;
    return 0;
}

int APS5_VABI sceAmprAprCommandBufferMapBegin(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    return RecordMapBegin(buffer, Apr::Opcode::AmmMap, address, 0, size, type, protection);
}

int APS5_VABI sceAmprAprCommandBufferMapDirectBegin(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    return RecordMapBegin(buffer, Apr::Opcode::AmmMapDirect, address, directOffset, size, type, protection);
}

int APS5_VABI sceAmprAprCommandBufferMapEnd(Apr::CommandBufferObject* buffer) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!InMap(buffer)) return SCE_KERNEL_ERROR_EPERM;
    const int result = AppendCommand(buffer, Apr::Opcode::MapEnd, Apr::MapEndCommand{});
    if (result == 0) buffer->recording &= static_cast<std::uint16_t>(~Apr::MapActive);
    return result;
}

int APS5_VABI sceAmprAprCommandBufferReadFile(Apr::CommandBufferObject* buffer, uint64_t* gatherState, uint64_t* scatterState, uint32_t fileId, void* destination, uint64_t size, uint64_t offset) {
    (void)gatherState;
    (void)scatterState;
    if (!ValidRead(destination, size, offset)) return SCE_KERNEL_ERROR_EINVAL;
    return AppendRead(buffer, Apr::Opcode::ReadFile, fileId, destination, size, offset);
}

int APS5_VABI sceAmprAprCommandBufferReadFileGather(Apr::CommandBufferObject* buffer, std::uint64_t* mapState, std::uint64_t* scatterGatherState, std::uint64_t size, std::uint64_t offset) {
    (void)mapState;
    (void)scatterGatherState;
    if (!ValidReadLength(size) || !ValidReadOffset(offset)) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer || (buffer->recording & Apr::ScatterGatherValid) == 0u) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::CompactReadFileGatherCommand command{
        static_cast<std::uint32_t>(Apr::Opcode::CompactReadFileGather) | static_cast<std::uint32_t>(offset >> 32u) << 8u,
        static_cast<std::uint32_t>(size - 1u), static_cast<std::uint32_t>(offset)};
    return Append(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprAprCommandBufferReadFileGatherScatter(Apr::CommandBufferObject* buffer, std::uint64_t* mapState, std::uint64_t* scatterGatherState, void* destination, std::uint64_t size, std::uint64_t offset) {
    (void)mapState;
    (void)scatterGatherState;
    if (!ValidRead(destination, size, offset)) return SCE_KERNEL_ERROR_EINVAL;
    return AppendRead(buffer, Apr::Opcode::ReadFileGatherScatter, 0, destination, size, offset);
}

int APS5_VABI sceAmprAprCommandBufferReadFileScatter(Apr::CommandBufferObject* buffer, std::uint64_t* mapState, std::uint64_t* scatterGatherState, void* destination, std::uint64_t size) {
    (void)mapState;
    (void)scatterGatherState;
    if (!ValidRead(destination, size, 0)) return SCE_KERNEL_ERROR_EINVAL;
    return AppendRead(buffer, Apr::Opcode::ReadFileScatter, 0, destination, size, 0);
}

int APS5_VABI sceAmprAprCommandBufferResetGatherScatterState(Apr::CommandBufferObject* buffer) {
    const int result = AppendCommand(buffer, Apr::Opcode::ResetGatherScatterState, Apr::ResetGatherScatterStateCommand{});
    if (result == 0) buffer->recording &= static_cast<std::uint16_t>(~Apr::ScatterGatherValid);
    return result;
}

void* APS5_VABI sceAmprCommandBufferClearBuffer(Apr::CommandBufferObject* buffer) {
    if (!buffer) return nullptr;
    Apr::CommandBufferObject local{};
    if (!CopyGuestIn(local, buffer)) return nullptr;
    void* memory = local.base;
    if (ReleaseExpandedCommandBuffer(buffer)) {
        if (!CopyGuestIn(local, buffer)) return nullptr;
        memory = local.base;
    }
    local.base = nullptr;
    local.size = 0;
    local.offset = 0;
    local.numCommands = 0;
    if (!CopyGuestOut(buffer, local)) return nullptr;
    return memory;
}

}
