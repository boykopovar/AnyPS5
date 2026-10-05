#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include <cstring>
#include <stdexcept>

static constexpr int SCE_AMPR_ERROR_BUFFER_FULL = 0x8002001C;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int SCE_KERNEL_ERROR_ESRCH = 0x80020003;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;

static int Append(Apr::CommandBufferObject* buffer, const void* command, uint32_t bytes) {
    if (!buffer->base || bytes > buffer->size - buffer->offset) return SCE_AMPR_ERROR_BUFFER_FULL;
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
    return sizeof(Apr::CommandHeader) + std::uint64_t{dwords} * 4u;
}

static int AppendNop(Apr::CommandBufferObject* buffer, std::uint32_t dwords, const std::uint32_t* data) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    const std::uint64_t bytes = NopBytes(dwords);
    if (!buffer->base || bytes > buffer->size - buffer->offset) return SCE_AMPR_ERROR_BUFFER_FULL;
    const Apr::CommandHeader header{Apr::Opcode::Nop, static_cast<std::uint32_t>(bytes)};
    std::uint8_t* destination = buffer->base + buffer->offset;
    std::memset(destination, 0, bytes);
    std::memcpy(destination, &header, sizeof(header));
    if (data) std::memcpy(destination + sizeof(header), data, std::uint64_t{dwords} * 4u);
    buffer->offset += static_cast<std::uint32_t>(bytes);
    ++buffer->numCommands;
    return 0;
}

static std::uint64_t MarkerBytes(const char* text) {
    return sizeof(Apr::MarkerCommand) + ((std::strlen(text) + 8) & ~std::uint64_t{7});
}

static std::uint64_t MeasureMarker(const char* text) {
    if (!text) return static_cast<std::uint32_t>(SCE_KERNEL_ERROR_EINVAL);
    return MarkerBytes(text);
}

static int AppendMarker(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, const char* text) {
    if (!buffer || !text) return SCE_KERNEL_ERROR_EINVAL;
    const std::uint64_t bytes = MarkerBytes(text);
    if (!buffer->base || bytes > buffer->size - buffer->offset) return SCE_AMPR_ERROR_BUFFER_FULL;
    const Apr::MarkerCommand command{{opcode, static_cast<std::uint32_t>(bytes)}};
    std::uint8_t* destination = buffer->base + buffer->offset;
    std::memset(destination, 0, bytes);
    std::memcpy(destination, &command, sizeof(command));
    std::memcpy(destination + sizeof(command), text, std::strlen(text));
    buffer->offset += static_cast<std::uint32_t>(bytes);
    ++buffer->numCommands;
    return 0;
}

static bool InMap(const Apr::CommandBufferObject* buffer) {
    return buffer && (buffer->recording & Apr::MapActive) != 0u;
}

static bool ValidMapRange(std::uint64_t address, std::uint64_t size) {
    constexpr std::uint64_t pageMask = 0x3FFFu;
    return address != 0u && size != 0u && ((address | size) & pageMask) == 0u && address + size > address;
}

static bool ValidMap(std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::int32_t protection) {
    return ValidMapRange(address, size) && (directOffset & 0x3FFFu) == 0u && (static_cast<std::uint32_t>(protection) & 0xFFFFFC0Cu) == 0u;
}

static constexpr std::int64_t AmmMeasureInvalid = SCE_KERNEL_ERROR_EINVAL;

static int AppendAmm(Apr::CommandBufferObject* buffer, const void* command, std::uint32_t bytes) {
    if (bytes > buffer->size - buffer->offset) return SCE_KERNEL_ERROR_EBUSY;
    return Append(buffer, command, bytes);
}

