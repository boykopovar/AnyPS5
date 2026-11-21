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

struct CaptureProfile {
    std::atomic<std::uint64_t> captures{0};
    std::atomic<std::uint64_t> reads{0};
    std::atomic<std::uint64_t> pages{0};
    std::atomic<std::uint64_t> captureNanoseconds{0};
    std::atomic<std::uint64_t> pagesWordwise{0};
    std::atomic<std::uint64_t> wordWaits{0};
    std::atomic<std::uint64_t> branchReadsRefused{0};
    std::atomic<std::uint64_t> branchCapturesRedone{0};
    std::atomic<std::uint64_t> wordWaitNanoseconds{0};
    std::atomic<std::uint64_t> wordsRaw{0};
    std::atomic<std::uint64_t> wordsVerified{0};
    std::atomic<std::uint64_t> wordMismatches{0};
    std::atomic<std::uint64_t> tablesKept{0};
    std::atomic<std::uint64_t> wordsKnown{0};
    std::atomic<std::uint64_t> wordsKnownVerified{0};
    std::atomic<std::uint64_t> wordsKnownMismatches{0};
    std::atomic<std::uint64_t> observationsSkipped{0};
    std::atomic<std::uint64_t> resolveNanoseconds{0};
    std::atomic<std::uint64_t> pageReadNanoseconds{0};
    std::atomic<std::uint64_t> wordReadNanoseconds{0};
    std::atomic<std::uint64_t> hookWaitNanoseconds{0};
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

std::uint64_t imageViewGeneration(void*) {
    return GuestAllocations::GuestAllocationsGeneration_nid_postfix();
}

std::uint64_t PageBacking(std::uint64_t base, std::uint64_t bytes) {
    std::uint64_t mix = 1469598103934665603ull;
    const auto fold = [&](std::uint64_t value) { mix = (mix ^ value) * 1099511628211ull; };
    for (auto cursor = base; cursor < base + bytes;) {
        const auto range = GuestAllocations::GuestAllocationsLookup_nid_postfix(static_cast<std::uintptr_t>(cursor), true);
        if (range == nullptr || range->address >= base + bytes) break;
        fold(range->address);
        fold(range->bytes);
        fold(range->allocationAddress);
        fold(range->backingSerial);
        fold((range->readable ? 1u : 0u) | (range->writable ? 2u : 0u) | (range->gpu ? 4u : 0u));
        cursor = range->address + range->bytes;
    }
    return mix;
}

bool normalizeImageView(void*, std::uint32_t maxArrayLayers, std::array<std::uint32_t, 8>& descriptor, ShaderRecompiler::SrtImageViewIdentity& identity) {
    identity.generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (((descriptor[3] >> 28u) & 0xfu) != 12u) return false;
    const auto resource = Graphics::DecodeTextureResource(descriptor);
    identity.address = resource.baseAddress;
    const auto range = GuestAllocations::GuestAllocationsLookup_nid_postfix(static_cast<std::uintptr_t>(resource.baseAddress), false);
    if (range != nullptr) {
        identity.hasRange = true;
        identity.readable = range->readable;
        identity.writable = range->writable;
        identity.gpu = range->gpu;
        identity.rangeAddress = range->address;
        identity.rangeBytes = range->bytes;
        identity.allocationAddress = range->allocationAddress;
        identity.allocationBytes = range->allocationBytes;
        identity.backingSerial = range->backingSerial;
    }
    if (maxArrayLayers == 0u || resource.dimension != Graphics::TextureDimension::k1DArray || range == nullptr) return false;
    const auto rawCount = resource.depthOrLastArray >= resource.baseArray ? resource.depthOrLastArray - resource.baseArray + 1u : 0u;
    const auto geometry = Graphics::DescribeSurface(resource);
    if (geometry.layerBytes == 0u || geometry.thick || geometry.layers != resource.depthOrLastArray + 1u) return false;
    if (const auto xorMode = Graphics::XorSwizzleMode(resource.tileMode); xorMode != 0u) {
        const auto* equation = Graphics::FindTextureSwizzleEquation(xorMode, Graphics::BytesPerElement(resource.format));
        if (equation == nullptr || std::any_of(equation->bits.begin(), equation->bits.end(), [](std::uint32_t bit) { return (bit & 0xff000000u) != 0u; })) return false;
    }
    if (!range->readable || !range->gpu || range->bytes == 0u || range->address > std::numeric_limits<std::uint64_t>::max() - range->bytes || range->allocationAddress > std::numeric_limits<std::uint64_t>::max() - range->allocationBytes) return false;
    const auto rangeEnd = range->address + range->bytes;
    const auto allocationEnd = range->allocationAddress + range->allocationBytes;
    if (resource.baseAddress < range->address || resource.baseAddress > rangeEnd || range->address < range->allocationAddress || rangeEnd > allocationEnd) return false;
    const auto offset = resource.baseAddress - range->address;
    if (offset % geometry.layerBytes != 0u) return false;
    const auto coveredBytes = rangeEnd - resource.baseAddress;
    if (coveredBytes % geometry.layerBytes != 0u) return false;
    const auto completeSlices = coveredBytes / geometry.layerBytes;
    if (completeSlices <= resource.baseArray) return false;
    if (rawCount <= maxArrayLayers) return false;
    const auto available = completeSlices - resource.baseArray;
    const auto effectiveCount = std::min<std::uint64_t>(rawCount, available);
    if (effectiveCount == 0u || effectiveCount == rawCount) return false;
    const auto lastArray = static_cast<std::uint64_t>(resource.baseArray) + effectiveCount - 1u;
    if (lastArray > 0x1fffu) return false;
    descriptor[4] = (descriptor[4] & ~0x1fffu) | static_cast<std::uint32_t>(lastArray);
    return true;
}

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

bool BranchesSkipPendingWords() {
    static const bool skip = std::getenv("APS5_WAIT_PENDING_BRANCHES") == nullptr;
    return skip;
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

ShaderMemory::ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> regions, PendingWriteQuery pendingWrite, PendingWriteObserver observe, HookWaitCounter hookWaits, PendingWriteQuery pendingSpan) : pendingWrite(pendingWrite), observe(observe), hookWaits(hookWaits), pendingSpan(pendingSpan != nullptr ? pendingSpan : pendingWrite), spanQuery(pendingSpan != nullptr) {
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
    owned->kept.Reset();
    owned->wordwise = false;
    owned->lazy = false;
    owned->backing = PageBacking(base, PageBytes);
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
    preparePage(page, base);
    return page;
}

void ShaderMemory::preparePage(Page& page, std::uint64_t base) {
    if (GuestMemory::Accessible(reinterpret_cast<const void*>(base), PageBytes)) {
        if (WordwisePages() && pendingSpan != nullptr && pendingSpan(base, PageBytes, {}) != PendingWrite::None) {
            page.wordwise = true;
            ++CaptureTotals().pagesWordwise;
            return;
        }
        const auto started = CaptureProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        GuestMemory::GuardRead(base, PageBytes);
        GuestMemory::FlushGpuWrites(base, PageBytes);
        if (CaptureProfiled()) CaptureTotals().pageReadNanoseconds += NanosecondsSince(started);
        page.lazy = true;
    }
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
    if (page.wordwise && page.valid.Test(index) && !page.kept.Test(index) &&
        (self.forceSync != 0u ? self.pendingSpan(address, checkSpanBytes, {}) != PendingWrite::None
                              : (spanInvalidatesCache(self.pendingSpan(address, checkSpanBytes, {})) ||
                                 (checkSpanBytes > sizeof(*value) && spanInvalidatesCache(self.pendingSpan(address, sizeof(*value), {})))))) {
        page.valid.Clear(index);
        page.raw.Clear(index);
    }
    if (!page.valid.Test(index)) {
        std::uint32_t word = 0;
        auto policy = PendingWrite::Sync;
        bool forcedRaw = false;
        if (page.wordwise) {
            if (self.descriptorSpanBytes > sizeof(word) &&
                (self.forceSync != 0u ? self.pendingSpan(address, self.descriptorSpanBytes, {}) != PendingWrite::None
                                      : spanNeedsSync(self.pendingSpan(address, self.descriptorSpanBytes, {})))) {
                policy = PendingWrite::Sync;
                forcedRaw = self.forceSync != 0u && !spanNeedsSync(self.pendingSpan(address, self.descriptorSpanBytes, {}));
            } else {
                policy = self.pendingWrite(address, sizeof(word), std::as_writable_bytes(std::span(&word, 1)));
                if (self.forceSync != 0u && policy != PendingWrite::None) {
                    forcedRaw = !spanNeedsSync(policy);
                    policy = PendingWrite::Sync;
                }
                if (policy != PendingWrite::None && policy != PendingWrite::Sync && !GuestMemory::Accessible(reinterpret_cast<const void*>(address), sizeof(word))) policy = PendingWrite::Sync;
            }
        }
        const auto hookWaits = [&] { return self.hookWaits != nullptr ? self.hookWaits() : 0; };
        const auto report = [&](std::uint64_t waitsBefore, bool unchanged) {
            if (self.hookWaits == nullptr || self.hookWaits() != waitsBefore) self.observe(address, unchanged);
            else ++CaptureTotals().observationsSkipped;
        };
        const auto fetchStarted = CaptureProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        bool waited = false;
        if (policy == PendingWrite::KnownValue || policy == PendingWrite::VerifyKnownValue) {
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
        } else if (page.wordwise && policy == PendingWrite::Sync && !forcedRaw && !self.waitBranches && BranchesSkipPendingWords() && ShaderRecompiler::SrtEvaluatingBranch()) {
            ++self.branchReadsRefused;
            if (CaptureProfiled()) ++CaptureTotals().branchReadsRefused;
            return false;
        } else {
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
        if (waited || (page.wordwise && policy == PendingWrite::Sync && !forcedRaw)) {
            const bool invalidated = self.forceSync != 0u && !waited ? self.invalidateChangedRawPages() : self.invalidateRawPages();
            if (invalidated) ++self.rawInvalidations;
        }
        if (CaptureProfiled()) CaptureTotals().wordReadNanoseconds += NanosecondsSince(fetchStarted);
        page.words[index] = word;
        page.valid.Set(index);
        if (page.wordwise && (policy == PendingWrite::Raw || policy == PendingWrite::KnownValue || (forcedRaw && !waited))) page.raw.Set(index);
        else page.raw.Clear(index);
    }
    page.read.Set(index);
    page.recent.Set(index);
    return true;
}

}
