#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "ImageTableAbi.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <list>
#include <mutex>
#include <iomanip>
#include <span>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

namespace {

struct TableEntry {
    std::uint64_t hash;
    std::vector<ShaderRecompiler::BdaAbi::Range> ranges;
    std::weak_ptr<Buffer> buffer;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> spaceKeys;
};

struct TableCache {
    AgcDriver::Mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    std::list<TableEntry> entries;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    BdaResources::TableCacheStats classes;
};

bool sameRanges(const std::vector<ShaderRecompiler::BdaAbi::Range>& left, const std::vector<ShaderRecompiler::BdaAbi::Range>& right);

void classifyFirstEntry(TableCache& cache, std::uint64_t hash, const std::vector<ShaderRecompiler::BdaAbi::Range>& ranges) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& classes = cache.classes;
    if (cache.entries.empty()) {
        ++classes.firstEmpty;
        return;
    }
    const auto& first = cache.entries.front();
    if (first.hash == hash) {
        if (first.buffer.expired()) ++classes.firstExpired;
        else if (!sameRanges(first.ranges, ranges)) ++classes.firstSameHash;
        return;
    }
    if (first.buffer.expired()) {
        ++classes.firstExpired;
        return;
    }
    const auto common = std::min(first.ranges.size(), ranges.size());
    std::size_t at = 0;
    while (at < common && std::memcmp(&first.ranges[at], &ranges[at], sizeof(ranges[at])) == 0) ++at;
    const auto begin = at < ranges.size() ? ranges[at].begin : at < first.ranges.size() ? first.ranges[at].begin : 0;
    ++(begin < 0x10000000ull ? classes.firstDiffersLow : classes.firstDiffersHeap);
}

TableCache& Tables() {
    static TableCache cache;
    return cache;
}

bool tableCacheEnabled() {
    static const bool disabled = std::getenv("APS5_NO_BDA_TABLE_CACHE") != nullptr;
    return !disabled;
}

std::size_t tableCacheEntries() {
    static const std::size_t entries = [] {
        const char* value = std::getenv("APS5_BDA_TABLE_CACHE_ENTRIES");
        const auto parsed = value != nullptr ? std::strtoull(value, nullptr, 10) : 8ull;
        return static_cast<std::size_t>(std::clamp<unsigned long long>(parsed, 1, 64));
    }();
    return entries;
}

bool sameRanges(const std::vector<ShaderRecompiler::BdaAbi::Range>& left, const std::vector<ShaderRecompiler::BdaAbi::Range>& right) {
    return left.size() == right.size() && (left.empty() || std::memcmp(left.data(), right.data(), left.size() * sizeof(left.front())) == 0);
}

// A 64-bit mix of the table's words: a table is ~1200 ranges of four words, and comparing the bytes
// against every held table would cost as much as building it.
std::uint64_t hashRanges(const std::vector<ShaderRecompiler::BdaAbi::Range>& ranges) {
    static_assert(sizeof(ShaderRecompiler::BdaAbi::Range) % sizeof(std::uint64_t) == 0);
    std::uint64_t hash = 0x9e3779b97f4a7c15ull ^ ranges.size();
    for (const auto& range : ranges) {
        std::uint64_t words[sizeof(range) / sizeof(std::uint64_t)];
        std::memcpy(words, &range, sizeof(range));
        for (const auto word : words) {
            hash ^= word;
            hash *= 0xff51afd7ed558ccdull;
            hash ^= hash >> 33u;
        }
    }
    return hash;
}

}

struct BdaFaultChunk {
    std::unique_ptr<Buffer> buffer;
    std::vector<VkDeviceSize> free;
    std::size_t slots = 0;
    VkDeviceSize slotBytes = 0;
};

