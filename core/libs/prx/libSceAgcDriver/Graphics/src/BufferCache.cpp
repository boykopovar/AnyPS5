#include "prx/libSceAgcDriver/Graphics/include/BufferCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <iterator>

namespace AgcDriver::Graphics {

namespace {
std::size_t poolIndex(BufferCache::Use use) {
    return use == BufferCache::Use::Storage ? 0 : 1;
}
}

void BufferCache::erase(std::map<Key, Entry>::iterator entry) {
    auto& pool = pools[poolIndex(std::get<1>(entry->first))];
    pool.bytes -= std::get<2>(entry->first);
    pool.recency.erase(entry->second.recent);
    entries.erase(entry);
}

BufferCache::Slice BufferCache::Find(std::uint64_t address, std::size_t bytes, Use use) {
    ++stats.lookups;
    const auto found = use == Use::Vertex ? entries.lower_bound({address, use, bytes}) : entries.find({address, use, bytes});
    if (found == entries.end() || std::get<0>(found->first) != address || std::get<1>(found->first) != use) return {};
    const auto mappingGeneration = GuestAllocations::GuestAllocationsValidateMapping_nid_no_patch(address, std::get<2>(found->first), found->second.mappingGeneration);
    const bool mappingChanged = mappingGeneration == 0;
    if (mappingChanged || !GuestMemory::UnchangedSince(address, bytes, found->second.generation)) {
        ++stats.invalidations;
        stats.mappingInvalidations += mappingChanged;
        erase(found);
        return {};
    }
    found->second.mappingGeneration = mappingGeneration;
    auto& recency = pools[poolIndex(use)].recency;
    recency.splice(recency.end(), recency, found->second.recent);
    ++stats.hits;
    return found->second.slice;
}

void BufferCache::Keep(std::uint64_t address, std::uint64_t generation, std::uint64_t mappingGeneration, Slice slice, Use use) {
    const bool storage = use == Use::Storage;
    const auto budget = storage ? StorageBudget : GeometryBudget;
    const auto maxEntries = storage ? StorageEntries : GeometryEntries;
    auto& pool = pools[poolIndex(use)];
    const auto bytes = slice.bytes;
    if (generation == 0 || bytes == 0 || bytes > budget) return;
    if (use == Use::Vertex) {
        for (auto it = entries.lower_bound({address, use, 0}); it != entries.end() && std::get<0>(it->first) == address && std::get<1>(it->first) == use && std::get<2>(it->first) <= bytes;) erase(it++);
    } else if (const auto found = entries.find({address, use, bytes}); found != entries.end()) erase(found);
    while (!pool.recency.empty() && (pool.bytes + bytes > budget || pool.recency.size() >= maxEntries)) {
        ++stats.evictions;
        erase(entries.find(pool.recency.front()));
    }
    const Key key{address, use, bytes};
    pool.recency.push_back(key);
    try {
        entries.emplace(key, Entry{generation, mappingGeneration, std::prev(pool.recency.end()), std::move(slice)});
    } catch (...) {
        pool.recency.pop_back();
        throw;
    }
    pool.bytes += bytes;
}

}
