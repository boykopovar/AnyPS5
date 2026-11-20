#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/SrtWalker.hpp"
#include "ThreadOwned.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace AgcDriver {
namespace {
bool ShaderMemory::pagesCurrent() {
    bool current = true;
    for (auto& [pageBase, existing] : pages) {
        const auto backing = PageBacking(pageBase, PageBytes);
        if (backing == existing->backing) continue;
        existing->valid.Reset();
        existing->raw.Reset();
        existing->kept.Reset();
        existing->backing = backing;
        existing->wordwise = false;
        existing->lazy = false;
        preparePage(*existing, pageBase);
        current = false;
    }
    return current;
}

bool ShaderMemory::snapshotCurrent(void* context, std::span<const ShaderRecompiler::RegisteredImageView> views) {
    const bool pages = static_cast<ShaderMemory*>(context)->pagesCurrent();
    return pages && DriverDetail::CaptureRegisteredViewsCurrent(views);
}

bool ShaderMemory::KeepTable(std::uint64_t address) {
    constexpr std::size_t Words = KeepBytes / sizeof(std::uint32_t);
    if (address % sizeof(std::uint32_t) != 0 || address > std::numeric_limits<std::uint64_t>::max() - KeepBytes) return false;
    const auto end = address + KeepBytes;
    if (overlapsInitial(address, end)) return false;
    if (std::any_of(keptRanges.begin(), keptRanges.end(), [&](const auto& range) { return range.first < end && address < range.second; })) return false;
    if (std::any_of(supplied.begin(), supplied.end(), [&](const Supplied& entry) { return entry.begin < end && address < entry.end; })) return false;
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), KeepBytes)) return false;
    const auto quiet = [&] { return pendingSpan != nullptr && pendingSpan(address, KeepBytes, {}) == PendingWrite::None; };
    if (!quiet()) return false;
    std::array<std::uint32_t, Words> copy;
    GuestMemory::CopyGuestBytes(copy.data(), reinterpret_cast<const void*>(address), KeepBytes);
    if (!quiet()) return false;
    for (auto base = address & ~static_cast<std::uint64_t>(PageBytes - 1); base < end; base += PageBytes) static_cast<void>(page(base));
    if (!place(address, copy.data(), Words, true, true)) return false;
    for (auto cursor = address; cursor < end;) {
        const auto base = cursor & ~static_cast<std::uint64_t>(PageBytes - 1);
        const auto index = static_cast<std::size_t>((cursor % PageBytes) / sizeof(std::uint32_t));
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>((end - cursor) / sizeof(std::uint32_t), PageWords - index));
        auto& marked = page(base);
        marked.kept.SetRun(index, count);
        for (std::size_t word = 0; word < count; ++word) {
            marked.read.Clear(index + word);
            marked.recent.Clear(index + word);
        }
        cursor += static_cast<std::uint64_t>(count) * sizeof(std::uint32_t);
    }
    GuestMemory::GuardRead(address, KeepBytes);
    keptRanges.emplace_back(address, end);
    CaptureTotals().tablesKept.fetch_add(1, std::memory_order_relaxed);
    return true;
}

std::uint64_t ShaderMemory::TablesKept() {
    return CaptureTotals().tablesKept.load(std::memory_order_relaxed);
}

bool ShaderMemory::invalidateRawPages() {
    bool invalidated = false;
    for (auto& [pageBase, existing] : pages) {
        if (!existing->wordwise || existing->raw.None()) continue;
        existing->valid.ClearMask(existing->raw);
        existing->raw.Reset();
        invalidated = true;
    }
    return invalidated;
}

bool ShaderMemory::invalidateChangedRawPages() {
    bool invalidated = false;
    for (auto& [pageBase, existing] : pages) {
        if (!existing->wordwise || existing->raw.None()) continue;
        bool changed = false;
        for (std::size_t index = 0; index < PageWords && !changed; ++index) {
            if (!existing->raw.Test(index)) continue;
            const auto address = pageBase + index * sizeof(std::uint32_t);
            std::uint32_t word = 0;
            GuestMemory::GuardRead(address, sizeof(word));
            std::memcpy(&word, reinterpret_cast<const void*>(address), sizeof(word));
            changed = word != existing->words[index];
        }
        if (!changed) continue;
        existing->valid.ClearMask(existing->raw);
        existing->raw.Reset();
        invalidated = true;
    }
    return invalidated;
}

