#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace AgcDriver::Pm4 {
namespace {
namespace {
bool IsTagMarker(std::span<const std::uint32_t> packet) {
    return packet.size() >= 3 && ((packet[0] >> 8u) & 0xffu) == 0x79u && packet[1] == 0x342u;
}

bool WaitCompares(std::uint32_t function, std::uint64_t reference, std::uint64_t mask, std::uint64_t value) {
    const auto masked = value & mask;
    switch (function) {
        case 0: return true;
        case 1: return masked < reference;
        case 2: return masked <= reference;
        case 3: return masked == reference;
        case 4: return masked != reference;
        case 5: return masked >= reference;
        case 6: return masked > reference;
        default: return false;
    }
}

namespace {

bool waitCompares(std::span<const std::uint32_t> packet, bool wide, std::uint64_t value) {
    return WaitCompares(packet[1] & 7u, wide ? address(packet[4], packet[5]) : packet[4], wide ? address(packet[6], packet[7]) : packet[5], value);
}

}

std::size_t WaitAwaitedBytes(std::span<const std::uint32_t> packet) {
    const bool wide = ((packet[0] >> 8u) & 0xffu) == 0x93u;
    return wide && (packet.size() < 8 || packet[7] != 0) ? 8 : 4;
}

bool WaitSatisfied(std::span<const std::uint32_t> packet) {
    const bool wide = ((packet[0] >> 8u) & 0xffu) == 0x93u;
    const auto awaitedBytes = WaitAwaitedBytes(packet);
    const auto source = address(packet[2] & (awaitedBytes == 8 ? ~7u : ~3u), packet[3]);
    std::uint64_t value = 0;
    // Named for the [hooksync] attribution (the read goes through the flush hook).
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Wait);
    GuestMemory::Read(source, std::as_writable_bytes(std::span(&value, 1)).first(awaitedBytes), awaitedBytes);
    return waitCompares(packet, wide, value);
}

bool WaitSatisfiedUnchecked(std::span<const std::uint32_t> packet) {
    const bool wide = ((packet[0] >> 8u) & 0xffu) == 0x93u;
    const auto awaitedBytes = WaitAwaitedBytes(packet);
    const auto source = address(packet[2] & (awaitedBytes == 8 ? ~7u : ~3u), packet[3]);
    const std::uint64_t value = awaitedBytes == 8 ? *reinterpret_cast<const volatile std::uint64_t*>(source) : *reinterpret_cast<const volatile std::uint32_t*>(source);
    return waitCompares(packet, wide, value);
}

bool WaitComparesValue(std::span<const std::uint32_t> packet, std::uint64_t value) {
    const bool wide = ((packet[0] >> 8u) & 0xffu) == 0x93u;
    return waitCompares(packet, wide, value);
}

std::size_t ConditionalWords(std::span<const std::uint32_t> packet) {
    require(packet.size() == 5 && ((packet[0] >> 8u) & 0xffu) == 0x22u, "expected COND_EXEC packet");
    return packet[4] & ConditionalWordsMask;
}

std::size_t PredicationBytes(const Predication& predication) {
    if (predication.operation == 1) return SampleDumpBytes + SampleDumpBlockBytes;
    return predication.operation == 3 ? 8 : 4;
}

bool PredicationPasses(const QueueState& queue) {
    const auto& predication = queue.predication;
    if (predication.operation == 0) return true;
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Wait);
    if (predication.operation == 1) {
        constexpr std::uint64_t ready = 1ull << 63u;
        require(queue.sampleDumpBlocks != 0, "Z-pass predication with no counter block enabled by PIXEL_PIPE_STAT_CONTROL is not implemented");
        std::uint64_t samples = 0;
        for (std::uint64_t db = 0; db < SampleDumpBlocks; ++db) {
            if (((queue.sampleDumpBlocks >> db) & 1u) == 0) continue;
            std::array<std::uint64_t, 2> pair{};
            GuestMemory::Read(predication.address + db * SampleDumpStride, std::as_writable_bytes(std::span(pair)), 8);
            const auto begin = pair[0];
            const auto end = pair[1];
            if ((begin & ready) == 0 || (end & ready) == 0) {
                require(predication.runWhenUnwritten, "Z-pass predication waits for a counter pair that was never written");
                return true;
            }
            samples += (end & ~ready) - (begin & ~ready);
        }
        return (samples != 0) == predication.executeWhenSet;
    }
    const std::size_t bytes = PredicationBytes(predication);
    std::uint64_t value = 0;
    GuestMemory::Read(predication.address, std::as_writable_bytes(std::span(&value, 1)).first(bytes), bytes);
    return (value != 0) == predication.executeWhenSet;
}

