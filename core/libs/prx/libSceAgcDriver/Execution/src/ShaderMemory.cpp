#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
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

struct CaptureProfile {
    std::atomic<std::uint64_t> captures{0};
    std::atomic<std::uint64_t> reads{0};
    std::atomic<std::uint64_t> pages{0};
    // The whole CaptureResources call: the plan lookup and the materialization are one call.
    std::atomic<std::uint64_t> captureNanoseconds{0};
    // Pages read word by word because recorded GPU work writes them, and the word reads of those
    // pages that waited (a read taking longer than WordWaitNanoseconds went through a hook sync),
    // were read raw on the driver's evidence, or were read both ways (verify) and differed.
    std::atomic<std::uint64_t> pagesWordwise{0};
    std::atomic<std::uint64_t> wordWaits{0};
    std::atomic<std::uint64_t> wordWaitNanoseconds{0};
    std::atomic<std::uint64_t> wordsRaw{0};
    std::atomic<std::uint64_t> wordsVerified{0};
    std::atomic<std::uint64_t> wordMismatches{0};
    // Words served from the driver's known values (never profile-gated: the [copy] line reads them).
    std::atomic<std::uint64_t> wordsKnown{0};
    std::atomic<std::uint64_t> wordsKnownVerified{0};
    std::atomic<std::uint64_t> wordsKnownMismatches{0};
    // Hook reads not reported to the observer because no GPU wait happened across them.
    std::atomic<std::uint64_t> observationsSkipped{0};
    // Sub-phases of a capture: the source lookup (the stage inputs, the recompiler's key over the
    // code and the plan, timed by CaptureResources itself: ResourceCapture::sourceNanoseconds),
    // whole-page fetches, per-word fetches (their hook waits included) and the GPU waits the flush
    // hook made anywhere inside the capture (Recorder::ThreadWaitedMs). The walk itself is what
    // remains of the capture time after the lookup, the fetches and the specialization
    // (ResourceMaterializer).
    std::atomic<std::uint64_t> resolveNanoseconds{0};
    std::atomic<std::uint64_t> pageReadNanoseconds{0};
    std::atomic<std::uint64_t> wordReadNanoseconds{0};
    std::atomic<std::uint64_t> hookWaitNanoseconds{0};
    // The driver's per-shader source handle memo: captures served from it, and resolves it made.
    std::atomic<std::uint64_t> handleHits{0};
    std::atomic<std::uint64_t> handleMisses{0};
    std::atomic<std::uint64_t> suppliedShared{0};
    std::atomic<std::uint64_t> suppliedCopied{0};
    std::atomic<std::uint64_t> suppliedSpilled{0};
    std::atomic<std::uint64_t> vouchCalls{0};
    std::atomic<std::uint64_t> vouchedBlocks{0};
    std::atomic<std::uint64_t> refusedBlocks{0};
    std::atomic<std::uint64_t> vouchedWords{0};
};

constexpr std::uint64_t WordWaitNanoseconds = 50000;

bool WordwisePages() {
    static const bool wordwise = std::getenv("APS5_NO_WORDWISE_CAPTURE") == nullptr;
    return wordwise;
}

bool CaptureProfiled() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

template<typename TSnapshots>
std::uint64_t InitialEnd(const TSnapshots& initial) {
    const auto& last = *std::prev(initial.end());
    return last.first + last.second.size();
}

std::uint64_t NanosecondsSince(std::chrono::steady_clock::time_point started) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());
}

CaptureProfile& CaptureTotals() {
    static CaptureProfile profile;
    return profile;
}

std::atomic<ShaderMemory::WaitedMsProvider> waitedMsProvider{nullptr};

double WaitedMs() {
    const auto provider = waitedMsProvider.load(std::memory_order_acquire);
    return provider != nullptr ? provider() : 0.0;
}

using GuestRuns = std::vector<std::pair<std::uint64_t, std::uint32_t>>;

std::uint64_t RunEnd(const std::pair<std::uint64_t, std::uint32_t>& run) {
    return run.first + static_cast<std::uint64_t>(run.second) * sizeof(std::uint32_t);
}

bool RunsTouch(const GuestRuns& runs, std::uint64_t begin, std::uint64_t end) {
    const auto after = std::upper_bound(runs.begin(), runs.end(), begin, [](std::uint64_t address, const std::pair<std::uint64_t, std::uint32_t>& run) { return address < run.first; });
    if (after != runs.begin() && RunEnd(*std::prev(after)) > begin) return true;
    return after != runs.end() && after->first < end;
}

bool RunsOverlap(const GuestRuns& left, const GuestRuns& right) {
    std::size_t a = 0;
    std::size_t b = 0;
    while (a < left.size() && b < right.size()) {
        if (left[a].first < RunEnd(right[b]) && right[b].first < RunEnd(left[a])) return true;
        if (RunEnd(left[a]) <= RunEnd(right[b])) ++a;
        else ++b;
    }
    return false;
}

