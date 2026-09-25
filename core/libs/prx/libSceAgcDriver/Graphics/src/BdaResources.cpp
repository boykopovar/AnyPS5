#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <list>
#include <mutex>
#include <sstream>

namespace AgcDriver::Graphics {

namespace {

// The page tables built per device, reused by a build whose ranges (guest ranges, device
// addresses, permissions) equal one of them: consecutive address-based builds map the same registered
// set through the same imports and mirrors, so the ~1200-entry table repeats. The shader only reads
// the table (its lookup keeps its state in function variables), and a batch in flight keeps its
// BdaResources, so sharing the buffer is safe. The cache holds the tables weakly: they never outlive
// the builds that own them (a static owner would destroy the buffer after its device, at exit or on
// device replacement, and would fail the tests' leak check), and with deferred lease release the
// previous builds are still in flight when the next one asks, which is what makes the hit rate.
// One table was enough while every build repeated the last one exactly; now the misaligned
// descriptor sub-ranges the GPU copies out of imports sit in pool staging buffers whose device
// addresses cycle through the few buffers the batches in flight leave free, and the three
// address-based programs of one queue interleave with the lease draws, so a table differs from the
// previous one and comes back a few builds later (69% misses in the wave-8 run, 4% before the GPU
// copies). Hence several recent tables, most recently used first, matched by a word hash before
// the byte compare. APS5_BDA_TABLE_CACHE_ENTRIES sets how many (default 8; 1 is the previous
// behaviour); APS5_NO_BDA_TABLE_CACHE=1 builds every table.
struct TableEntry {
    std::uint64_t hash;
    std::vector<ShaderRecompiler::BdaAbi::Range> ranges;
    std::weak_ptr<Buffer> buffer;
};

struct TableCache {
    std::mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    std::list<TableEntry> entries;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
};

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

BdaResources::BdaResources(const Context& context) {
    static_assert(std::endian::native == std::endian::little);
    Require(sizeof(ShaderRecompiler::BdaAbi::Fault) <= context.limits.maxStorageBufferRange, "BDA fault buffer exceeds storage buffer range limit");
    fault = std::make_unique<Buffer>(context, sizeof(ShaderRecompiler::BdaAbi::Fault), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(fault->Bytes().data(), 0, fault->Bytes().size());
}

BdaResources::BdaResources(const Context& context, const GuestBufferMemory& memory) : BdaResources(context) {
    auto ranges = memory.AddressRanges();
    Require(ranges.size() <= std::numeric_limits<std::uint32_t>::max(), "BDA table range count overflow");
    Require(ranges.size() <= (std::numeric_limits<std::size_t>::max() - sizeof(ShaderRecompiler::BdaAbi::Header)) / sizeof(ShaderRecompiler::BdaAbi::Range), "BDA table size overflow");
    tableBytes = sizeof(ShaderRecompiler::BdaAbi::Header) + ranges.size() * sizeof(ShaderRecompiler::BdaAbi::Range);
    Require(tableBytes <= context.limits.maxStorageBufferRange && sizeof(ShaderRecompiler::BdaAbi::Fault) <= context.limits.maxStorageBufferRange, "BDA descriptors exceed storage buffer range limit");
    auto& cache = Tables();
    const auto hash = tableCacheEnabled() ? hashRanges(ranges) : 0;
    if (tableCacheEnabled()) {
        std::lock_guard lock(cache.mutex);
        // Another device's tables are not this one's (their buffers are dead or foreign).
        if (cache.device != context.device) {
            cache.entries.clear();
            cache.device = context.device;
        }
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
                table = std::move(shared);
                cache.entries.splice(cache.entries.begin(), cache.entries, it);
                return;
            }
            ++it;
        }
    }
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
        cache.entries.push_front({hash, std::move(ranges), table});
        while (cache.entries.size() > tableCacheEntries()) cache.entries.pop_back();
    }
}

BdaResources::TableCacheStats BdaResources::TableCacheCounters() {
    auto& cache = Tables();
    std::lock_guard lock(cache.mutex);
    return {cache.hits, cache.misses, cache.entries.size()};
}

VkDescriptorBufferInfo BdaResources::Table() const {
    Require(table != nullptr, "BDA page table was not requested");
    return {table->Handle(), 0, tableBytes};
}

VkDescriptorBufferInfo BdaResources::Fault() const {
    return {fault->Handle(), 0, sizeof(ShaderRecompiler::BdaAbi::Fault)};
}

void BdaResources::CheckFault() const {
    ShaderRecompiler::BdaAbi::Fault report{};
    std::memcpy(&report, fault->Bytes().data(), sizeof(report));
    if (report.state == ShaderRecompiler::BdaAbi::FaultState::Empty) {
        Require(static_cast<std::uint32_t>(report.reason) == 0 && report.address == 0 && report.bytes == 0 && report.stage == 0 && report.instruction == 0 && report.reserved == 0, "BDA fault record has data without publication");
        return;
    }
    Require(report.state == ShaderRecompiler::BdaAbi::FaultState::Ready && report.reserved == 0, "incomplete or invalid BDA fault record");
    Require(report.reason != ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle, "rect-list requires finite nondegenerate axis-aligned positions with equal positive W");
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::LoopLimit) {
        // APS5_LOOP_GUARD: the shader left a loop that ran past the guard; the dispatch result is kept.
        std::fprintf(stderr, "[gpu] loop guard: the loop exit at pc 0x%x ran past %u evaluations\n", report.instruction, report.bytes);
        std::memset(fault->Bytes().data(), 0, fault->Bytes().size());
        return;
    }
    std::ostringstream message;
    message << "BDA access failed: address=0x" << std::hex << report.address << " instruction=0x" << report.instruction << std::dec << " bytes=" << report.bytes << " stage=" << report.stage << " reason=" << static_cast<std::uint32_t>(report.reason);
    throw std::runtime_error(message.str());
}

}
