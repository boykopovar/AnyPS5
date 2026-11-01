#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "ImageTableAbi.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <list>
#include <mutex>
#include <iomanip>
#include <sstream>

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

BdaResources::BdaResources(const Context& context) {
    static_assert(std::endian::native == std::endian::little);
    Require(ShaderRecompiler::BdaAbi::FaultBufferBytes <= context.limits.maxStorageBufferRange, "BDA fault buffer exceeds storage buffer range limit");
    fault = std::make_unique<Buffer>(context, ShaderRecompiler::BdaAbi::FaultBufferBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memset(fault->Bytes().data(), 0, ShaderRecompiler::BdaAbi::WrittenListWord * sizeof(std::uint32_t));
    std::memset(fault->Bytes().data() + ShaderRecompiler::BdaAbi::SrtGuardWord * sizeof(std::uint32_t), 0, ShaderRecompiler::BdaAbi::FaultBufferBytes - ShaderRecompiler::BdaAbi::SrtGuardWord * sizeof(std::uint32_t));
}

std::uint32_t BdaResources::StorePageSlots() {
    static const std::uint32_t slots = [] {
        const char* value = std::getenv("APS5_BDA_STORE_PAGES");
        const auto parsed = value != nullptr ? std::strtoull(value, nullptr, 10) : 1024ull;
        return std::bit_ceil(static_cast<std::uint32_t>(std::clamp<unsigned long long>(parsed, 64, 65536)));
    }();
    return slots;
}

BdaResources::BdaResources(const Context& context, const GuestBufferMemory& memory) : BdaResources(context) {
    namespace Abi = ShaderRecompiler::BdaAbi;
    const auto spans = memory.AddressStoreSpans();
    if (!spans.empty()) {
        pageSlots = StorePageSlots();
        pageBase = spans.front().begin / Abi::StorePageBytes * Abi::StorePageBytes;
        Require(spans.back().end - pageBase <= Abi::StorePageSpan, "heap mirrors stored through addresses that span more than 16 TiB are not implemented");
        fault = std::make_unique<Buffer>(context, Abi::FaultBufferBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        std::memset(fault->Bytes().data(), 0, Abi::FaultBufferBytes);
        poolDevice = context.device;
        if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->Device() == context.device) pages = recorder->TakeStorePagePool(Abi::StorePagePoolBytes(pageSlots));
        if (pages == nullptr) {
            pages = std::make_unique<Buffer>(context, Abi::StorePagePoolBytes(pageSlots), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            std::memset(pages->Bytes().data(), 0, Abi::StorePagePoolBytes(pageSlots));
        }
        std::vector<std::uint64_t> words;
        for (const auto& span : spans) {
            if (!span.resident) continue;
            words.insert(words.end(), {span.begin, span.end, span.device, 0});
            ++targetCount;
        }
        if (targetCount != 0) {
            targets = std::make_unique<Buffer>(context, words.size() * sizeof(std::uint64_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            std::memcpy(targets->Bytes().data(), words.data(), words.size() * sizeof(std::uint64_t));
        }
        attachStorePages();
    }
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

void BdaResources::attachStorePages() const {
    namespace Abi = ShaderRecompiler::BdaAbi;
    if (pages == nullptr) return;
    auto* words = reinterpret_cast<std::uint32_t*>(fault->Bytes().data());
    const auto address = pages->DeviceAddress();
    words[Abi::StorePagesWord] = static_cast<std::uint32_t>(address);
    words[Abi::StorePagesWord + 1] = static_cast<std::uint32_t>(address >> 32u);
    words[Abi::StorePageBaseWord] = static_cast<std::uint32_t>(pageBase);
    words[Abi::StorePageBaseWord + 1] = static_cast<std::uint32_t>(pageBase >> 32u);
    words[Abi::StorePageSlotsWord] = pageSlots;
    words[Abi::StorePageOverflowWord] = 0;
}

void BdaResources::RecordStorePageGather(Recorder& recorder) const {
    if (pages == nullptr || targetCount == 0) return;
    recorder.RecordStorePageGather(recorder.Commands(), {fault->DeviceAddress(), pages->DeviceAddress(), targets->DeviceAddress(), pageBase, targetCount, pageSlots});
}

std::vector<GuestBufferMemory::StorePage> BdaResources::TakeStorePages() const {
    return std::exchange(storePages, {});
}

void BdaResources::collectStorePages() const {
    namespace Abi = ShaderRecompiler::BdaAbi;
    storePages.clear();
    if (pages == nullptr) return;
    auto* faultWords = reinterpret_cast<std::uint32_t*>(fault->Bytes().data());
    pagesOverflowed = faultWords[Abi::StorePageOverflowWord] != 0;
    faultWords[Abi::StorePageOverflowWord] = 0;
    auto* bytes = pages->Bytes().data();
    auto* words = reinterpret_cast<std::uint32_t*>(bytes);
    for (std::uint32_t slot = 0; slot < pageSlots; ++slot) {
        if (words[slot] == 0) continue;
        GuestBufferMemory::StorePage page;
        page.address = pageBase + (static_cast<std::uint64_t>(words[slot] - 1u) << Abi::StorePageShift);
        auto* data = bytes + Abi::StorePageWord(pageSlots, slot) * sizeof(std::uint32_t);
        auto* mask = words + Abi::StoreMaskWord(pageSlots, slot);
        page.data.assign(data, data + Abi::StorePageBytes);
        std::copy(mask, mask + Abi::StoreMaskWords, page.mask.begin());
        std::memset(data, 0, Abi::StorePageBytes);
        std::memset(mask, 0, Abi::StoreMaskWords * sizeof(std::uint32_t));
        words[slot] = 0;
        storePages.push_back(std::move(page));
    }
    if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->Device() == poolDevice) recorder->KeepStorePagePool(std::move(pages));
}

VkDescriptorBufferInfo BdaResources::Fault() const {
    return {fault->Handle(), 0, ShaderRecompiler::BdaAbi::FaultBufferBytes};
}

namespace {
std::atomic<bool> loopGuardTripped{false};
}

bool LoopGuardTripped() {
    return loopGuardTripped.load(std::memory_order_relaxed);
}

void BdaResources::CheckFault() const {
    collectStorePages();
    markWrittenPages();
    ShaderRecompiler::BdaAbi::Fault report{};
    std::memcpy(&report, fault->Bytes().data(), sizeof(report));
    if (report.state == ShaderRecompiler::BdaAbi::FaultState::Empty) {
        Require(static_cast<std::uint32_t>(report.reason) == 0 && report.address == 0 && report.bytes == 0 && report.stage == 0 && report.instruction == 0 && report.otherFaults == 0, "BDA fault record has data without publication");
        Require(!pagesOverflowed, "the GPU stored to more pages of resident heap mirrors in one use than the " + std::to_string(pageSlots) + " store pages hold (APS5_BDA_STORE_PAGES)");
        checkSrtGuard();
        return;
    }
    Require(report.state == ShaderRecompiler::BdaAbi::FaultState::Ready && report.otherFaults <= 1, "incomplete or invalid BDA fault record");
    Require(report.reason != ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle, "rect-list requires finite nondegenerate axis-aligned positions with equal positive W");
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::LoopLimit) {
        // APS5_LOOP_GUARD: the shader left a loop that ran past the guard; the dispatch result is kept.
        loopGuardTripped.store(true, std::memory_order_relaxed);
        std::fprintf(stderr, "[gpu] loop guard: the loop exit at pc 0x%x of shader 0x%llx ran past %u evaluations\n", report.instruction, static_cast<unsigned long long>(report.address), report.bytes);
        std::memset(fault->Bytes().data(), 0, fault->Bytes().size());
        attachStorePages();
        return;
    }
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::ImageTable) {
        const auto table = std::find_if(imageTables.begin(), imageTables.end(), [&](const ImageTableFaults& candidate) { return candidate.stage == report.stage; });
        const auto* tables = table != imageTables.end() ? &*table : imageTables.size() == 1 ? &imageTables.front() : nullptr;
        std::ostringstream message;
        message << "image table: shader 0x" << std::hex << (tables != nullptr ? tables->shader : 0u) << std::dec << " stage " << report.stage << " pc 0x" << std::hex << report.instruction << " selected the descriptor at 0x" << report.address << std::dec;
        if (tables != nullptr && report.bytes < tables->poison.size()) {
            const auto& poison = tables->poison[report.bytes];
            if (poison.dwordCount != 0) {
                message << " (words";
                for (std::uint32_t index = 0; index < poison.dwordCount && index < poison.words.size(); ++index) message << ' ' << std::hex << std::setw(8) << std::setfill('0') << poison.words[index];
                message << std::dec << ")";
            }
            message << ": " << ShaderRecompiler::ImageTableAbi::PoisonReasonName(static_cast<ShaderRecompiler::ImageTableAbi::PoisonReason>(poison.reason));
            if (report.bytes < tables->messages.size() && tables->messages[report.bytes] != nullptr && !tables->messages[report.bytes]->empty()) message << ": " << *tables->messages[report.bytes];
        } else {
            message << ": poison entry " << report.bytes;
        }
        throw std::runtime_error(message.str());
    }
    std::ostringstream message;
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::UnlinkedCall) {
        message << "shader stage " << report.stage << " called address 0x" << std::hex << report.address << " at program counter 0x" << report.instruction << ", which no linked callee of that call matches";
        throw std::runtime_error(message.str());
    }
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::StorePages) {
        message << "BDA store at 0x" << std::hex << report.address << std::dec << " (" << report.bytes << " bytes, instruction 0x" << std::hex << report.instruction << std::dec << ") into a heap mirror found none of the " << pageSlots << " store pages free (APS5_BDA_STORE_PAGES)";
        throw std::runtime_error(message.str());
    }
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::WindowLaneSpill) {
        message << "shader stage " << report.stage << ": a relative write stored " << report.bytes << " in lane " << (report.address >> 32u) << " of vector window slot " << (report.address & 0xffffffffu) << ", which holds a scalar spilled with v_writelane_b32 that a later v_readlane_b32 was replaced with";
        throw std::runtime_error(message.str());
    }
    message << "BDA access failed: address=0x" << std::hex << report.address << " instruction=0x" << report.instruction << std::dec << " bytes=" << report.bytes << " stage=" << report.stage << " reason=" << static_cast<std::uint32_t>(report.reason);
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::Permission && table != nullptr) {
        const auto bytes = table->Bytes();
        ShaderRecompiler::BdaAbi::Header header{};
        std::memcpy(&header, bytes.data(), sizeof(header));
        for (std::uint32_t index = 0; index < header.count; ++index) {
            ShaderRecompiler::BdaAbi::Range range{};
            std::memcpy(&range, bytes.data() + sizeof(header) + index * sizeof(range), sizeof(range));
            if (report.address < range.begin || report.address >= range.end) continue;
            message << std::hex << "; the store hit 0x" << range.begin << "+0x" << range.end - range.begin << ", read-only in the BDA table: stores through GPU-selected descriptors and FLAT/GLOBAL addresses reach only writable ranges imported in place, not ones served by a mirror or a copy (past APS5_HOST_IMPORT_MIB, or refused by the driver)";
            break;
        }
    }
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::Unaligned) {
        if (report.otherFaults == 0) throw BdaUnalignedAccess(message.str(), report.instruction);
        message << "; a fault of another kind was recorded in the same dispatch";
    }
    throw std::runtime_error(message.str());
}

}