template<typename TPage>
std::vector<std::unique_ptr<TPage>>& pagePool() {
    struct PagePoolStorage {};
    return HostThreadLocal<std::vector<std::unique_ptr<TPage>>, PagePoolStorage>();
}

constexpr std::size_t PagePoolLimit = 64;

bool sharedPagePools() {
    static const bool enabled = std::getenv("APS5_DRAW_PIPELINE") != nullptr;
    return enabled;
}

template<typename TPage>
struct SharedPagePool {
    std::mutex mutex;
    std::vector<std::unique_ptr<TPage>> pages;
};

template<typename TPage>
SharedPagePool<TPage>& sharedPagePool() {
    static auto* pool = new SharedPagePool<TPage>();
    return *pool;
}

}

void ShaderMemory::SetWaitedMsProvider(WaitedMsProvider provider) {
    waitedMsProvider.store(provider, std::memory_order_release);
}

ShaderMemory::ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> regions, PendingWriteQuery pendingWrite, PendingWriteObserver observe, HookWaitCounter hookWaits, PendingWriteQuery pendingSpan) : pendingWrite(pendingWrite), observe(observe), hookWaits(hookWaits), pendingSpan(pendingSpan != nullptr ? pendingSpan : pendingWrite) {
    const ShaderRecompiler::RequestMemoryView validated(regions);
    initial.reserve(regions.size());
    for (const auto& region : regions) initial.emplace_back(region.guestAddress, region.bytes);
    std::sort(initial.begin(), initial.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
}

ShaderMemory::~ShaderMemory() {
    if (sharedPagePools() && !pages.empty()) {
        auto& shared = sharedPagePool<Page>();
        std::lock_guard lock(shared.mutex);
        for (const auto& [base, page] : pages) {
            if (shared.pages.size() < 1024) shared.pages.emplace_back(page);
            else delete page;
        }
        return;
    }
    auto& pool = pagePool<Page>();
    for (const auto& [base, page] : pages) {
        if (pool.size() < PagePoolLimit) pool.emplace_back(page);
        else delete page;
    }
}

std::size_t ShaderMemory::Bytes() const {
    return sizeof(ShaderMemory) + initial.capacity() * sizeof(initial[0]) + pages.capacity() * sizeof(pages[0]) + pages.size() * sizeof(Page) + supplied.capacity() * sizeof(Supplied) + spilled.capacity() * sizeof(spilled[0]);
}

void ShaderMemory::WordMask::SetRun(std::size_t first, std::size_t count) {
    while (count != 0) {
        const auto shift = first % 64u;
        const auto take = std::min<std::size_t>(count, 64u - shift);
        bits[first / 64u] |= (take == 64u ? ~std::uint64_t{0} : (std::uint64_t{1} << take) - 1u) << shift;
        first += take;
        count -= take;
    }
}

bool ShaderMemory::WordMask::None() const {
    return std::all_of(bits.begin(), bits.end(), [](std::uint64_t word) { return word == 0u; });
}

template<typename TRun>
void ShaderMemory::WordMask::ForEachRun(TRun&& run) const {
    constexpr std::size_t words = PageWords / 64u;
    std::size_t index = 0;
    while (index < PageWords) {
        auto word = index / 64u;
        auto set = bits[word] & (~std::uint64_t{0} << (index % 64u));
        while (set == 0u) {
            if (++word == words) return;
            set = bits[word];
        }
        const auto first = word * 64u + static_cast<std::size_t>(std::countr_zero(set));
        auto clear = ~bits[word] & (~std::uint64_t{0} << (first % 64u));
        while (clear == 0u && ++word < words) clear = ~bits[word];
        const auto end = word == words ? PageWords : word * 64u + static_cast<std::size_t>(std::countr_zero(clear));
        run(first, end);
        index = end;
    }
}

std::size_t ShaderMemory::pageAtOrAfter(std::uint64_t base) const {
    return static_cast<std::size_t>(std::partition_point(pages.begin(), pages.end(), [base](const std::pair<std::uint64_t, Page*>& entry) { return entry.first < base; }) - pages.begin());
}

ShaderMemory::Page& ShaderMemory::addPage(std::size_t position, std::uint64_t base) {
    auto& pool = pagePool<Page>();
    if (pool.empty() && sharedPagePools()) {
        auto& shared = sharedPagePool<Page>();
        std::lock_guard lock(shared.mutex);
        for (std::size_t i = 0; i < 32 && !shared.pages.empty(); ++i) {
            pool.push_back(std::move(shared.pages.back()));
            shared.pages.pop_back();
        }
    }
    std::unique_ptr<Page> owned;
    if (pool.empty()) {
        owned = std::make_unique<Page>();
    } else {
        owned = std::move(pool.back());
        pool.pop_back();
    }
    owned->valid.Reset();
    owned->raw.Reset();
    owned->read.Reset();
    owned->recent.Reset();
    owned->wordwise = false;
    owned->lazy = false;
    pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(position), {base, owned.get()});
    lastBase = base;
    lastPage = owned.release();
    return *lastPage;
}

