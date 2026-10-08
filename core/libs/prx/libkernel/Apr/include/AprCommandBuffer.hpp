#ifndef CORE_LIBS_PRX_LIBKERNEL_APR_APRCOMMANDBUFFER_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APR_APRCOMMANDBUFFER_HPP

#include <cstddef>
#include <cstdint>

// Shared between libSceAmpr (which records commands) and libkernel (which executes them).
// The object layout is guest ABI; command records use the shared host encoding.
namespace Apr {

enum class BufferType : std::uint16_t {
    Generic = 0,
    Apr = 1,
};

struct CommandBufferObject {
    BufferType type;
    std::uint16_t recording;
    std::uint32_t offset;
    std::uint32_t numCommands;
    std::uint32_t size;
    std::uint8_t* base;
};
static_assert(sizeof(CommandBufferObject) == 0x18, "guest reserves 0x18 bytes for sce::Ampr::CommandBuffer");

static_assert(offsetof(CommandBufferObject, type) == 0x00);
static_assert(offsetof(CommandBufferObject, recording) == 0x02);
static_assert(offsetof(CommandBufferObject, numCommands) == 0x08);
static_assert(offsetof(CommandBufferObject, offset) == 0x04);
static_assert(offsetof(CommandBufferObject, size) == 0x0c);
static_assert(offsetof(CommandBufferObject, base) == 0x10);

inline constexpr std::uint16_t ScatterGatherValid = 1;
inline constexpr std::uint16_t MapActive = 2;

enum class Opcode : std::uint32_t {
    Nop = 0,
    ReadFile = 1,
    WriteAddress = 2,
    WriteCounter = 3,
    WaitOnAddress = 4,
    WaitOnCounter = 5,
    WriteKernelEventQueue = 6,
    WriteAddressFromTimeCounter = 7,
    WriteAddressFromCounter = 8,
    WriteAddressFromCounterPair = 9,
    PushMarker = 10,
    PopMarker = 11,
    SetMarker = 12,
    ReadFileGather = 13,
    ReadFileScatter = 14,
    ReadFileGatherScatter = 15,
    ResetGatherScatterState = 16,
    AmmMap = 17,
    AmmMapDirect = 18,
    AmmUnmap = 19,
    MapEnd = 20,
    AmmRemap = 21,
    AmmMultiMap = 22,
    AmmModifyProtect = 23,
    AmmModifyMtypeProtect = 24,
    AmmMapAsPrt = 25,
    AmmAllocatePaForPrt = 26,
    AmmRemapIntoPrt = 27,
    AmmUnmapToPrt = 28,
    CompactReadFileGather = 29,
};

struct CommandHeader {
    Opcode opcode;
    std::uint32_t bytes;
};

struct ReadFileCommand {
    CommandHeader header;
    std::uint32_t fileId;
    std::uint32_t reserved;
    std::uint64_t destination;
    std::uint64_t size;
    std::uint64_t offset;
};

struct CompactReadFileGatherCommand {
    std::uint32_t opcodeAndOffsetHigh;
    std::uint32_t sizeMinusOne;
    std::uint32_t offsetLow;
};
static_assert(sizeof(CompactReadFileGatherCommand) == 12);

struct ResetGatherScatterStateCommand {
    CommandHeader header;
};

struct AmmMapCommand {
    CommandHeader header;
    std::uint64_t address;
    std::uint64_t directOffset;
    std::uint64_t size;
    std::int32_t type;
    std::int32_t protection;
};

struct AmmUnmapCommand {
    CommandHeader header;
    std::uint64_t address;
    std::uint64_t size;
};

struct MapEndCommand {
    CommandHeader header;
};

struct AmmRemapCommand {
    CommandHeader header;
    std::uint64_t address;
    std::uint64_t source;
    std::uint64_t size;
    std::int32_t protection;
    std::int32_t reserved;
};

struct AmmProtectCommand {
    CommandHeader header;
    std::uint64_t address;
    std::uint64_t size;
    std::int32_t type;
    std::int32_t protection;
    std::int32_t mask;
    std::int32_t reserved;
};

struct WriteAddressCommand {
    CommandHeader header;
    std::uint64_t address;
    std::uint64_t value;
    std::uint32_t flags;
    std::uint32_t reserved;
};

enum class CounterAccess : std::uint8_t {
    Size8 = 0,
    Size4 = 1,
    Size2Offset0 = 2,
    Size2Offset1 = 3,
    Size1Offset0 = 4,
    Size1Offset1 = 5,
    Size1Offset2 = 6,
    Size1Offset3 = 7,
};

enum class CounterOperation : std::uint8_t {
    Store = 0,
    AtomicOr = 1,
    AtomicAndComplement = 2,
    AtomicXor = 3,
    AtomicAdd = 4,
};

struct WriteCounterCommand {
    CommandHeader header;
    std::uint32_t counter;
    CounterAccess access;
    CounterOperation operation;
    std::uint16_t reserved;
    std::uint64_t value;
};

struct WaitCommand {
    CommandHeader header;
    std::uint64_t address;
    std::uint64_t reference;
    std::uint64_t mask;
    std::uint32_t counter;
    std::uint8_t compare;
    CounterAccess access;
    std::uint16_t reserved;
};

struct WriteKernelEventQueueCommand {
    CommandHeader header;
    std::uint64_t equeue;
    std::uint64_t ident;
    std::uint64_t data;
    std::uint64_t userData;
};

struct WriteAddressFromCounterCommand {
    CommandHeader header;
    std::uint64_t address;
    std::uint32_t counter0;
    std::uint32_t counter1;
};

struct MarkerCommand {
    CommandHeader header;
};

}

extern "C" {
int AmmGiveDirectMemory_nid_no_patch(std::int64_t searchStart, std::int64_t searchEnd, std::size_t size, std::size_t alignment, int usage, std::int64_t* offset);
void AmmVirtualAddressRanges_nid_no_patch(std::uint64_t* start, std::uint64_t* end, std::uint64_t* multimapStart, std::uint64_t* multimapEnd);
std::uint32_t AmmSubmit_nid_no_patch(void* base, std::uint32_t bytes);
bool AmmSubmitted_nid_no_patch(std::uint32_t id);
}

#endif