static int RecordAmmMap(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, std::uint64_t address, std::uint64_t directOffset, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (!ValidMap(address, directOffset, size, protection)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmMapCommand command{{opcode, sizeof(command)}, address, directOffset, size, type, protection};
    return AppendAmm(buffer, &command, sizeof(command));
}

static bool ValidAmmProtection(std::int32_t protection) {
    return (static_cast<std::uint32_t>(protection) & 0xFFFFFC0Cu) == 0u;
}

static bool ValidRemap(std::uint64_t address, std::uint64_t source, std::uint64_t size, std::int32_t protection) {
    return ValidMapRange(address, size) && ValidMapRange(source, size) && ValidAmmProtection(protection);
}

static bool ValidProtect(std::uint64_t address, std::uint64_t size, std::int32_t protection, std::int32_t mask) {
    return ValidMapRange(address, size) && ValidAmmProtection(protection) && ValidAmmProtection(mask);
}

static int RecordAmmRemap(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::int32_t protection) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (!ValidRemap(address, source, size, protection)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmRemapCommand command{{opcode, sizeof(command)}, address, source, size, protection, 0};
    return AppendAmm(buffer, &command, sizeof(command));
}

static int RecordAmmProtect(Apr::CommandBufferObject* buffer, Apr::Opcode opcode, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t mask) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (!ValidProtect(address, size, protection, mask)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmProtectCommand command{{opcode, sizeof(command)}, address, size, type, protection, mask, 0};
    return AppendAmm(buffer, &command, sizeof(command));
}

static constexpr std::int32_t PrtAllocationMask = 1019;

static int RecordPrt(Apr::CommandBufferObject* buffer, const void* command, std::uint32_t bytes) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (bytes > buffer->size - buffer->offset) return SCE_KERNEL_ERROR_EBUSY;
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

extern "C" {

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
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnAddress() { return sizeof(Apr::WaitCommand); }
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnCounter() { return sizeof(Apr::WaitCommand); }
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteKernelEventQueueOnCompletion() { return sizeof(Apr::WriteKernelEventQueueCommand); }

int APS5_VABI sceAmprAprCommandBufferConstructor(Apr::CommandBufferObject* buffer, uint64_t* gatherState, uint64_t* scatterState) {
    buffer->type = Apr::BufferType::Apr;
    *gatherState = 0;
    *scatterState = 0;
    return 0;
}

int APS5_VABI sceAmprAprCommandBufferDestructor(Apr::CommandBufferObject* buffer, uint64_t* gatherState, uint64_t* scatterState) {
    (void)buffer;
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
    return AppendRead(buffer, Apr::Opcode::ReadFileGather, 0, nullptr, size, offset);
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
    void* memory = buffer->base;
    buffer->base = nullptr;
    buffer->size = 0;
    buffer->offset = 0;
    buffer->numCommands = 0;
    return memory;
}

int APS5_VABI sceAmprCommandBufferConstructMarker(Apr::CommandBufferObject* buffer, std::uint32_t type, const char* text, const std::uint32_t* color) {
    switch (type) {
        case 1: return AppendMarker(buffer, Apr::Opcode::SetMarker, text);
        case 2: return AppendMarker(buffer, Apr::Opcode::PushMarker, text);
        case 3: return AppendCommand(buffer, Apr::Opcode::PopMarker, Apr::MarkerCommand{});
        case 5: return color ? AppendMarker(buffer, Apr::Opcode::SetMarker, text) : SCE_KERNEL_ERROR_EINVAL;
        case 6: return color ? AppendMarker(buffer, Apr::Opcode::PushMarker, text) : SCE_KERNEL_ERROR_EINVAL;
        default: return SCE_KERNEL_ERROR_EINVAL;
    }
}

int APS5_VABI sceAmprCommandBufferConstructNop(Apr::CommandBufferObject* buffer, std::int16_t type, const void* payload, std::uint32_t payloadBytes, const std::uint32_t* word) {
    (void)type;
    if (payloadBytes > (word ? 56u : 60u)) return SCE_KERNEL_ERROR_EINVAL;
    if (payloadBytes != 0u && !payload) throw std::invalid_argument("sceAmprCommandBufferConstructNop: null payload");
    const std::uint32_t dwords = (payloadBytes + 3u) / 4u + (word ? 1u : 0u);
    const std::uint32_t offset = buffer ? buffer->offset : 0u;
    const int result = AppendNop(buffer, dwords, nullptr);
    if (result != 0) return result;
    std::uint8_t* data = buffer->base + offset + sizeof(Apr::CommandHeader);
    if (word) {
        std::memcpy(data, word, sizeof(*word));
        data += sizeof(*word);
    }
    if (payloadBytes != 0u) std::memcpy(data, payload, payloadBytes);
    return 0;
}

int APS5_VABI sceAmprCommandBufferConstructor(Apr::CommandBufferObject* buffer) {
    *buffer = {nullptr, 0, 0, 0, Apr::BufferType::Generic};
    return 0;
}

int APS5_VABI sceAmprCommandBufferDestructor(Apr::CommandBufferObject* buffer) {
    (void)buffer;
    return 0;
}

void* APS5_VABI sceAmprCommandBufferGetBufferBaseAddress(const Apr::CommandBufferObject* buffer) {
    return buffer->base;
}

uint32_t APS5_VABI sceAmprCommandBufferGetCurrentOffset(const Apr::CommandBufferObject* buffer) {
    return buffer->offset;
}

uint32_t APS5_VABI sceAmprCommandBufferGetNumCommands(const Apr::CommandBufferObject* buffer) {
    return buffer->numCommands;
}

uint32_t APS5_VABI sceAmprCommandBufferGetSize(const Apr::CommandBufferObject* buffer) {
    return buffer->size;
}

uint32_t APS5_VABI sceAmprCommandBufferGetType(const Apr::CommandBufferObject* buffer) {
    return static_cast<uint32_t>(buffer->type);
}

int APS5_VABI sceAmprCommandBufferNop(Apr::CommandBufferObject* buffer, std::uint32_t dwords) {
    if (dwords == 0u || dwords > 16u) return SCE_KERNEL_ERROR_EINVAL;
    return AppendNop(buffer, dwords, nullptr);
}

int APS5_VABI sceAmprCommandBufferNopWithData(Apr::CommandBufferObject* buffer, std::uint32_t dwords, const std::uint32_t* data) {
    if (dwords > 15u) return SCE_KERNEL_ERROR_EINVAL;
    if (dwords != 0u && !data) throw std::invalid_argument("sceAmprCommandBufferNopWithData: null data");
    return AppendNop(buffer, dwords, data);
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
    buffer->offset = 0;
    buffer->numCommands = 0;
    return 0;
}

int APS5_VABI sceAmprCommandBufferSetBuffer(Apr::CommandBufferObject* buffer, void* memory, uint32_t size) {
    buffer->base = static_cast<uint8_t*>(memory);
    buffer->size = size;
    buffer->offset = 0;
    buffer->numCommands = 0;
    return 0;
}

int APS5_VABI sceAmprCommandBufferSetMarker(Apr::CommandBufferObject* buffer, const char* text) {
    return AppendMarker(buffer, Apr::Opcode::SetMarker, text);
}

int APS5_VABI sceAmprCommandBufferSetMarkerWithColor(Apr::CommandBufferObject* buffer, const char* text, const std::uint32_t* color) {
    if (!color) return SCE_KERNEL_ERROR_EINVAL;
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
    return NopBytes(dwords - 1u);
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
    return sizeof(Apr::ReadFileCommand);
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

struct AmmSubmitResult {
    std::int32_t result;
    std::uint32_t errorOffset;
};

int APS5_VABI sceAmprAmmCommandBufferConstructor(Apr::CommandBufferObject* buffer) {
    (void)buffer;
    return 0;
}

int APS5_VABI sceAmprAmmCommandBufferDestructor(Apr::CommandBufferObject* buffer) {
    (void)buffer;
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
    if (!ValidMapRange(address, size)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmUnmapCommand command{{Apr::Opcode::AmmUnmap, sizeof(command)}, address, size};
    return AppendAmm(buffer, &command, sizeof(command));
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

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtect(std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t mask) {
    (void)type;
    return ValidProtect(address, size, protection, mask) ? std::int64_t{sizeof(Apr::AmmProtectCommand)} : AmmMeasureInvalid;
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtectWithGpuMaskId(std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection, std::int32_t mask, std::uint8_t gpuMaskId) {
    (void)gpuMaskId;
    return sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtect(address, size, type, protection, mask);
}

int APS5_VABI sceAmprAmmCommandBufferMapAsPrt(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!ValidMapRange(address, size)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmUnmapCommand command{{Apr::Opcode::AmmMapAsPrt, sizeof(command)}, address, size};
    return RecordPrt(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprAmmCommandBufferAllocatePaForPrt(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    if (!ValidAmmProtection(protection)) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!ValidMapRange(address, size)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmProtectCommand command{{Apr::Opcode::AmmAllocatePaForPrt, sizeof(command)}, address, size, type, protection, PrtAllocationMask, 0};
    return RecordPrt(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprAmmCommandBufferRemapIntoPrt(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t source, std::uint64_t size, std::int32_t protection, std::uint32_t opcode) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (!ValidRemap(address, source, size, protection)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmRemapCommand command{{Apr::Opcode::AmmRemapIntoPrt, sizeof(command)}, address, source, size, protection, static_cast<std::int32_t>(opcode ? opcode : 1011u)};
    return AppendAmm(buffer, &command, sizeof(command));
}

int APS5_VABI sceAmprAmmCommandBufferUnmapToPrt(Apr::CommandBufferObject* buffer, std::uint64_t address, std::uint64_t size) {
    if (!buffer) return SCE_KERNEL_ERROR_EINVAL;
    if (!buffer->base) return SCE_KERNEL_ERROR_EPERM;
    if (!ValidMapRange(address, size)) return SCE_KERNEL_ERROR_EINVAL;
    const Apr::AmmUnmapCommand command{{Apr::Opcode::AmmUnmapToPrt, sizeof(command)}, address, size};
    return AppendAmm(buffer, &command, sizeof(command));
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMapAsPrt(std::uint64_t address, std::uint64_t size) {
    return ValidMapRange(address, size) ? std::int64_t{sizeof(Apr::AmmUnmapCommand)} : AmmMeasureInvalid;
}

std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeAllocatePaForPrt(std::uint64_t address, std::uint64_t size, std::int32_t type, std::int32_t protection) {
    (void)type;
    return ValidMapRange(address, size) && ValidAmmProtection(protection) ? std::int64_t{sizeof(Apr::AmmProtectCommand)} : AmmMeasureInvalid;
}

int APS5_VABI sceAmprAmmGiveDirectMemory(std::int64_t searchStart, std::int64_t searchEnd, std::size_t size, std::size_t alignment, int usage, std::int64_t* offset) {
    return AmmGiveDirectMemory_nid_no_patch(searchStart, searchEnd, size, alignment, usage, offset);
}

int APS5_VABI sceAmprAmmGetVirtualAddressRanges(std::uint64_t* start, std::uint64_t* end, std::uint64_t* multimapStart, std::uint64_t* multimapEnd) {
    if (!start || !end || !multimapStart || !multimapEnd) throw std::invalid_argument("sceAmprAmmGetVirtualAddressRanges: null output");
    AmmVirtualAddressRanges_nid_no_patch(start, end, multimapStart, multimapEnd);
    return 0;
}

int APS5_VABI sceAmprAmmSubmitCommandBuffer(void* base, std::uint32_t offset, std::uint32_t priority) {
    if (priority > 2u) return SCE_KERNEL_ERROR_EINVAL;
    if (!base) return SCE_KERNEL_ERROR_EPERM;
    AmmSubmit_nid_no_patch(base, offset);
    return 0;
}

int APS5_VABI sceAmprAmmSubmitCommandBuffer2(void* base, std::uint32_t offset, std::uint32_t priority, AmmSubmitResult* result, std::uint32_t* id) {
    if (priority > 2u) return SCE_KERNEL_ERROR_EINVAL;
    if (!base) return SCE_KERNEL_ERROR_EPERM;
    const auto submitted = AmmSubmit_nid_no_patch(base, offset);
    if (result) *result = {0, 0};
    if (id) *id = submitted;
    return 0;
}

int APS5_VABI sceAmprAmmSubmitCommandBuffer3(void* base, std::uint32_t offset, std::uint32_t priority, std::uint32_t* id) {
    if (priority > 2u) return SCE_KERNEL_ERROR_EINVAL;
    if (!base) return SCE_KERNEL_ERROR_EPERM;
    const auto submitted = AmmSubmit_nid_no_patch(base, offset);
    if (id) *id = submitted;
    return 0;
}

int APS5_VABI sceAmprAmmWaitCommandBufferCompletion(std::uint32_t id) {
    return AmmSubmitted_nid_no_patch(id) ? 0 : SCE_KERNEL_ERROR_ESRCH;
}

int APS5_VABI _ZNSt8ios_base7_AddstdEPS__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZSt16_Throw_Cpp_errori_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Thrd_join_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt4_Pad8_ReleaseEv_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt4_PadC2Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt4_PadD2Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Thrd_id_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Cnd_signal_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Unlock_shared_ptr_spin_lock_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Cnd_init_with_name_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZTVN10__cxxabiv120__function_type_infoE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZTVSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKcSE__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZTVN10__cxxabiv119__pointer_type_infoE_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZTVSt12bad_weak_ptr_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Lock_shared_ptr_spin_lock_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNSt12bad_weak_ptrD1Ev_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Thrd_sleep_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI wcstombs_s_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __cxa_call_unexpected_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