ShaderMemory::Page& ShaderMemory::page(std::uint64_t base) {
    if (base == lastBase) return *lastPage;
    const auto position = pageAtOrAfter(base);
    if (position != pages.size() && pages[position].first == base) {
        lastBase = base;
        lastPage = pages[position].second;
        return *lastPage;
    }
    auto& page = addPage(position, base);
    if (CaptureProfiled()) ++CaptureTotals().pages;
    // A page is either mapped whole or read word by word where the guest mapped less than a page,
    // or where recorded GPU work writes into it (a whole-page read would wait for that work).
    if (GuestMemory::Accessible(reinterpret_cast<const void*>(base), PageBytes)) {
        if (WordwisePages() && pendingWrite != nullptr && pendingWrite(base, PageBytes, {}) != PendingWrite::None) {
            page.wordwise = true;
            ++CaptureTotals().pagesWordwise;
            return page;
        }
        const auto started = CaptureProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        GuestMemory::GuardRead(base, PageBytes);
        GuestMemory::FlushGpuWrites(base, PageBytes);
        if (CaptureProfiled()) CaptureTotals().pageReadNanoseconds += NanosecondsSince(started);
        page.lazy = true;
    }
    return page;
}

bool ShaderMemory::read(void* context, std::uint64_t address, std::uint32_t* value) {
    auto& self = *static_cast<ShaderMemory*>(context);
    if (address % sizeof(*value) != 0 || address > std::numeric_limits<std::uint64_t>::max() - sizeof(*value)) {
        throw std::runtime_error("AGC driver: invalid shader memory read address");
    }
    if (CaptureProfiled()) ++CaptureTotals().reads;
    if (!self.initial.empty() && address + sizeof(*value) > self.initial.begin()->first && address < InitialEnd(self.initial)) {
        const auto next = std::upper_bound(self.initial.begin(), self.initial.end(), address, [](std::uint64_t value, const auto& entry) { return value < entry.first; });
        if (next != self.initial.begin()) {
            const auto previous = std::prev(next);
            const auto offset = address - previous->first;
            if (offset < previous->second.size()) {
                if (previous->second.size() - offset < sizeof(*value)) throw std::runtime_error("AGC driver: shader memory read crosses a snapshot boundary");
                std::memcpy(value, previous->second.data() + offset, sizeof(*value));
                return true;
            }
        }
        if (next != self.initial.end() && next->first - address < sizeof(*value)) throw std::runtime_error("AGC driver: shader memory read overlaps a snapshot boundary");
    }
    if (!self.supplied.empty()) self.spillTouching(address, address + sizeof(*value));
    auto& page = self.page(address & ~static_cast<std::uint64_t>(PageBytes - 1));
    const auto index = static_cast<std::size_t>((address % PageBytes) / sizeof(*value));
    if (!page.valid.Test(index) && page.lazy) {
        const auto first = index / BlockWords * BlockWords;
        GuestMemory::CopyGuestBytes(&page.words[first], reinterpret_cast<const void*>((address & ~static_cast<std::uint64_t>(PageBytes - 1)) + first * sizeof(std::uint32_t)), BlockWords * sizeof(std::uint32_t));
        page.valid.SetBlock(index);
    }
    const auto spanNeedsSync = [](PendingWrite spanPolicy) {
        return spanPolicy == PendingWrite::Sync || spanPolicy == PendingWrite::VerifyRaw || spanPolicy == PendingWrite::VerifyKnownValue;
    };
    const auto spanInvalidatesCache = [&](PendingWrite spanPolicy) {
        return spanNeedsSync(spanPolicy) || spanPolicy == PendingWrite::KnownValue;
    };
    const auto checkSpanBytes = std::max<std::size_t>(self.descriptorSpanBytes, sizeof(*value));
    if (page.wordwise && page.valid.Test(index) &&
        (self.forceSync != 0u ? self.pendingWrite(address, checkSpanBytes, {}) != PendingWrite::None
                              : (spanInvalidatesCache(self.pendingWrite(address, checkSpanBytes, {})) ||
                                 (checkSpanBytes > sizeof(*value) && spanInvalidatesCache(self.pendingWrite(address, sizeof(*value), {})))))) {
        page.valid.Clear(index);
        page.raw.Clear(index);
    }
    if (!page.valid.Test(index)) {
        std::uint32_t word = 0;
        auto policy = PendingWrite::Sync;
        bool forcedRaw = false;
        if (page.wordwise) {
            if (self.descriptorSpanBytes > sizeof(word) &&
                (self.forceSync != 0u ? self.pendingWrite(address, self.descriptorSpanBytes, {}) != PendingWrite::None
                                      : spanNeedsSync(self.pendingWrite(address, self.descriptorSpanBytes, {})))) {
                policy = PendingWrite::Sync;
                forcedRaw = self.forceSync != 0u && !spanNeedsSync(self.pendingWrite(address, self.descriptorSpanBytes, {}));
            } else {
                policy = self.pendingWrite(address, sizeof(word), std::as_writable_bytes(std::span(&word, 1)));
                if (self.forceSync != 0u && policy != PendingWrite::None) {
                    forcedRaw = !spanNeedsSync(policy);
                    policy = PendingWrite::Sync;
                }
                if (policy != PendingWrite::None && policy != PendingWrite::Sync && !GuestMemory::Accessible(reinterpret_cast<const void*>(address), sizeof(word))) policy = PendingWrite::Sync;
            }
        }
        // A read is evidence only when the hook waited for unfinished GPU work across it.
        const auto hookWaits = [&] { return self.hookWaits != nullptr ? self.hookWaits() : 0; };
        const auto report = [&](std::uint64_t waitsBefore, bool unchanged) {
            if (self.hookWaits == nullptr || self.hookWaits() != waitsBefore) self.observe(address, unchanged);
            else ++CaptureTotals().observationsSkipped;
        };
        const auto fetchStarted = CaptureProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        bool waited = false;
        if (policy == PendingWrite::KnownValue || policy == PendingWrite::VerifyKnownValue) {
            // The query stored the known dword in `word`.
            ++CaptureTotals().wordsKnown;
            if (policy == PendingWrite::VerifyKnownValue) {
                std::uint32_t verifiedWord = 0;
                const auto waitsBefore = hookWaits();
                GuestMemory::Read(address, std::as_writable_bytes(std::span(&verifiedWord, 1)), alignof(std::uint32_t));
                ++CaptureTotals().wordsKnownVerified;
                if (verifiedWord != word) ++CaptureTotals().wordsKnownMismatches;
                if (self.observe != nullptr) report(waitsBefore, verifiedWord == word);
                waited = page.wordwise && (self.hookWaits != nullptr ? self.hookWaits() != waitsBefore : true);
                word = verifiedWord;
            }
        } else if (policy == PendingWrite::Raw || policy == PendingWrite::VerifyRaw) {
            GuestMemory::GuardRead(address, sizeof(word));
            std::memcpy(&word, reinterpret_cast<const void*>(address), sizeof(word));
            ++CaptureTotals().wordsRaw;
            if (policy == PendingWrite::VerifyRaw) {
                std::uint32_t verifiedWord = 0;
                const auto waitsBefore = hookWaits();
                GuestMemory::Read(address, std::as_writable_bytes(std::span(&verifiedWord, 1)), alignof(std::uint32_t));
                ++CaptureTotals().wordsVerified;
                if (verifiedWord != word) ++CaptureTotals().wordMismatches;
                if (self.observe != nullptr) report(waitsBefore, verifiedWord == word);
                waited = page.wordwise && (self.hookWaits != nullptr ? self.hookWaits() != waitsBefore : true);
                word = verifiedWord;
            }
        } else {
            // On a word-wise page the bytes before the hook's wait are kept for the observer.
            const bool observed = page.wordwise && policy == PendingWrite::Sync && self.observe != nullptr && GuestMemory::Accessible(reinterpret_cast<const void*>(address), sizeof(word));
            std::uint32_t before = 0;
            if (observed) std::memcpy(&before, reinterpret_cast<const void*>(address), sizeof(before));
            const auto waitsBefore = page.wordwise ? hookWaits() : 0;
            const auto started = page.wordwise ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            if (page.wordwise && policy == PendingWrite::Sync && self.descriptorSpanBytes > sizeof(word)) {
                GuestMemory::FlushGpuWrites(address, self.descriptorSpanBytes);
            }
            GuestMemory::Read(address, std::as_writable_bytes(std::span(&word, 1)), alignof(std::uint32_t));
            if (page.wordwise) {
                const auto nanoseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());
                if (nanoseconds >= WordWaitNanoseconds) {
                    ++CaptureTotals().wordWaits;
                    CaptureTotals().wordWaitNanoseconds += nanoseconds;
                }
                waited = policy != PendingWrite::None && (self.hookWaits != nullptr ? self.hookWaits() != waitsBefore : true);
            }
            if (observed) report(waitsBefore, before == word);
        }
        if ((waited || (page.wordwise && policy == PendingWrite::Sync && !forcedRaw)) && self.invalidateRawPages()) ++self.rawInvalidations;
        if (CaptureProfiled()) CaptureTotals().wordReadNanoseconds += NanosecondsSince(fetchStarted);
        page.words[index] = word;
        page.valid.Set(index);
        if (page.wordwise && (policy == PendingWrite::Raw || policy == PendingWrite::KnownValue || (forcedRaw && !waited))) page.raw.Set(index);
        else page.raw.Clear(index);
    }
    page.read.Set(index);
    page.recent.Set(index);
    *value = page.words[index];
    return true;
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
    if (self.pendingWrite == nullptr || bytes == 0) return false;
    const auto policy = self.pendingWrite(address, static_cast<std::size_t>(bytes), {});
    return policy == PendingWrite::Sync || policy == PendingWrite::VerifyRaw || policy == PendingWrite::VerifyKnownValue;
}