bool ShaderMemory::overlapsInitial(std::uint64_t begin, std::uint64_t end) const {
    if (initial.empty()) return false;
    const auto after = std::lower_bound(initial.begin(), initial.end(), begin, [](const auto& entry, std::uint64_t value) { return entry.first < value; });
    if (after != initial.end() && after->first < end) return true;
    return after != initial.begin() && std::prev(after)->first + std::prev(after)->second.size() > begin;
}

bool ShaderMemory::readWords(void* context, std::uint64_t address, std::uint32_t* values, std::uint64_t count) {
    auto& self = *static_cast<ShaderMemory*>(context);
    if (!self.supplied.empty() && count != 0) self.spillTouching(address, count > (std::numeric_limits<std::uint64_t>::max() - address) / sizeof(std::uint32_t) ? std::numeric_limits<std::uint64_t>::max() : address + count * sizeof(std::uint32_t));
    const auto startAddress = address;
    auto* const startValues = values;
    const auto startCount = count;
    const auto savedSpanBytes = self.descriptorSpanBytes;
    const auto savedForceSync = self.forceSync;
    struct ScopeReset {
        ShaderMemory& self;
        std::size_t spanBytes;
        std::uint32_t forceSync;
        ~ScopeReset() {
            self.descriptorSpanBytes = spanBytes;
            self.forceSync = forceSync;
        }
    } scopeReset{self, savedSpanBytes, savedForceSync};
    const auto strideWords = savedSpanBytes / sizeof(std::uint32_t);
    for (std::uint32_t attempt = 0; attempt < 3u; ++attempt) {
        if (attempt == 2u) ++self.forceSync;
        address = startAddress;
        values = startValues;
        count = startCount;
        const auto invalidationsBefore = self.rawInvalidations;
        while (count != 0) {
            const auto base = address & ~static_cast<std::uint64_t>(PageBytes - 1);
            const auto index = static_cast<std::size_t>((address % PageBytes) / sizeof(std::uint32_t));
            const auto words = static_cast<std::size_t>(std::min<std::uint64_t>(count, PageWords - index));
            const auto bytes = static_cast<std::uint64_t>(words) * sizeof(std::uint32_t);
            Page* page = nullptr;
            if (address % sizeof(std::uint32_t) == 0 && address <= std::numeric_limits<std::uint64_t>::max() - bytes && !self.overlapsInitial(address, address + bytes)) page = &self.page(base);
            if (page != nullptr && page->lazy) {
                const auto end = (index + words + BlockWords - 1u) / BlockWords * BlockWords;
                auto first = index / BlockWords * BlockWords;
                while (first < end) {
                    if (page->valid.TestBlock(first)) {
                        first += BlockWords;
                        continue;
                    }
                    auto last = first + BlockWords;
                    while (last < end && !page->valid.TestBlock(last)) last += BlockWords;
                    GuestMemory::CopyGuestBytes(&page->words[first], reinterpret_cast<const void*>(base + first * sizeof(std::uint32_t)), (last - first) * sizeof(std::uint32_t));
                    page->valid.SetRun(first, last - first);
                    first = last;
                }
            }
            if (page != nullptr && (page->lazy || (!page->wordwise && page->valid.All()))) {
                std::memcpy(values, page->words.data() + index, bytes);
                page->read.SetRun(index, words);
                page->recent.SetRun(index, words);
                CaptureTotals().reads += words;
            } else {
                const auto offsetWords = static_cast<std::uint64_t>(values - startValues);
                for (std::size_t word = 0; word < words; ++word) {
                    if (strideWords > 1u) {
                        const auto inRecord = (offsetWords + word) % strideWords;
                        const auto remainingWords = std::min<std::uint64_t>(strideWords - inRecord, startCount - (offsetWords + word));
                        self.descriptorSpanBytes = static_cast<std::size_t>(remainingWords * sizeof(std::uint32_t));
                    }
                    const bool ok = read(context, address + word * sizeof(std::uint32_t), values + word);
                    self.descriptorSpanBytes = savedSpanBytes;
                    if (!ok) {
                        return false;
                    }
                    if (self.rawInvalidations != invalidationsBefore) break;
                }
                if (self.rawInvalidations != invalidationsBefore) break;
            }
            address += bytes;
            values += words;
            count -= words;
        }
        if (self.rawInvalidations == invalidationsBefore) {
            return true;
        }
    }
    return false;
}