namespace AgcDriver::Graphics {

void BdaResources::SetImageTables(std::vector<ImageTableFaults> tables, std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges) {
    imageTables = std::move(tables);
    imageTableRanges = std::move(ranges);
}

void BdaResources::SetSrtGuards(std::vector<SrtGuardFaults> guards) {
    srtGuards = std::move(guards);
}

void BdaResources::checkSrtGuard() const {
    namespace Abi = ShaderRecompiler::BdaAbi;
    std::uint32_t guard = 0;
    std::memcpy(&guard, fault->Bytes().data() + Abi::SrtGuardWord * sizeof(std::uint32_t), sizeof(guard));
    if (guard == 0) return;
    const auto stage = guard >> Abi::SrtGuardStageShift;
    const auto record = guard & ((1u << Abi::SrtGuardStageShift) - 1u);
    const auto found = std::find_if(srtGuards.begin(), srtGuards.end(), [&](const SrtGuardFaults& candidate) { return candidate.stage == stage; });
    const auto* guards = found != srtGuards.end() ? &*found : srtGuards.size() == 1 ? &srtGuards.front() : nullptr;
    Require(guards != nullptr && record + 2u < guards->flattenedSrt.size(), "a guarded SRT read faulted without its poison record");
    const auto& words = guards->flattenedSrt;
    const auto address = static_cast<std::uint64_t>(words[record + 1u]) | (static_cast<std::uint64_t>(words[record + 2u]) << 32u);
    std::ostringstream message;
    message << "BDA access failed: address=0x" << std::hex << address << " instruction=0x" << words[record] << std::dec << " bytes=4 stage=" << stage << " reason=" << static_cast<std::uint32_t>(Abi::FaultReason::Unmapped);
    throw std::runtime_error(message.str());
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> BdaResources::TakeWrittenPages() const {
    return std::exchange(writtenPages, {});
}

void BdaResources::markWrittenPages() const {
    namespace Abi = ShaderRecompiler::BdaAbi;
    auto* words = reinterpret_cast<std::uint32_t*>(fault->Bytes().data());
    Require(words[Abi::WrittenOverflowWord] == 0, "more than " + std::to_string(Abi::WrittenPageSlots) + " chunks of " + std::to_string(Abi::WrittenChunkPages) + " pages stored to through the BDA table in one use are not implemented");
    const auto count = words[Abi::WrittenCountWord];
    if (count == 0) return;
    Require(Abi::ForEachWrittenRange(words, [&](std::uint64_t begin, std::size_t bytes) {
        const auto end = begin + bytes;
        for (const auto& [base, size] : imageTableRanges) {
            if (size != 0 && begin < base + size && base < end) {
                std::ostringstream message;
                message << "image table at 0x" << std::hex << base << " is written by its own dispatch";
                Require(false, message.str());
            }
        }
        GuestMemory::MarkPossiblyWritten(begin, bytes);
        writtenPages.emplace_back(begin, end);
    }), "invalid BDA written-page record");
    for (std::uint32_t claim = 0; claim < count; ++claim) {
        const auto slot = words[Abi::WrittenListWord + claim];
        words[Abi::WrittenSlotsWord + 2 * slot] = 0;
        words[Abi::WrittenSlotsWord + 2 * slot + 1] = 0;
    }
    words[Abi::WrittenCountWord] = 0;
}

}