ShaderRecompiler::SrtWriteStamp ShaderMemory::stampWrites(void* context, std::uint64_t address, std::uint64_t bytes) {
    const auto& self = *static_cast<const ShaderMemory*>(context);
    if (bytes == 0 || address > std::numeric_limits<std::uint64_t>::max() - bytes || self.overlapsInitial(address, address + bytes)) return {};
    const auto fetched = self.pageAtOrAfter(address & ~static_cast<std::uint64_t>(PageBytes - 1));
    if (fetched != self.pages.size() && self.pages[fetched].first < address + bytes) return {};
    GuestMemory::FlushGpuWrites(address, static_cast<std::size_t>(bytes));
    const auto layout = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    const auto generation = GuestMemory::CollectWrites(address, static_cast<std::size_t>(bytes));
    if (generation == 0) return {};
    if (self.pendingSpan != nullptr && self.pendingSpan(address, static_cast<std::size_t>(bytes), {}) != PendingWrite::None) return {};
    return {generation, layout};
}

bool ShaderMemory::Vouched(PendingWriteQuery pending, std::uint64_t address, std::uint64_t bytes, const ShaderRecompiler::SrtWriteStamp& stamp) {
    CaptureTotals().vouchCalls.fetch_add(1, std::memory_order_relaxed);
    if (stamp.generation == 0 || bytes == 0 || address > std::numeric_limits<std::uint64_t>::max() - bytes || stamp.layout != GuestAllocations::GuestAllocationsGeneration_nid_postfix()) return false;
    if (pending != nullptr && pending(address, static_cast<std::size_t>(bytes), {}) != PendingWrite::None) return false;
    GuestMemory::FlushGpuWrites(address, static_cast<std::size_t>(bytes));
    if (GuestMemory::CollectWrites(address, static_cast<std::size_t>(bytes)) == 0) return false;
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

ShaderMemory::KnownValueCounts ShaderMemory::KnownValues() {
    auto& totals = CaptureTotals();
    return {totals.wordsKnown.load(std::memory_order_relaxed), totals.wordsKnownVerified.load(std::memory_order_relaxed), totals.wordsKnownMismatches.load(std::memory_order_relaxed)};
}

void ShaderMemory::CountHandleMemo(bool hit) {
    if (!CaptureProfiled()) return;
    (hit ? CaptureTotals().handleHits : CaptureTotals().handleMisses).fetch_add(1, std::memory_order_relaxed);
}

ShaderMemory::VouchCounts ShaderMemory::Vouches() {
    auto& totals = CaptureTotals();
    return {totals.vouchCalls.load(std::memory_order_relaxed), totals.vouchedBlocks.load(std::memory_order_relaxed), totals.refusedBlocks.load(std::memory_order_relaxed), totals.vouchedWords.load(std::memory_order_relaxed)};
}

ShaderMemory::SupplyCounts ShaderMemory::Supplies() {
    auto& totals = CaptureTotals();
    return {totals.suppliedShared.load(std::memory_order_relaxed), totals.suppliedCopied.load(std::memory_order_relaxed), totals.suppliedSpilled.load(std::memory_order_relaxed)};
}

std::vector<std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> ShaderMemory::SuppliedBlocks() const {
    std::vector<std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> blocks;
    blocks.reserve(supplied.size() + spilled.size());
    for (const auto& entry : supplied) blocks.push_back(entry.words);
    blocks.insert(blocks.end(), spilled.begin(), spilled.end());
    return blocks;
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> ShaderMemory::Capture(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::SourceHandle* handle) {
    return capture(request, handle, nullptr);
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> ShaderMemory::Capture(const ShaderRecompiler::PreparedShaderInvocation& invocation) {
    return capture(invocation.Request(), nullptr, &invocation);
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> ShaderMemory::capture(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::SourceHandle* handle, const ShaderRecompiler::PreparedShaderInvocation* invocation) {
    // The capture's word and page reads (through `read`) are attributed to it ([hooksync], [guestmem]).
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Capture);
    const bool profile = CaptureProfiled();
    auto& totals = CaptureTotals();
    // The hook's GPU waits made anywhere inside the capture.
    const auto waitedBefore = profile ? WaitedMs() : 0.0;
    const auto started = std::chrono::steady_clock::now();
    ShaderRecompiler::SrtRuntime runtime;
    runtime.userData = request.context.userData;
    runtime.shaderBase = request.shader.codeAddress;
    runtime.userContext = this;
    runtime.readMemory = &read;
    runtime.readSpecializationMemory = &read;
    runtime.accessible = &accessible;
    runtime.pendingWrite = &pending;
    runtime.readWords = &readWords;
    runtime.stampWrites = &stampWrites;
    runtime.unchangedSince = &unchangedSince;
    runtime.supplyWords = &supplyWords;
    runtime.descriptorScope = &descriptorScope;
    auto capture = invocation != nullptr ? invocation->Capture(runtime) : handle != nullptr ? ShaderRecompiler::CaptureResources(request, runtime, *handle) : ShaderRecompiler::CaptureResources(request, runtime);
    if (profile) {
        totals.captureNanoseconds += NanosecondsSince(started);
        totals.resolveNanoseconds += capture->sourceNanoseconds;
        totals.hookWaitNanoseconds += static_cast<std::uint64_t>((WaitedMs() - waitedBefore) * 1e6);
        std::uint64_t initialBytes = 0;
        for (const auto& [address, bytes] : initial) initialBytes += bytes.size();
        ++totals.captures;
        static std::atomic<std::chrono::steady_clock::rep> nextReport{0};
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto due = nextReport.load(std::memory_order_relaxed);
        if (now >= due && nextReport.compare_exchange_strong(due, now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::seconds(10)).count(), std::memory_order_relaxed)) {
            const auto captures = static_cast<double>(totals.captures.load());
            const auto captureNs = totals.captureNanoseconds.load();
            const auto resolveNs = totals.resolveNanoseconds.load();
            const auto pageNs = totals.pageReadNanoseconds.load();
            const auto wordNs = totals.wordReadNanoseconds.load();
            const auto specializationNs = ShaderRecompiler::ResourceMaterializer::SpecializationNanoseconds();
            const auto hookNs = totals.hookWaitNanoseconds.load();
            const auto materializeNs = ShaderRecompiler::ResourceMaterializer::MaterializeNanoseconds();
            const auto evaluateNs = ShaderRecompiler::ResourceMaterializer::EvaluateNanoseconds();
            const auto walkNs = captureNs > resolveNs + pageNs + wordNs + specializationNs ? captureNs - resolveNs - pageNs - wordNs - specializationNs : 0;
            AgcDriver::ProfilePrint_nid_no_patch( "[capture] %llu captures: %llu word reads, %llu pages fetched (%llu read word by word over pending GPU writes: %llu words waited %.2f s, %llu read raw on evidence, %llu verified with %llu mismatches, %llu served from known values, %llu hook reads not observed: no GPU wait), capture (plan lookup + materialize) %.1f s, %llu KiB registered code this capture; sub-phases (s, us per capture): source resolve %.2f/%.1f, walk %.2f/%.1f, specialization (every materialize) %.2f/%.1f, page reads %.2f/%.1f, word reads %.2f/%.1f, hook GPU waits inside %.2f/%.1f, materialize %.2f/%.1f (SRT evaluate %.2f/%.1f); source handle memo %llu hits, %llu resolves\n", static_cast<unsigned long long>(totals.captures.load()), static_cast<unsigned long long>(totals.reads.load()), static_cast<unsigned long long>(totals.pages.load()), static_cast<unsigned long long>(totals.pagesWordwise.load()), static_cast<unsigned long long>(totals.wordWaits.load()), totals.wordWaitNanoseconds.load() / 1e9, static_cast<unsigned long long>(totals.wordsRaw.load()), static_cast<unsigned long long>(totals.wordsVerified.load()), static_cast<unsigned long long>(totals.wordMismatches.load()), static_cast<unsigned long long>(totals.wordsKnown.load()), static_cast<unsigned long long>(totals.observationsSkipped.load()), captureNs / 1e9, static_cast<unsigned long long>(initialBytes / 1024), resolveNs / 1e9, resolveNs / captures / 1e3, walkNs / 1e9, walkNs / captures / 1e3, specializationNs / 1e9, specializationNs / captures / 1e3, pageNs / 1e9, pageNs / captures / 1e3, wordNs / 1e9, wordNs / captures / 1e3, hookNs / 1e9, hookNs / captures / 1e3, materializeNs / 1e9, materializeNs / captures / 1e3, evaluateNs / 1e9, evaluateNs / captures / 1e3, static_cast<unsigned long long>(totals.handleHits.load()), static_cast<unsigned long long>(totals.handleMisses.load()));
        }
    }
    return capture;
}

std::vector<ShaderRecompiler::MemoryRegion> ShaderMemory::collect(bool recentOnly) const {
    struct Cursor {
        const ShaderRecompiler::SrtGuestWords* words;
        std::size_t run;
        std::size_t at;
    };
    std::vector<Cursor> cursors;
    std::size_t runs = 0;
    for (const auto& entry : supplied) {
        if ((recentOnly && !entry.recent) || entry.words->runs.empty()) continue;
        cursors.push_back({entry.words.get(), 0u, 0u});
        runs += entry.words->runs.size() + entry.words->words.size() / PageWords;
    }
    std::vector<ShaderRecompiler::MemoryRegion> result;
    result.reserve((recentOnly ? 0u : initial.size()) + pages.size() + runs);
    auto next = recentOnly ? initial.end() : initial.begin();
    const bool heap = cursors.size() > 1u;
    const auto later = [](const Cursor& left, const Cursor& right) {
        return left.words->runs[left.run].first > right.words->runs[right.run].first;
    };
    if (heap) std::make_heap(cursors.begin(), cursors.end(), later);
    const auto emitBefore = [&](std::uint64_t address) {
        while (true) {
            Cursor* lowest = heap && !cursors.empty() ? &cursors.front() : nullptr;
            if (!heap) {
                for (auto& cursor : cursors) {
                    if (cursor.run < cursor.words->runs.size() && (lowest == nullptr || cursor.words->runs[cursor.run].first < lowest->words->runs[lowest->run].first)) lowest = &cursor;
                }
            }
            const auto run = lowest != nullptr ? lowest->words->runs[lowest->run].first : std::numeric_limits<std::uint64_t>::max();
            if (next != initial.end() && next->first < address && next->first < run) {
                result.push_back({next->first, next->second});
                ++next;
            } else if (lowest != nullptr && run < address) {
                if (heap) {
                    std::pop_heap(cursors.begin(), cursors.end(), later);
                    lowest = &cursors.back();
                }
                const auto count = lowest->words->runs[lowest->run].second;
                const auto words = std::span(lowest->words->words).subspan(lowest->at, count);
                for (std::size_t done = 0; done < count;) {
                    const auto at = run + done * sizeof(std::uint32_t);
                    const auto piece = std::min<std::size_t>(count - done, (PageBytes - static_cast<std::size_t>(at % PageBytes)) / sizeof(std::uint32_t));
                    result.push_back({at, std::as_bytes(words.subspan(done, piece))});
                    done += piece;
                }
                lowest->at += count;
                ++lowest->run;
                if (heap) {
                    if (lowest->run == lowest->words->runs.size()) cursors.pop_back();
                    else std::push_heap(cursors.begin(), cursors.end(), later);
                }
            } else {
                return;
            }
        }
    };
    for (const auto& [base, pagePointer] : pages) {
        const auto& page = *pagePointer;
        const auto& marks = recentOnly ? page.recent : page.read;
        if (marks.None()) continue;
        marks.ForEachRun([&](std::size_t first, std::size_t end) {
            const auto address = base + first * sizeof(std::uint32_t);
            emitBefore(address);
            result.push_back({address, std::as_bytes(std::span(page.words).subspan(first, end - first))});
        });
    }
    emitBefore(std::numeric_limits<std::uint64_t>::max());
    return result;
}

std::vector<ShaderRecompiler::MemoryRegion> ShaderMemory::Regions() const {
    return collect(false);
}

std::vector<ShaderRecompiler::MemoryRegion> ShaderMemory::TakeRecentRegions() {
    auto result = collect(true);
    for (auto& [base, page] : pages) page->recent.Reset();
    for (auto& entry : supplied) entry.recent = false;
    return result;
}

void AppendSharedWords(std::vector<SharedWords>& shared, std::size_t region, std::size_t first, std::span<const std::byte> bytes, std::span<const std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> blocks) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes.data());
    const auto count = bytes.size() / sizeof(std::uint32_t);
    if (count == 0) return;
    for (const auto& block : blocks) {
        if (block == nullptr || block->words.empty()) continue;
        const auto begin = reinterpret_cast<std::uintptr_t>(block->words.data());
        const auto end = begin + block->words.size() * sizeof(std::uint32_t);
        if (address < begin || address >= end || end - address < count * sizeof(std::uint32_t) || (address - begin) % sizeof(std::uint32_t) != 0) continue;
        const auto offset = static_cast<std::size_t>((address - begin) / sizeof(std::uint32_t));
        if (!shared.empty()) {
            auto& last = shared.back();
            if (last.first + last.count == first && last.offset + last.count == offset && last.region + last.regions == region && !last.block.owner_before(block) && !block.owner_before(last.block)) {
                last.count += count;
                ++last.regions;
                return;
            }
        }
        shared.push_back({first, count, offset, block, region, 1u});
        return;
    }
}