std::uint32_t ReadCondition(std::span<const std::uint32_t> packet) {
    require(packet.size() == 5 && ((packet[0] >> 8u) & 0xffu) == 0x22u, "expected COND_EXEC packet");
    std::uint32_t value = 0;
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Wait);
    GuestMemory::Read(address(packet[1], packet[2]), std::as_writable_bytes(std::span(&value, 1)), 4);
    return value;
}

std::optional<LabelWrite> DecodeLabelWrite(std::span<const std::uint32_t> packet) {
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    if (opcode == 0x49 && packet.size() >= 7) {
        const auto dataSelect = packet[2] >> 29u;
        const auto destination = address(packet[3], packet[4]);
        if (dataSelect == 0 || dataSelect > 4) return std::nullopt;
        if (destination == 0) return std::nullopt;
        const auto value = ReleaseMemValue(packet);
        LabelWrite label{destination, {}};
        label.inlineSize = dataSelect == 1 ? 4 : 8;
        std::memcpy(label.inlineBytes.data(), &value, label.inlineSize);
        return label;
    }
    if (opcode == 0x37 && packet.size() > 4) {
        const auto destinationSelect = (packet[1] >> 8u) & 0xfu;
        if (destinationSelect == 0 || destinationSelect == 4 || destinationSelect > 5) return std::nullopt;
        const auto destination = address(packet[2], packet[3]);
        if (destination == 0) return std::nullopt;
        // WR_ONE_ADDR stores every value at the same address: the last one remains.
        const auto words = (packet[1] & 0x10000u) != 0 ? packet.last(1) : packet.subspan(4);
        return LabelWrite{destination, std::as_bytes(words)};
    }
    return std::nullopt;
}

std::optional<StoreWrite> ResolveStore(std::span<const std::uint32_t> packet, const QueueState& queue, std::size_t limit) {
    // The same decode as Execute's cases below (a packet Validate accepted): a store that is not
    // resolved here still runs there. The size and alignment tests precede any memory read: a store
    // the GPU path would reject anyway must not read (and sync for) its source here and again there.
    const auto fits = [limit](std::uint64_t destination, std::size_t bytes) { return bytes <= limit && destination % 4 == 0 && bytes % 4 == 0; };
    switch ((packet[0] >> 8u) & 0xffu) {
        case 0x40: {
            if (packet.size() < 6 || CopyDataToRegister(packet)) return std::nullopt;
            const auto dstSelect = (packet[1] >> 8u) & 0xfu;
            if (dstSelect != 1 && dstSelect != 2 && dstSelect != 5) return std::nullopt;
            if (CopyDataSource(packet) == CopyDataRegisterSource) return std::nullopt;
            const std::size_t bytes = CopyDataBytes(packet);
            const auto destination = address(packet[4], packet[5]);
            if (!fits(destination, bytes)) return std::nullopt;
            return StoreWrite{destination, {}, copyDataSource(packet, queue, bytes)};
        }
        case 0x50: {
            if (packet.size() < 7) return std::nullopt;
            if (dmaDestination(packet) == DmaDestinationNowhere) return StoreWrite{address(packet[2], packet[3]), {}, {}};
            const std::size_t bytes = DmaBytes(packet);
            const auto destination = dmaDestination(packet) == DmaSelectGds ? GdsAddress() + packet[4] : address(packet[4], packet[5]);
            if (!fits(destination, bytes)) return std::nullopt;
            return StoreWrite{destination, {}, dmaSourceBytes(packet)};
        }
        case 0x83: {
            if (packet.size() != 5) return std::nullopt;
            const auto dump = DecodeConstantRamDump(packet);
            if (dump.offset / 4 > QueueState::ConstantRamWords || dump.words > QueueState::ConstantRamWords - dump.offset / 4) return std::nullopt;
            const auto words = queue.ConstantRam().subspan(dump.offset / 4, dump.words);
            if (words.size_bytes() > limit) return std::nullopt;
            // A view of the queue's constant RAM: valid until the next CONST_RAM write on this queue,
            // which is a later packet of the same worker.
            return StoreWrite{dump.destination, std::as_bytes(words), {}};
        }
        default: return std::nullopt;
    }
}

std::uint32_t DmaSource(std::span<const std::uint32_t> packet) {
    return dmaSource(packet);
}