std::uint64_t ShaderMemory::descriptorScope(void* context, std::size_t bytes) {
    auto& self = *static_cast<ShaderMemory*>(context);
    if (bytes == ShaderRecompiler::SrtDescriptorForceSync) {
        if (self.forceSync++ == 0u && self.invalidateRawPages()) ++self.rawInvalidations;
        self.descriptorSpanBytes = 0;
        return self.rawInvalidations;
    }
    if (bytes == ShaderRecompiler::SrtDescriptorEndForceSync) {
        if (self.forceSync != 0u) --self.forceSync;
        self.descriptorSpanBytes = 0;
        return self.rawInvalidations;
    }
    self.descriptorSpanBytes = bytes;
    return self.rawInvalidations;
}

bool ShaderMemory::accessible(void* context, std::uint64_t address, std::uint64_t bytes) {
    auto& self = *static_cast<ShaderMemory*>(context);
    if (bytes == 0 || address > std::numeric_limits<std::uint64_t>::max() - bytes) return false;
    const auto end = address + bytes;
    while (address < end) {
        if (!self.initial.empty() && address < InitialEnd(self.initial)) {
            const auto next = std::upper_bound(self.initial.begin(), self.initial.end(), address, [](std::uint64_t value, const auto& entry) { return value < entry.first; });
            if (next != self.initial.begin()) {
                const auto previous = std::prev(next);
                if (address - previous->first < previous->second.size()) {
                    address = previous->first + previous->second.size();
                    continue;
                }
            }
        }
        const auto base = address & ~static_cast<std::uint64_t>(PageBytes - 1);
        const auto pageEnd = std::min<std::uint64_t>(base + PageBytes, end);
        const auto& page = self.page(base);
        if (!page.wordwise && !page.lazy && !page.valid.All() && !GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(pageEnd - address))) return false;
        address = pageEnd;
    }
    return true;
}

bool ShaderMemory::pending(void* context, std::uint64_t address, std::uint64_t bytes) {
    const auto& self = *static_cast<const ShaderMemory*>(context);
    if (self.pendingSpan == nullptr || bytes == 0) return false;
    const auto policy = self.pendingSpan(address, static_cast<std::size_t>(bytes), {});
    return policy == PendingWrite::Sync || policy == PendingWrite::VerifyRaw || policy == PendingWrite::VerifyKnownValue;
}

ShaderRecompiler::SrtWriteStamp ShaderMemory::stampWrites(void* context, std::uint64_t address, std::uint64_t bytes) {
    const auto& self = *static_cast<const ShaderMemory*>(context);
    if (bytes == 0 || address > std::numeric_limits<std::uint64_t>::max() - bytes || self.overlapsInitial(address, address + bytes)) return {};
    const auto fetched = self.pageAtOrAfter(address & ~static_cast<std::uint64_t>(PageBytes - 1));
    const bool anyFetched = fetched != self.pages.size() && self.pages[fetched].first < address + bytes;
    if (anyFetched && !self.spanQuery) return {};
    GuestMemory::FlushGpuWrites(address, static_cast<std::size_t>(bytes));
    const auto layout = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    const auto generation = GuestMemory::CollectWrites(address, static_cast<std::size_t>(bytes));
    if (generation == 0) return {};
    if (self.pendingSpan != nullptr && self.pendingSpan(address, static_cast<std::size_t>(bytes), {}) != PendingWrite::None) return {};
    if (anyFetched) {
        std::array<std::uint32_t, PageWords> current{};
        for (auto index = fetched; index < self.pages.size() && self.pages[index].first < address + bytes; ++index) {
            const auto& page = *self.pages[index].second;
            if (page.wordwise || GuestMemory::CopyMapped(self.pages[index].first, std::as_writable_bytes(std::span(current))) != GuestMemory::Compare::Equal) return {};
            for (std::size_t word = 0; word < PageWords; ++word) {
                if (page.valid.Test(word) && page.words[word] != current[word]) return {};
            }
        }
    }
    return {generation, layout};
}