bool SameCapturedWords(std::span<const std::uint32_t> left, std::span<const SharedWords> leftShared, std::span<const std::uint32_t> right, std::span<const SharedWords> rightShared, std::size_t* compared) {
    if (compared != nullptr) *compared = 0;
    if (left.size() != right.size()) return false;
    std::size_t checked = 0;
    const auto equal = [&](std::size_t from, std::size_t to) {
        checked += to - from;
        return std::equal(left.begin() + static_cast<std::ptrdiff_t>(from), left.begin() + static_cast<std::ptrdiff_t>(to), right.begin() + static_cast<std::ptrdiff_t>(from));
    };
    std::size_t at = 0;
    std::size_t next = 0;
    bool same = true;
    for (const auto& run : leftShared) {
        while (next < rightShared.size() && rightShared[next].first < run.first) ++next;
        if (next == rightShared.size()) break;
        const auto& other = rightShared[next];
        const bool placed = other.first == run.first && other.count == run.count && other.offset == run.offset && run.first >= at && run.first <= left.size() && run.count <= left.size() - run.first;
        if (!placed || run.block.expired() || run.block.owner_before(other.block) || other.block.owner_before(run.block)) continue;
        if (!equal(at, run.first)) {
            same = false;
            break;
        }
        at = run.first + run.count;
    }
    same = same && equal(at, left.size());
    if (compared != nullptr) *compared = checked;
    return same;
}

