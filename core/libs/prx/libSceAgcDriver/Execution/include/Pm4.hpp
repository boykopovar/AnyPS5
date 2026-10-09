#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_PM4_HPP

#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include <cstddef>
#include <vector>
#include <optional>
#include <array>
#include <span>
#include <string>

namespace AgcDriver::Pm4 {

constexpr std::size_t GdsBytes = 0x10000;
std::uint64_t GdsAddress();

struct DrawParameters {
    std::uint64_t indexAddress;
    std::uint32_t indexCount;
    std::uint32_t indexSize;
    std::uint32_t instanceCount;
    std::uint32_t flags;
    bool indexed = true;
    std::uint32_t firstVertex = 0;
    std::uint32_t firstInstance = 0;
    // An indirect draw (DRAW_INDIRECT 0x24, DRAW_INDEX_INDIRECT 0x25 and their multi forms 0x2c and
    // 0x38): the CP reads {vertexCount, instanceCount, startVertex, startInstance} (indexed:
    // {indexCount, instanceCount, firstIndex, vertexOffset, startInstance}) per record at
    // `arguments` + record * stride, writes startVertex / vertexOffset, startInstance and the draw
    // index into the SH registers the packet names (0x280: nowhere) and takes the instance count
    // from the record. indexCount and instanceCount above are then unknown (indexed draws bind the
    // declared INDEX_BUFFER_SIZE range), firstVertex holds GE_INDX_OFFSET. The driver decides per
    // dimension how the GPU reproduces the CP's register write for the fixed-function fetch (see
    // Driver.cpp): InPlace draws from the record's dword, Constant rewrites it with the constant.
    struct IndirectDraw {
        std::uint64_t arguments;
        std::uint32_t opcode;
        std::uint32_t recordBytes;
        std::uint32_t stride;
        std::uint32_t count;
        bool countIndirect;
        std::uint64_t countAddress;
        std::uint32_t baseVertexLocation;
        std::uint32_t startInstanceLocation;
        std::uint32_t drawIndexLocation;
        bool drawIndexEnabled;
        std::uint32_t indxOffset;
        enum class Rule : std::uint8_t { InPlace, Constant };
        Rule vertexRule = Rule::Constant;
        Rule instanceRule = Rule::Constant;
        std::uint32_t vertexConstant = 0;
        std::uint32_t instanceConstant = 0;
        std::int32_t baseVertexSgpr = -1;
        std::int32_t startInstanceSgpr = -1;
        std::int32_t drawIndexSgpr = -1;
        // Bytes of the record range: (count - 1) * stride + recordBytes, or 0 for no records.
        std::uint64_t RangeBytes() const { return count == 0 ? 0 : static_cast<std::uint64_t>(count - 1) * stride + recordBytes; }
        std::uint32_t VertexDwordOffset() const { return recordBytes == 20 ? 12u : 8u; }
        std::uint32_t InstanceDwordOffset() const { return recordBytes == 20 ? 16u : 12u; }
    };
    std::optional<IndirectDraw> indirect;
};

// One indirect draw record as the CP reads it (firstVertexOrIndex is startVertex for the
// non-indexed layout and firstIndex for the indexed one; vertexOffset exists only in the latter).
struct DrawArguments {
    std::uint32_t count;
    std::uint32_t instances;
    std::uint32_t firstVertexOrIndex;
    std::uint32_t vertexOffset;
    std::uint32_t firstInstance;
};
// Reads record `record` of an indirect draw, or its count dword, through the checked guest memory
// path (which waits for recorded GPU work that writes them).
DrawArguments ReadDrawArguments(const DrawParameters::IndirectDraw& indirect, std::uint32_t record);
std::uint32_t ReadDrawCount(const DrawParameters::IndirectDraw& indirect);
inline bool IndirectDrawOpcode(std::uint32_t opcode) { return opcode == 0x24 || opcode == 0x25 || opcode == 0x2c || opcode == 0x38; }
inline bool DrawOpcode(std::uint32_t opcode) { return opcode == 0x27 || opcode == 0x2d || opcode == 0x35 || IndirectDrawOpcode(opcode); }

std::string Name(std::uint32_t header);
// A PM4 type-2 packet is a one-dword filler (command-buffer padding); type 3 and type 0 carry a
// dword count in bits 29:16. Type 1 is undefined.
inline bool FillerPacket(std::uint32_t header) { return (header >> 30u) == 2u; }
inline std::size_t PacketWords(std::uint32_t header) { return FillerPacket(header) ? 1u : static_cast<std::size_t>((header >> 16u) & 0x3fffu) + 2u; }
std::string_view UnsupportedReason(std::uint32_t header);
void Validate(std::span<const std::uint32_t> packet, std::uint32_t queue);
void Execute(std::span<const std::uint32_t> packet, QueueState& queue);
inline bool IndirectRegisterOpcode(std::uint32_t opcode) { return opcode == 0x63 || opcode == 0x64 || opcode == 0x9f; }
std::vector<std::uint32_t> ReadIndirectRegisters(std::span<const std::uint32_t> packet);
void ExecuteIndirectRegisters(std::span<const std::uint32_t> packet, std::span<const std::uint32_t> pairs, QueueState& queue);
bool AccessesMemory(std::uint32_t header);
// Whether an ACQUIRE_MEM packet asks only for GPU cache actions (no CPU-visible memory
// synchronization): such a packet needs a pipeline barrier, not a device drain.
bool UsesGpuCacheBarrier(std::span<const std::uint32_t> packet);
bool IsTagMarker(std::span<const std::uint32_t> packet);
inline bool Predicated(std::uint32_t header) { return !FillerPacket(header) && (header & 1u) != 0; }
bool PredicationPasses(const QueueState& queue);
bool WaitSatisfied(std::span<const std::uint32_t> packet);
// WaitSatisfied for a polling loop: the caller has validated the address once, so the value is read
// directly instead of through the checked guest memory path.
bool WaitSatisfiedUnchecked(std::span<const std::uint32_t> packet);
// Whether a WAIT_REG_MEM's compare holds for `value` (the 4 or 8 bytes a label the recorder still
// holds will store), without reading memory.
bool WaitComparesValue(std::span<const std::uint32_t> packet, std::uint64_t value);
std::size_t WaitAwaitedBytes(std::span<const std::uint32_t> packet);
std::size_t ConditionalWords(std::span<const std::uint32_t> packet);
std::uint32_t ReadCondition(std::span<const std::uint32_t> packet);
// A label write (RELEASE_MEM with a data select, WRITE_DATA to memory): the destination and the
// bytes it stores, so the write can be recorded on the GPU behind the work it signals. Packets
// without a memory destination decode to nothing. No allocation per label (tens of thousands per
// second): WRITE_DATA's bytes are a view of the packet (valid while the packet is), a RELEASE_MEM
// value is held inline.
struct LabelWrite {
    std::uint64_t address;
    std::span<const std::byte> packetBytes;
    std::array<std::byte, 8> inlineBytes{};
    std::size_t inlineSize = 0;
    std::span<const std::byte> Bytes() const { return inlineSize != 0 ? std::span<const std::byte>(inlineBytes).first(inlineSize) : packetBytes; }
};
std::optional<LabelWrite> DecodeLabelWrite(std::span<const std::uint32_t> packet);
// A memory store the CPU can resolve before the GPU runs it (COPY_DATA and DMA_DATA to memory,
// DUMP_CONST_RAM): the destination and the bytes it stores, so the driver can record the store on
// the GPU like a label instead of draining the device and storing on the CPU. Immediate and constant
// RAM bytes are known at once; a memory source is read here through the checked guest memory path
// (which waits only for recorded GPU work that writes the source). Nothing when the packet is not
// such a store, when it stores more than `limit` bytes, or when its destination or size is not a
// multiple of 4 (the GPU store needs both; the source is then not read: the caller keeps its CPU
// path). A store of zero bytes resolves with empty bytes.
struct StoreWrite {
    std::uint64_t address;
    std::span<const std::byte> viewBytes;
    std::vector<std::byte> ownedBytes;
    std::span<const std::byte> Bytes() const { return ownedBytes.empty() ? viewBytes : std::span<const std::byte>(ownedBytes); }
};
std::optional<StoreWrite> ResolveStore(std::span<const std::uint32_t> packet, const QueueState& queue, std::size_t limit);
struct MemoryCopy {
    std::uint64_t source;
    std::uint64_t destination;
    std::size_t bytes;
};
std::optional<MemoryCopy> DecodeMemoryCopy(std::span<const std::uint32_t> packet);
std::uint32_t DmaSource(std::span<const std::uint32_t> packet);
std::uint32_t DmaDestination(std::span<const std::uint32_t> packet);
// A DISPATCH_INDIRECT's arguments: the guest address of its three group-count dwords (no memory
// access, so the GPU can read them in place: VulkanDevice::DispatchIndirect), the DISPATCH_DIRECT
// packet made by reading them there (through the checked guest memory path, which waits for
// recorded GPU work that writes them), and both steps in one for the packet.
std::uint64_t DispatchArgumentAddress(std::span<const std::uint32_t> packet, const QueueState& queue);
std::array<std::uint32_t, 5> ReadDispatchArguments(std::uint64_t arguments, std::uint32_t initiator);
std::array<std::uint32_t, 5> ResolveDispatch(std::span<const std::uint32_t> packet, const QueueState& queue);
DrawParameters ResolveDraw(std::span<const std::uint32_t> packet, const QueueState& queue);

}

#endif
