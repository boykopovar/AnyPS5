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

struct DrawParameters {
    std::uint64_t indexAddress;
    std::uint32_t indexCount;
    std::uint32_t indexSize;
    std::uint32_t instanceCount;
    std::uint32_t flags;
    bool indexed = true;
    std::uint32_t firstVertex = 0;
    std::uint32_t firstInstance = 0;
};

std::string Name(std::uint32_t header);
// A PM4 type-2 packet is a one-dword filler (command-buffer padding); type 3 and type 0 carry a
// dword count in bits 29:16. Type 1 is undefined.
inline bool FillerPacket(std::uint32_t header) { return (header >> 30u) == 2u; }
inline std::size_t PacketWords(std::uint32_t header) { return FillerPacket(header) ? 1u : static_cast<std::size_t>((header >> 16u) & 0x3fffu) + 2u; }
std::string_view UnsupportedReason(std::uint32_t header);
void Validate(std::span<const std::uint32_t> packet, std::uint32_t queue);
void Execute(std::span<const std::uint32_t> packet, QueueState& queue);
bool AccessesMemory(std::uint32_t header);
// Whether an ACQUIRE_MEM packet asks only for GPU cache actions (no CPU-visible memory
// synchronization): such a packet needs a pipeline barrier, not a device drain.
bool UsesGpuCacheBarrier(std::span<const std::uint32_t> packet);
bool IsTagMarker(std::span<const std::uint32_t> packet);
bool WaitSatisfied(std::span<const std::uint32_t> packet);
// WaitSatisfied for a polling loop: the caller has validated the address once, so the value is read
// directly instead of through the checked guest memory path.
bool WaitSatisfiedUnchecked(std::span<const std::uint32_t> packet);
// Whether a WAIT_REG_MEM's compare holds for `value` (the 4 or 8 bytes a label the recorder still
// holds will store), without reading memory.
bool WaitComparesValue(std::span<const std::uint32_t> packet, std::uint64_t value);
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