bool ShaderMemory::Vouched(PendingWriteQuery pending, std::uint64_t address, std::uint64_t bytes, const ShaderRecompiler::SrtWriteStamp& stamp, bool uncached) {
    CaptureTotals().vouchCalls.fetch_add(1, std::memory_order_relaxed);
    if (stamp.generation == 0 || bytes == 0 || address > std::numeric_limits<std::uint64_t>::max() - bytes || stamp.layout != GuestAllocations::GuestAllocationsGeneration_nid_postfix()) return false;
    if (pending != nullptr && pending(address, static_cast<std::size_t>(bytes), {}) != PendingWrite::None) return false;
    GuestMemory::FlushGpuWrites(address, static_cast<std::size_t>(bytes));
    if ((uncached ? GuestMemory::CollectWritesUncached(address, static_cast<std::size_t>(bytes)) : GuestMemory::CollectWrites(address, static_cast<std::size_t>(bytes))) == 0) return false;
    return GuestMemory::UnchangedSince(address, static_cast<std::size_t>(bytes), stamp.generation);
}

bool ShaderMemory::unchangedSince(void* context, std::uint64_t address, std::uint64_t bytes, const ShaderRecompiler::SrtWriteStamp& stamp) {
    return Vouched(static_cast<const ShaderMemory*>(context)->pendingSpan, address, bytes, stamp);
}

bool ShaderMemory::place(std::uint64_t address, const std::uint32_t* values, std::uint64_t count, bool overwrite, bool recent) {
    while (count != 0) {
        const auto base = address & ~static_cast<std::uint64_t>(PageBytes - 1);
        const auto index = static_cast<std::size_t>((address % PageBytes) / sizeof(std::uint32_t));
        const auto words = static_cast<std::size_t>(std::min<std::uint64_t>(count, PageWords - index));
        const auto position = pageAtOrAfter(base);
        Page* found = position != pages.size() && pages[position].first == base ? pages[position].second : nullptr;
        if (found == nullptr) {
            if (!overwrite && !GuestMemory::Accessible(reinterpret_cast<const void*>(base), PageBytes)) return false;
            found = &addPage(position, base);
        }
        auto& page = *found;
        if (page.wordwise && !overwrite) return false;
        const auto head = index / BlockWords * BlockWords;
        const auto tail = (index + words + BlockWords - 1) / BlockWords * BlockWords;
        const bool headOpen = page.lazy && !page.valid.Test(head);
        const bool tailOpen = page.lazy && !page.valid.Test(tail - 1);
        if (overwrite || page.valid.None()) {
            std::memcpy(page.words.data() + index, values, words * sizeof(std::uint32_t));
        } else {
            for (std::size_t word = 0; word < words; ++word) {
                if (!page.valid.Test(index + word)) page.words[index + word] = values[word];
            }
        }
        if (headOpen && head < index) GuestMemory::CopyGuestBytes(&page.words[head], reinterpret_cast<const void*>(base + head * sizeof(std::uint32_t)), (index - head) * sizeof(std::uint32_t));
        if (tailOpen && index + words < tail) GuestMemory::CopyGuestBytes(&page.words[index + words], reinterpret_cast<const void*>(base + (index + words) * sizeof(std::uint32_t)), (tail - index - words) * sizeof(std::uint32_t));
        page.valid.SetRun(page.lazy ? head : index, page.lazy ? tail - head : words);
        page.read.SetRun(index, words);
        if (recent) page.recent.SetRun(index, words);
        address += static_cast<std::uint64_t>(words) * sizeof(std::uint32_t);
        values += words;
        count -= words;
    }
    return true;
}