std::uint32_t DmaDestination(std::span<const std::uint32_t> packet) {
    return dmaDestination(packet);
}

WaitFields DecodeWait(std::span<const std::uint32_t> packet) {
    const auto opcode = packet.empty() ? 0u : (packet[0] >> 8u) & 0xffu;
    require((opcode == 0x3cu && packet.size() == 7) || (opcode == 0x93u && packet.size() == 9), "expected WAIT_REG_MEM or WAIT_REG_MEM_64 packet");
    const bool wide = opcode == 0x93u;
    const auto awaitedBytes = WaitAwaitedBytes(packet);
    return {packet[1] & 7u, address(packet[2] & (awaitedBytes == 8 ? ~7u : ~3u), packet[3]), wide ? address(packet[4], packet[5]) : packet[4], wide ? address(packet[6], packet[7]) : packet[5], wide ? packet[8] : packet[6], awaitedBytes};
}

ConditionalWrite DecodeConditionalWrite(std::span<const std::uint32_t> packet) {
    require(packet.size() == 9 && ((packet[0] >> 8u) & 0xffu) == 0x45u, "expected COND_WRITE packet");
    return {address(packet[2], packet[3]), address(packet[6], packet[7]), packet[8]};
}

ConstantRamDump DecodeConstantRamDump(std::span<const std::uint32_t> packet) {
    require(packet.size() == 5 && ((packet[0] >> 8u) & 0xffu) == 0x83u, "expected DUMP_CONST_RAM packet");
    return {packet[1], packet[2], address(packet[3], packet[4])};
}

std::vector<PacketStore> DecodeStores(std::span<const std::uint32_t> packet) {
    require(!packet.empty(), "truncated PM4 header");
    const auto known = [](std::uint64_t destination, std::span<const std::byte> bytes) {
        return PacketStore{destination, bytes.size(), PacketStore::Value::Bytes, {bytes.begin(), bytes.end()}, false};
    };
    const auto pattern = [](std::uint64_t destination, std::uint64_t bytes, std::uint32_t value) {
        const auto repeated = std::as_bytes(std::span(&value, 1));
        return PacketStore{destination, bytes, PacketStore::Value::Pattern, {repeated.begin(), repeated.end()}, false};
    };
    const auto runTime = [](std::uint64_t destination, std::uint64_t bytes) { return PacketStore{destination, bytes, PacketStore::Value::RunTime, {}, false}; };
    if (FillerPacket(packet[0])) return {};
    switch ((packet[0] >> 8u) & 0xffu) {
        case 0x49: {
            const auto label = DecodeLabelWrite(packet);
            if (!label.has_value()) return {};
            if ((packet[2] >> 29u) >= 3) return {runTime(label->address, label->Bytes().size())};
            return {known(label->address, label->Bytes())};
        }
        case 0x37: {
            const auto label = DecodeLabelWrite(packet);
            if (!label.has_value()) return {};
            return {known(label->address, label->Bytes())};
        }
        case 0x39: return {runTime(address(packet[1], packet[2]), 8)};
        case 0x40: {
            require(packet.size() == 6, "invalid COPY_DATA size");
            if (CopyDataToRegister(packet)) return {};
            const auto dstSelect = (packet[1] >> 8u) & 0xfu;
            if (dstSelect != 1 && dstSelect != 2 && dstSelect != 5) return {};
            const auto destination = address(packet[4], packet[5]);
            if (CopyDataImmediate(CopyDataSource(packet))) return {pattern(destination, CopyDataBytes(packet), packet[2])};
            return {runTime(destination, CopyDataBytes(packet))};
        }
        case 0x50: {
            require(packet.size() == 7, "invalid DMA_DATA size");
            const auto bytes = DmaBytes(packet);
            if (bytes == 0 || dmaDestination(packet) == DmaDestinationNowhere || dmaDestination(packet) == DmaSelectGds) return {};
            const auto destination = address(packet[4], packet[5]);
            if (dmaSource(packet) == DmaSourceData) return {pattern(destination, bytes, packet[2])};
            return {runTime(destination, bytes)};
        }
        case 0x45: {
            const auto write = DecodeConditionalWrite(packet);
            if (((packet[1] >> 8u) & 3u) != 1u) return {};
            auto store = known(write.destination, std::as_bytes(std::span(&write.value, 1)));
            store.conditional = true;
            return {store};
        }
        case 0x83: {
            const auto dump = DecodeConstantRamDump(packet);
            if (dump.words == 0) return {};
            return {runTime(dump.destination, static_cast<std::uint64_t>(dump.words) * 4)};
        }
        case 0x46: {
            if (!SampleDump(packet)) return {};
            require(packet.size() == 4, "invalid EVENT_WRITE sample dump size");
            std::vector<PacketStore> stores;
            for (std::uint64_t block = 0; block < SampleDumpBlocks; ++block) stores.push_back(runTime(SampleDumpAddress(packet) + block * SampleDumpStride, SampleDumpBlockBytes));
            return stores;
        }
        default: return {};
    }
}