namespace {

struct FaultArena {
    std::vector<std::shared_ptr<BdaFaultChunk>> chunks;
    std::unordered_set<BdaResources*> holders;
    std::uint64_t finished = 0;
    BdaResources::FaultSlotCounts counts;
};

struct FaultArenas {
    std::mutex mutex;
    std::unordered_map<VkDevice, FaultArena> devices;
};

FaultArenas& faultArenas() {
    static auto* arenas = new FaultArenas();
    return *arenas;
}

bool sharedFaultSlots() {
    static const bool shared = std::getenv("APS5_NO_SHARED_FAULT_SLOTS") == nullptr;
    return shared;
}

constexpr VkDeviceSize FaultChunkBytes = VkDeviceSize{512} * 1024;

VkDeviceSize faultSlotBytes(bool reportOnly) {
    return reportOnly ? sizeof(ShaderRecompiler::BdaAbi::Fault) : ShaderRecompiler::BdaAbi::FaultBufferBytes;
}

VkDeviceSize faultStride(const Context& context, VkDeviceSize bytes) {
    const auto alignment = std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, sizeof(std::uint32_t));
    return (bytes + alignment - 1) / alignment * alignment;
}

bool faultSlotClean(std::span<const std::byte> bytes) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    if (bytes.size() < Abi::FaultBufferBytes) return std::all_of(bytes.begin(), bytes.end(), [](std::byte value) { return value == std::byte{0}; });
    std::array<std::uint32_t, Abi::WrittenSlotsWord> head{};
    std::memcpy(head.data(), bytes.data(), sizeof(head));
    std::uint32_t guard = 0;
    std::memcpy(&guard, bytes.data() + Abi::SrtGuardWord * sizeof(std::uint32_t), sizeof(guard));
    return guard == 0 && std::all_of(head.begin(), head.end(), [](std::uint32_t word) { return word == 0; });
}

void clearFaultSlot(std::span<std::byte> bytes) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    if (bytes.size() < Abi::FaultBufferBytes) {
        std::memset(bytes.data(), 0, bytes.size());
        return;
    }
    std::memset(bytes.data(), 0, Abi::WrittenListWord * sizeof(std::uint32_t));
    std::memset(bytes.data() + Abi::SrtGuardWord * sizeof(std::uint32_t), 0, sizeof(std::uint32_t));
}

void dropChunk(FaultArena& arena, const std::shared_ptr<BdaFaultChunk>& chunk, std::vector<std::shared_ptr<BdaFaultChunk>>& dropped) {
    const auto found = std::find(arena.chunks.begin(), arena.chunks.end(), chunk);
    if (found == arena.chunks.end()) return;
    arena.counts.slots -= (*found)->slots;
    --arena.counts.chunks;
    dropped.push_back(std::move(*found));
    arena.chunks.erase(found);
}

}

BdaResources::BdaResources(const Context& context, bool reportOnly) : reportOnly(reportOnly) {
    static_assert(std::endian::native == std::endian::little);
    Require(ShaderRecompiler::BdaAbi::FaultBufferBytes <= context.limits.maxStorageBufferRange, "BDA fault buffer exceeds storage buffer range limit");
    TakeFaultSlot(context);
}

BdaResources::~BdaResources() {
    std::vector<std::shared_ptr<BdaFaultChunk>> dropped;
    auto& arenas = faultArenas();
    std::lock_guard lock(arenas.mutex);
    if (faultChunk == nullptr) return;
    const auto bytes = faultBytes();
    if (!faultSlotClean(bytes)) clearFaultSlot(bytes);
    const auto chunk = faultChunk;
    dropped.push_back(std::move(faultChunk));
    const auto found = arenas.devices.find(faultDevice);
    if (found == arenas.devices.end()) return;
    auto& arena = found->second;
    arena.holders.erase(this);
    --arena.counts.held;
    if (!sharedFaultSlots()) {
        arena.counts.slots -= chunk->slots;
        --arena.counts.chunks;
    } else {
        chunk->free.push_back(faultOffset);
        if (chunk->free.size() == chunk->slots) dropChunk(arena, chunk, dropped);
    }
    if (arena.holders.empty()) {
        while (!arena.chunks.empty()) dropChunk(arena, arena.chunks.back(), dropped);
        arenas.devices.erase(found);
    }
}