void ShaderMemory::spill(std::size_t index) {
    auto entry = std::move(supplied[index]);
    supplied.erase(supplied.begin() + static_cast<std::ptrdiff_t>(index));
    std::size_t at = 0;
    for (const auto& [address, count] : entry.words->runs) {
        static_cast<void>(place(address, entry.words->words.data() + at, count, true, entry.recent));
        at += count;
    }
    CaptureTotals().suppliedSpilled += entry.words->words.size();
    spilled.push_back(std::move(entry.words));
}

void ShaderMemory::spillTouching(std::uint64_t begin, std::uint64_t end) {
    for (std::size_t index = supplied.size(); index-- > 0;) {
        const auto& entry = supplied[index];
        if (entry.begin < end && begin < entry.end && RunsTouch(entry.words->runs, begin, end)) spill(index);
    }
}

bool ShaderMemory::supplyWords(void* context, const std::shared_ptr<const ShaderRecompiler::SrtGuestWords>& words) {
    auto& self = *static_cast<ShaderMemory*>(context);
    if (words == nullptr || words->runs.empty()) return false;
    std::uint64_t total = 0;
    std::uint64_t end = 0;
    if (words->ordered && words->low == words->runs.front().first && words->low < words->high) {
        total = words->words.size();
        end = words->high;
    } else {
        for (const auto& [address, count] : words->runs) {
            const auto bytes = static_cast<std::uint64_t>(count) * sizeof(std::uint32_t);
            if (count == 0 || address % sizeof(std::uint32_t) != 0 || address < end || address > std::numeric_limits<std::uint64_t>::max() - bytes) return false;
            end = address + bytes;
            total += count;
        }
    }
    const auto begin = words->runs.front().first;
    if (total != words->words.size() || self.overlapsInitial(begin, end)) return false;
    auto& totals = CaptureTotals();
    for (auto& entry : self.supplied) {
        if (entry.words != words) continue;
        entry.recent = true;
        totals.suppliedShared += total;
        return true;
    }
    const auto fetched = self.pageAtOrAfter(begin & ~static_cast<std::uint64_t>(PageBytes - 1));
    const bool paged = fetched != self.pages.size() && self.pages[fetched].first < end;
    const bool overlapping = std::any_of(self.supplied.begin(), self.supplied.end(), [&](const Supplied& entry) { return entry.begin < end && begin < entry.end && RunsOverlap(entry.words->runs, words->runs); });
    if (paged || overlapping) {
        for (std::size_t index = self.supplied.size(); index-- > 0;) {
            const auto& entry = self.supplied[index];
            if (entry.begin < end && begin < entry.end && RunsOverlap(entry.words->runs, words->runs)) self.spill(index);
        }
        std::size_t at = 0;
        for (const auto& [address, count] : words->runs) {
            if (!self.place(address, words->words.data() + at, count, false, true)) return false;
            at += count;
        }
        totals.suppliedCopied += total;
        return true;
    }
    const auto spanBegin = begin & ~static_cast<std::uint64_t>(PageBytes - 1);
    const auto spanEnd = (end + PageBytes - 1) & ~static_cast<std::uint64_t>(PageBytes - 1);
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(spanBegin), static_cast<std::size_t>(spanEnd - spanBegin))) {
        std::uint64_t first = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t past = 0;
        for (const auto& [address, count] : words->runs) {
            const auto from = address & ~static_cast<std::uint64_t>(PageBytes - 1);
            const auto to = (address + static_cast<std::uint64_t>(count) * sizeof(std::uint32_t) + PageBytes - 1) & ~static_cast<std::uint64_t>(PageBytes - 1);
            if (from > past || first == std::numeric_limits<std::uint64_t>::max()) {
                if (first != std::numeric_limits<std::uint64_t>::max() && !GuestMemory::Accessible(reinterpret_cast<const void*>(first), static_cast<std::size_t>(past - first))) return false;
                first = from;
            }
            past = std::max(past, to);
        }
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(first), static_cast<std::size_t>(past - first))) return false;
    }
    self.supplied.push_back({words, begin, end, true});
    totals.suppliedShared += total;
    return true;
}

}
}