std::optional<MemoryCopy> DecodeMemoryCopy(std::span<const std::uint32_t> packet) {
    if (packet.size() != 7 || ((packet[0] >> 8u) & 0xffu) != 0x50) return std::nullopt;
    if (!memorySelector(dmaSource(packet)) || !memorySelector(dmaDestination(packet))) return std::nullopt;
    const std::size_t bytes = DmaBytes(packet);
    const auto source = address(packet[2], packet[3]);
    const auto destination = address(packet[4], packet[5]);
    if (bytes == 0 || (source < destination + bytes && destination < source + bytes)) return std::nullopt;
    return MemoryCopy{source, destination, bytes};
}

bool UsesGpuCacheBarrier(std::span<const std::uint32_t> packet) {
    require(!packet.empty() && ((packet[0] >> 8u) & 0xffu) == 0x58, "cache barrier requires ACQUIRE_MEM");
    Validate(packet, 0);
    return packet.size() == 8 && (packet[7] & 0xfc00u) == 0;
}

bool AccessesMemory(std::uint32_t header) {
    switch ((header >> 8u) & 0xffu) {
        case 0x22: case 0x27: case 0x39: case 0x3c: case 0x93: case 0x49: case 0x16: case 0x2d: case 0x35: case 0x24: case 0x25: case 0x2c: case 0x38: case 0x37: case 0x40: case 0x45: case 0x50: case 0x63: case 0x64: case 0x83: case 0x91: case 0x9f: return true;
        default: return false;
    }
}

std::uint64_t DispatchArgumentAddress(std::span<const std::uint32_t> packet, const QueueState& queue) {
    if (packet.size() == 4) return address(packet[1], packet[2]);
    require(queue.dispatchIndirectBase != 0, "indirect dispatch base has not been set");
    require(packet[1] <= std::numeric_limits<std::uint64_t>::max() - queue.dispatchIndirectBase, "indirect dispatch address overflow");
    return queue.dispatchIndirectBase + packet[1];
}

std::array<std::uint32_t, 3> PartialGroupThreads(const std::array<std::uint32_t, 3>& groups, const std::array<std::uint32_t, 3>& numThreads) {
    std::array<std::uint32_t, 3> threads{};
    bool partial = false;
    bool empty = false;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto full = numThreads[axis] & 0xffffu;
        const auto last = numThreads[axis] >> 16u;
        require(last != 0 && last <= full, "COMPUTE_NUM_THREAD partial group size is zero or exceeds the full group");
        partial |= last != full;
        if (groups[axis] == 0) {
            empty = true;
            continue;
        }
        const auto total = static_cast<std::uint64_t>(groups[axis] - 1) * full + last;
        require(total <= std::numeric_limits<std::uint32_t>::max(), "partial thread group dispatch size exceeds 32 bits");
        threads[axis] = static_cast<std::uint32_t>(total);
    }
    return partial && !empty ? threads : std::array<std::uint32_t, 3>{};
}

std::array<std::uint32_t, 5> ReadDispatchArguments(std::uint64_t arguments, std::uint32_t initiator) {
    std::array<std::uint32_t, 5> result{0xc0031500u, 0, 0, 0, initiator};
    // Named for the [hooksync] attribution (the read goes through the flush hook).
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::IndirectArguments);
    GuestMemory::Read(arguments, std::as_writable_bytes(std::span(result).subspan(1, 3)), 4);
    return result;
}

std::array<std::uint32_t, 5> ResolveDispatch(std::span<const std::uint32_t> packet, const QueueState& queue) {
    return ReadDispatchArguments(DispatchArgumentAddress(packet, queue), packet.back());
}