std::size_t VouchSharedRegions(std::span<const SharedWords> shared, std::size_t regions, ShaderMemory::PendingWriteQuery pending, std::vector<VouchedRegions>& vouched) {
    vouched.clear();
    if (shared.empty() || regions == 0) return 0;
    thread_local std::vector<std::pair<const ShaderRecompiler::SrtGuestWords*, bool>>* blocksSlot = nullptr;
    auto& blocks = ShaderRecompiler::ThreadOwned(blocksSlot);
    blocks.clear();
    auto& totals = CaptureTotals();
    std::size_t words = 0;
    for (const auto& entry : shared) {
        if (entry.regions == 0 || entry.region > regions || entry.regions > regions - entry.region || (!vouched.empty() && entry.region < vouched.back().end)) continue;
        const auto block = entry.block.lock();
        if (block == nullptr || block->low >= block->high) continue;
        auto known = std::find_if(blocks.begin(), blocks.end(), [&](const auto& seen) { return seen.first == block.get(); });
        if (known == blocks.end()) {
            const bool good = ShaderMemory::Vouched(pending, block->low, block->high - block->low, ShaderRecompiler::LoadStamp(*block));
            (good ? totals.vouchedBlocks : totals.refusedBlocks).fetch_add(1, std::memory_order_relaxed);
            known = blocks.insert(blocks.end(), {block.get(), good});
        }
        if (!known->second) continue;
        if (!vouched.empty() && vouched.back().end == entry.region) {
            auto& last = vouched.back();
            last.end += entry.regions;
            last.words += entry.count;
            last.low = std::min(last.low, block->low);
            last.high = std::max(last.high, block->high);
        } else {
            vouched.push_back({entry.region, entry.region + entry.regions, entry.count, block->low, block->high});
        }
        words += entry.count;
    }
    blocks.clear();
    totals.vouchedWords.fetch_add(words, std::memory_order_relaxed);
    return words;
}