std::span<std::byte> BdaResources::faultBytes() const {
    return faultChunk->buffer->Bytes().subspan(static_cast<std::size_t>(faultOffset), static_cast<std::size_t>(faultChunk->slotBytes));
}

void BdaResources::NoteUse(std::uint64_t serial) {
    auto& arenas = faultArenas();
    std::lock_guard lock(arenas.mutex);
    lastUse = std::max(lastUse, serial);
}

void BdaResources::NoteFinished(VkDevice device, std::uint64_t serial) {
    auto& arenas = faultArenas();
    std::lock_guard lock(arenas.mutex);
    const auto found = arenas.devices.find(device);
    if (found != arenas.devices.end()) found->second.finished = serial;
}

BdaResources::FaultSlotCounts BdaResources::FaultSlots(VkDevice device) {
    auto& arenas = faultArenas();
    std::lock_guard lock(arenas.mutex);
    const auto found = arenas.devices.find(device);
    return found != arenas.devices.end() ? found->second.counts : FaultSlotCounts{};
}

bool BdaResources::TakeFaultSlot(const Context& context) {
    if (faultChunk != nullptr) return false;
    std::vector<std::shared_ptr<BdaFaultChunk>> dropped;
    auto& arenas = faultArenas();
    std::lock_guard lock(arenas.mutex);
    auto& arena = arenas.devices[context.device];
    const bool retake = faultDevice != VK_NULL_HANDLE;
    if (!retake) lastUse = arena.finished + 1;
    faultDevice = context.device;
    const bool shared = sharedFaultSlots();
    const auto slotBytes = shared ? faultSlotBytes(reportOnly) : VkDeviceSize{ShaderRecompiler::BdaAbi::FaultBufferBytes};
    const auto withFree = [&] { return std::find_if(arena.chunks.rbegin(), arena.chunks.rend(), [&](const std::shared_ptr<BdaFaultChunk>& chunk) { return chunk->slotBytes == slotBytes && !chunk->free.empty(); }); };
    auto source = shared ? withFree() : arena.chunks.rend();
    bool reclaimed = false;
    if (shared && source == arena.chunks.rend() && GuestMemory::GpuMutex().HeldByThisThread()) {
        std::vector<BdaResources*> idle;
        for (auto* holder : arena.holders) {
            if (holder->faultChunk->slotBytes == slotBytes && holder->lastUse <= arena.finished && faultSlotClean(holder->faultBytes())) idle.push_back(holder);
        }
        if (!idle.empty()) {
            const auto reclaim = (idle.size() + 1) / 2;
            std::nth_element(idle.begin(), idle.begin() + static_cast<std::ptrdiff_t>(reclaim - 1), idle.end(), [](const BdaResources* left, const BdaResources* right) { return left->lastUse < right->lastUse; });
            for (std::size_t index = 0; index < reclaim; ++index) {
                auto& holder = *idle[index];
                holder.faultChunk->free.push_back(holder.faultOffset);
                holder.faultChunk.reset();
                arena.holders.erase(&holder);
                --arena.counts.held;
                ++arena.counts.reclaimed;
            }
            reclaimed = true;
            source = withFree();
        }
    }
    std::shared_ptr<BdaFaultChunk> chunk;
    if (source != arena.chunks.rend()) {
        chunk = *source;
    } else {
        const auto stride = shared ? faultStride(context, slotBytes) : slotBytes;
        const auto slots = shared ? std::max<VkDeviceSize>(1, FaultChunkBytes / stride) : 1;
        chunk = std::make_shared<BdaFaultChunk>();
        chunk->buffer = std::make_unique<Buffer>(context, static_cast<std::size_t>(stride * slots), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        std::memset(chunk->buffer->Bytes().data(), 0, chunk->buffer->Bytes().size());
        chunk->slots = static_cast<std::size_t>(slots);
        chunk->slotBytes = slotBytes;
        for (auto slot = slots; slot-- > 0;) chunk->free.push_back(slot * stride);
        if (shared) arena.chunks.push_back(chunk);
        arena.counts.slots += chunk->slots;
        ++arena.counts.chunks;
    }
    faultOffset = chunk->free.back();
    chunk->free.pop_back();
    faultChunk = chunk;
    arena.holders.insert(this);
    ++arena.counts.held;
    if (retake) ++arena.counts.retaken;
    bool spare = false;
    for (auto it = arena.chunks.begin(); reclaimed && it != arena.chunks.end();) {
        if ((*it)->free.size() != (*it)->slots || !std::exchange(spare, true)) {
            ++it;
            continue;
        }
        arena.counts.slots -= (*it)->slots;
        --arena.counts.chunks;
        dropped.push_back(std::move(*it));
        it = arena.chunks.erase(it);
    }
    return true;
}

BdaResources::BdaResources(const Context& context, const GuestBufferMemory& memory, bool reportOnly) : BdaResources(context, reportOnly) {
    const auto cached = memory.CachedAddressTable();
    std::vector<ShaderRecompiler::BdaAbi::Range> built;
    if (!cached.has_value()) built = memory.AddressRanges();
    const auto& addressRanges = cached.has_value() ? *cached->ranges : built;
    const auto serial = cached.has_value() ? cached->serial : 0;
    auto guardSerial = MultisampleTargetRangeGeneration();
    if (tableCacheEnabled() && serial != 0) {
        auto& cache = Tables();
        std::lock_guard lock(cache.mutex);
        if (cache.device != context.device) {
            cache.entries.clear();
            cache.device = context.device;
        }
        for (auto it = cache.entries.begin(); it != cache.entries.end(); ++it) {
            const auto key = std::pair{serial, guardSerial};
            if (std::find(it->spaceKeys.begin(), it->spaceKeys.end(), key) == it->spaceKeys.end()) continue;
            if (auto shared = it->buffer.lock(); shared != nullptr) {
                tableBytes = shared->Bytes().size();
                tableGuardSerial = guardSerial;
                ++cache.hits;
                ++cache.classes.spaceTables;
                table = std::move(shared);
                cache.entries.splice(cache.entries.begin(), cache.entries, it);
                return;
            }
            cache.entries.erase(it);
            break;
        }
    }
    auto filtered = ExcludeMultisampleTargetRanges(context.device, addressRanges, guardSerial);
    tableGuardSerial = guardSerial;
    const auto& ranges = filtered.has_value() ? *filtered : addressRanges;
    Require(ranges.size() <= std::numeric_limits<std::uint32_t>::max(), "BDA table range count overflow");
    Require(ranges.size() <= (std::numeric_limits<std::size_t>::max() - sizeof(ShaderRecompiler::BdaAbi::Header)) / sizeof(ShaderRecompiler::BdaAbi::Range), "BDA table size overflow");
    tableBytes = sizeof(ShaderRecompiler::BdaAbi::Header) + ranges.size() * sizeof(ShaderRecompiler::BdaAbi::Range);
    Require(tableBytes <= context.limits.maxStorageBufferRange && ShaderRecompiler::BdaAbi::FaultBufferBytes <= context.limits.maxStorageBufferRange, "BDA descriptors exceed storage buffer range limit");
    auto& cache = Tables();
    std::uint64_t hash = 0;
    if (tableCacheEnabled()) {
        std::lock_guard lock(cache.mutex);
        // Another device's tables are not this one's (their buffers are dead or foreign).
        if (cache.device != context.device) {
            cache.entries.clear();
            cache.device = context.device;
        }
        hash = hashRanges(ranges);
        classifyFirstEntry(cache, hash, ranges);
        for (auto it = cache.entries.begin(); it != cache.entries.end();) {
            // Compare the hash before taking a strong reference: locking a non-matching entry's
            // buffer could make this thread its last owner (a concurrent reap releasing it) and
            // run the pool release under the cache mutex; expired() takes no reference.
            if (it->hash != hash) {
                if (it->buffer.expired()) {
                    it = cache.entries.erase(it);
                } else {
                    ++it;
                }
                continue;
            }
            auto shared = it->buffer.lock();
            if (shared == nullptr) {
                // Every build that owned the table is gone, and its buffer with it.
                it = cache.entries.erase(it);
                continue;
            }
            if (sameRanges(it->ranges, ranges)) {
                ++cache.hits;
                if (serial != 0) {
                    const auto key = std::pair{serial, guardSerial};
                    auto found = std::find(it->spaceKeys.begin(), it->spaceKeys.end(), key);
                    if (found != it->spaceKeys.end()) it->spaceKeys.erase(found);
                    it->spaceKeys.push_back(key);
                    if (it->spaceKeys.size() > tableCacheEntries()) it->spaceKeys.erase(it->spaceKeys.begin());
                }
                table = std::move(shared);
                cache.entries.splice(cache.entries.begin(), cache.entries, it);
                return;
            }
            ++it;
        }
    }
    Require(ShaderRecompiler::BdaAbi::RangesOrdered(ranges.data(), ranges.size()), "BDA table ranges do not ascend by begin");
    for (const auto& range : ranges) Require((range.permissions & ShaderRecompiler::BdaAbi::Write) == 0 || range.end <= ShaderRecompiler::BdaAbi::WrittenAddressLimit, "a writable BDA range ends past the addresses the written-page record can hold");
    table = std::make_shared<Buffer>(context, tableBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const ShaderRecompiler::BdaAbi::Header header{ShaderRecompiler::BdaAbi::Version, static_cast<std::uint32_t>(ranges.size()), sizeof(ShaderRecompiler::BdaAbi::Range), 0};
    std::memcpy(table->Bytes().data(), &header, sizeof(header));
    if (!ranges.empty()) std::memcpy(table->Bytes().data() + sizeof(header), ranges.data(), ranges.size() * sizeof(ranges.front()));
    if (tableCacheEnabled()) {
        std::lock_guard lock(cache.mutex);
        ++cache.misses;
        if (cache.device != context.device) {
            cache.entries.clear();
            cache.device = context.device;
        }
        auto cachedRanges = filtered.has_value() ? std::move(*filtered) : cached.has_value() ? *cached->ranges : std::move(built);
        cache.entries.push_front({hash, std::move(cachedRanges), table, {}});
        if (serial != 0) cache.entries.front().spaceKeys.emplace_back(serial, guardSerial);
        while (cache.entries.size() > tableCacheEntries()) cache.entries.pop_back();
    }
}

BdaResources::TableCacheStats BdaResources::TableCacheCounters() {
    auto& cache = Tables();
    std::lock_guard lock(cache.mutex);
    auto stats = cache.classes;
    stats.hits = cache.hits;
    stats.misses = cache.misses;
    stats.held = cache.entries.size();
    return stats;
}

VkDescriptorBufferInfo BdaResources::Table() const {
    Require(table != nullptr, "BDA page table was not requested");
    return {table->Handle(), 0, tableBytes};
}

std::span<const std::byte> BdaResources::TableBytes() const {
    if (table == nullptr) return {};
    return table->Bytes().first(tableBytes);
}

bool BdaResources::TableGuardCurrent(VkDevice device, std::span<const ShaderRecompiler::BdaAbi::Range> ranges) {
    if (table == nullptr || tableGuardSerial == MultisampleTargetRangeGeneration()) return true;
    auto generation = tableGuardSerial;
    const auto filtered = ExcludeMultisampleTargetRanges(device, ranges, generation);
    const auto expected = filtered.has_value() ? std::span<const ShaderRecompiler::BdaAbi::Range>(*filtered) : ranges;
    const auto bytes = TableBytes();
    ShaderRecompiler::BdaAbi::Header header{};
    if (bytes.size() < sizeof(header)) return false;
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.count != expected.size() || (bytes.size() - sizeof(header)) / sizeof(ShaderRecompiler::BdaAbi::Range) < expected.size()) return false;
    if (!expected.empty() && std::memcmp(bytes.data() + sizeof(header), expected.data(), expected.size() * sizeof(ShaderRecompiler::BdaAbi::Range)) != 0) return false;
    tableGuardSerial = generation;
    return true;
}

}