namespace {

// The index buffer state an indexed draw binds: the index size for the journaled INDEX_TYPE and the
// checks every indexed draw makes on INDEX_BASE.
std::uint32_t indexSizeOf(const QueueState& queue) {
    require(queue.indexType <= 2, "unsupported index type");
    const std::uint32_t indexSize = queue.indexType == 0 ? 2 : queue.indexType == 1 ? 4 : 1;
    require(queue.indexBase != 0 && queue.indexBase % indexSize == 0, "null or misaligned index base");
    return indexSize;
}

DrawParameters resolveIndirectDraw(std::span<const std::uint32_t> packet, const QueueState& queue) {
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    const bool indexed = opcode == 0x25 || opcode == 0x38;
    const bool multi = opcode == 0x2c || opcode == 0x38;
    require(queue.drawIndirectBase != 0, "indirect draw base has not been set");
    require(packet[1] <= std::numeric_limits<std::uint64_t>::max() - queue.drawIndirectBase, "indirect draw address overflow");
    DrawParameters::IndirectDraw indirect{};
    indirect.arguments = queue.drawIndirectBase + packet[1];
    indirect.opcode = opcode;
    indirect.recordBytes = indexed ? 20 : 16;
    indirect.stride = multi ? packet[8] : indirect.recordBytes;
    indirect.count = multi ? packet[5] : 1;
    indirect.countIndirect = multi && (packet[4] & (1u << 30u)) != 0;
    indirect.countAddress = multi ? address(packet[6], packet[7]) : 0;
    const auto location = [](std::uint32_t value) { return value == 0 ? 0x280u : value; };
    indirect.baseVertexLocation = location(packet[2]);
    indirect.startInstanceLocation = location(packet[3]);
    indirect.drawIndexLocation = multi ? location(packet[4] & 0xffffu) : 0x280u;
    indirect.drawIndexEnabled = multi && (packet[4] >> 31u) != 0;
    indirect.indxOffset = 0;
    require(indirect.RangeBytes() <= std::numeric_limits<std::uint64_t>::max() - indirect.arguments, "indirect draw record range overflow");
    // The counts are unknown here (the CP takes them from the record; NUM_INSTANCES is ignored).
    DrawParameters draw{0, 0, 0, 0, packet.back() & 0x20u, indexed, 0, 0};
    if (!indexed) {
        const auto offset = queue.userConfig.find(0x24a);
        require(offset != queue.userConfig.end(), "missing GE_INDX_OFFSET register");
        draw.firstVertex = offset->second;
        indirect.indxOffset = offset->second;
    } else {
        const auto indexSize = indexSizeOf(queue);
        draw.indexAddress = queue.indexBase;
        draw.indexSize = indexSize;
        if (queue.indexBufferSize) {
            require(*queue.indexBufferSize != 0, "INDEX_BUFFER_SIZE is zero");
            const auto bytes = static_cast<std::uint64_t>(*queue.indexBufferSize) * indexSize;
            require(bytes <= std::numeric_limits<std::size_t>::max() && bytes <= std::numeric_limits<std::uint64_t>::max() - queue.indexBase, "index range size overflow");
            draw.indexCount = *queue.indexBufferSize;
        } else {
            draw.indexCount = 0;
            indirect.indexLimitKnown = false;
        }
    }
    draw.indirect = indirect;
    return draw;
}

}

DrawArguments ReadDrawArguments(const DrawParameters::IndirectDraw& indirect, std::uint32_t record) {
    require(record < indirect.count, "indirect draw record index exceeds the packet's count");
    std::array<std::uint32_t, 5> words{};
    // Named for the [hooksync] attribution (the read goes through the flush hook).
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::IndirectArguments);
    GuestMemory::Read(indirect.arguments + static_cast<std::uint64_t>(record) * indirect.stride, std::as_writable_bytes(std::span(words)).first(indirect.recordBytes), 4);
    if (indirect.recordBytes == 20) return {words[0], words[1], words[2], words[3], words[4]};
    return {words[0], words[1], words[2], 0, words[3]};
}

DrawArguments ReadDrawArgumentsUnsynced(const DrawParameters::IndirectDraw& indirect, std::uint32_t record) {
    require(record < indirect.count, "indirect draw record index exceeds the packet's count");
    std::array<std::uint32_t, 5> words{};
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::IndirectArguments);
    GuestMemory::ReadCommittedUnsynced(indirect.arguments + static_cast<std::uint64_t>(record) * indirect.stride, std::as_writable_bytes(std::span(words)).first(indirect.recordBytes));
    if (indirect.recordBytes == 20) return {words[0], words[1], words[2], words[3], words[4]};
    return {words[0], words[1], words[2], 0, words[3]};
}

}
}
}