DataWordPositionCounts DataWordPositions(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, std::span<const std::pair<std::uint32_t, std::uint64_t>> leaves, std::span<const std::uint64_t> otherReads, std::span<const std::uint32_t> words, std::span<const std::uint32_t> flattenedSrt, std::vector<std::uint32_t>& positions, std::vector<std::uint32_t>& slots) {
    DataWordPositionCounts counts;
    positions.clear();
    slots.clear();
    std::vector<std::size_t> prefix(runs.size() + 1, 0);
    for (std::size_t i = 0; i < runs.size(); ++i) prefix[i + 1] = prefix[i] + static_cast<std::size_t>((runs[i].second - runs[i].first) / sizeof(std::uint32_t));
    std::vector<std::pair<std::uint32_t, std::uint32_t>> found;
    for (const auto& [slot, address] : leaves) {
        if ((address & 3u) != 0) {
            ++counts.unmapped;
            continue;
        }
        if (std::binary_search(otherReads.begin(), otherReads.end(), address)) {
            ++counts.aliased;
            continue;
        }
        auto run = std::upper_bound(runs.begin(), runs.end(), address, [](std::uint64_t value, const std::pair<std::uint64_t, std::uint64_t>& candidate) { return value < candidate.first; });
        if (run == runs.begin() || address + sizeof(std::uint32_t) > (run - 1)->second) {
            ++counts.unmapped;
            continue;
        }
        --run;
        const auto position = prefix[static_cast<std::size_t>(run - runs.begin())] + static_cast<std::size_t>((address - run->first) / sizeof(std::uint32_t));
        if (position >= words.size() || slot >= flattenedSrt.size() || words[position] != flattenedSrt[slot]) {
            ++counts.mismatched;
            continue;
        }
        found.emplace_back(static_cast<std::uint32_t>(position), slot);
    }
    std::sort(found.begin(), found.end());
    positions.reserve(found.size());
    slots.reserve(found.size());
    for (const auto& [position, slot] : found) {
        positions.push_back(position);
        slots.push_back(slot);
    }
    return counts;
}

}
