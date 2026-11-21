#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <stdexcept>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
#include "ImageTableAbi.hpp"
#include <cstring>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "prx/libc/include/GuestAllocations.hpp"

namespace AgcDriver::Graphics {
namespace {

bool overlap(std::uint64_t first, std::size_t firstSize, std::uint64_t second, std::size_t secondSize) {
    return first < second + secondSize && second < first + firstSize;
}

bool registeredImageViewsCurrent(std::span<const ShaderRecompiler::RegisteredImageView> views) {
    if (views.empty()) return true;
    std::vector<std::uintptr_t> addresses;
    addresses.reserve(views.size());
    for (const auto& view : views) addresses.push_back(static_cast<std::uintptr_t>(view.address));
    std::vector<std::shared_ptr<const GuestAllocations::Range>> ranges(views.size());
    GuestAllocations::GuestAllocationsLookupEach_nid_postfix(addresses.data(), addresses.size(), ranges.data());
    for (std::size_t index = 0; index < views.size(); ++index) {
        const auto& view = views[index];
        const auto& range = ranges[index];
        if (view.hasRange != (range != nullptr)) return false;
        if (range != nullptr && (range->address != view.rangeAddress || range->bytes != view.rangeBytes || range->allocationAddress != view.allocationAddress || range->allocationBytes != view.allocationBytes || range->backingSerial != view.backingSerial || range->readable != view.readable || range->writable != view.writable || range->gpu != view.gpu)) return false;
    }
    return true;
}

VkComponentSwizzle ComponentSwizzleFor(std::uint8_t dstSel) {
    switch (dstSel) {
        case 0: return VK_COMPONENT_SWIZZLE_ZERO;
        case 1: return VK_COMPONENT_SWIZZLE_ONE;
        case 4: return VK_COMPONENT_SWIZZLE_R;
        case 5: return VK_COMPONENT_SWIZZLE_G;
        case 6: return VK_COMPONENT_SWIZZLE_B;
        case 7: return VK_COMPONENT_SWIZZLE_A;
        default: throw std::runtime_error("AGC graphics: guest texture descriptor has an invalid destination channel selector " + std::to_string(dstSel));
    }
}

VkComponentMapping ViewComponents(const GuestTextureResource& resource) {
    if (IsConvertedTextureFormat(resource.format) && !IsRepackedTextureFormat(resource.format) && resource.format != static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format11_11_10Float)) return {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    const auto channel = [&](std::uint8_t dstSel) { return TextureComponentChannel(resource.format, ComponentSwizzleFor(dstSel)); };
    return {channel(resource.dstSelX), channel(resource.dstSelY), channel(resource.dstSelZ), channel(resource.dstSelW)};
}

struct AllocationIdentity {
    std::uint64_t rangeAddress = 0;
    std::uint64_t rangeBytes = 0;
    std::uint64_t allocationAddress = 0;
    std::uint64_t allocationBytes = 0;
    std::uint64_t backingSerial = 0;
    std::uint32_t flags = 0;
    bool operator==(const AllocationIdentity&) const = default;
};

AllocationIdentity LookupAllocationIdentity(std::uintptr_t address) {
    const auto range = GuestAllocations::GuestAllocationsLookup_nid_postfix(address, false);
    if (range == nullptr) return {};
    return {range->address, range->bytes, range->allocationAddress, range->allocationBytes, range->backingSerial, (range->readable ? 1u : 0u) | (range->writable ? 2u : 0u) | (range->gpu ? 4u : 0u)};
}

struct AllocationIdentityMemo {
    std::uintptr_t address = 0;
    std::uint64_t generation = 0;
    bool valid = false;
    AllocationIdentity identity;
};

constexpr std::size_t AllocationIdentitySlots = 4096;
struct AllocationIdentityTag {};

AllocationIdentity ImageAllocationIdentity(std::span<const std::uint32_t> words) {
    const auto address = TextureResourceAllocationAddress(words);
    if (!address) return {};
    const auto key = static_cast<std::uintptr_t>(*address);
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    auto& memos = HostThreadLocal<std::unique_ptr<std::array<AllocationIdentityMemo, AllocationIdentitySlots>>, AllocationIdentityTag>();
    if (memos == nullptr) memos = std::make_unique<std::array<AllocationIdentityMemo, AllocationIdentitySlots>>();
    auto& memo = (*memos)[((static_cast<std::uint64_t>(key) * 0x9e3779b97f4a7c15ull) >> 32u) % AllocationIdentitySlots];
    if (memo.valid && memo.address == key && memo.generation == generation) return memo.identity;
    memo.identity = LookupAllocationIdentity(key);
    memo.address = key;
    memo.generation = generation;
    memo.valid = true;
    return memo.identity;
}

struct TextureKey {
    VkDevice device;
    std::array<std::uint32_t, 8> words;
    std::array<std::uint32_t, 4> components;
    bool depthCompare = false;
    AllocationIdentity allocation;
    bool operator==(const TextureKey&) const = default;
};

std::uint64_t hashWords(std::uint64_t hash, const std::uint32_t* words, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) hash = (hash ^ words[i]) * 1099511628211ull;
    return hash;
}

struct TextureKeyHash {
    std::size_t operator()(const TextureKey& key) const noexcept {
        auto hash = 14695981039346656037ull ^ reinterpret_cast<std::uintptr_t>(key.device);
        hash = hashWords(hash, key.words.data(), key.words.size()) ^ static_cast<std::uint64_t>(key.depthCompare);
        for (const auto value : {key.allocation.rangeAddress, key.allocation.rangeBytes, key.allocation.allocationAddress, key.allocation.allocationBytes, key.allocation.backingSerial, static_cast<std::uint64_t>(key.allocation.flags)}) hash = (hash ^ value) * 1099511628211ull;
        return static_cast<std::size_t>(hashWords(hash, key.components.data(), key.components.size()));
    }
};

TextureKey MakeTextureKey(VkDevice device, std::span<const std::uint32_t> words, VkComponentMapping components, bool depthCompare = false) {
    TextureKey key{device, {}, {static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a)}};
    key.depthCompare = depthCompare;
    key.allocation = ImageAllocationIdentity(words);
    std::copy(words.begin(), words.end(), key.words.begin());
    key.words[5] &= ~((0xfffu << 8u) | (1u << 25u));
    key.words[6] &= ~0xffu;
    return key;
}

struct CommittedSlabs {
    std::vector<std::uint32_t> slabs;
    std::uint64_t slabBytes = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    bool operator==(const CommittedSlabs& other) const { return slabs == other.slabs && slabBytes == other.slabBytes; }
};

struct CachedTexture {
    TextureKey key;
    std::uint64_t address;
    std::vector<std::byte> bytes;
    std::shared_ptr<Texture> texture;
    DccKeys keys = DccKeys::Uncompressed;
    std::uint64_t generation = 0;
    std::shared_ptr<StorageTexture> source;
    std::uint64_t sourceVersion = 0;
    std::uint64_t accounted = 0;
    std::uint64_t present = 0;
    std::optional<std::vector<std::pair<std::uint64_t, std::uint64_t>>> committed;
    std::array<std::uint64_t, 4> untracked{};
    std::uint64_t surfaceBytes = 0;
    std::optional<CommittedSlabs> slabs;
};

struct TextureCache {
    AgcDriver::Mutex mutex;
    std::list<CachedTexture> entries;
    std::unordered_map<TextureKey, std::list<CachedTexture>::iterator, TextureKeyHash> index;
    std::unordered_multimap<const StorageTexture*, std::list<CachedTexture>::iterator> views;
    std::vector<std::pair<std::vector<std::byte>, std::uint64_t>> spare;
    std::uint64_t spareBytes = 0;
    std::uint64_t bytes = 0;
    std::size_t passed = 0;
    std::chrono::steady_clock::time_point passedSince{};
    std::uint64_t firstUsesPresent = 0;
    std::unordered_set<TextureKey, TextureKeyHash> firstUses;
    struct Counts {
        std::uint64_t inserted = 0;
        std::uint64_t uncached = 0;
        std::uint64_t evicted = 0;
        std::uint64_t passed = 0;
        std::uint64_t stale = 0;
    } counts;
    std::chrono::steady_clock::time_point countsReported = std::chrono::steady_clock::now();
    VkPhysicalDevice budgetDevice = VK_NULL_HANDLE;
    std::chrono::steady_clock::time_point budgetRead{};
    std::uint64_t budget = 0;
    std::uint64_t reportedBudget = 0;
    std::uint64_t reportedStorage = 0;
    std::chrono::steady_clock::time_point reportedAt{};
    std::uint64_t erasures = 0;
};

TextureCache& Textures() {
    static TextureCache cache;
    return cache;
}

thread_local unsigned sampledCacheHeld = 0;
thread_local unsigned storageCacheHeld = 0;
thread_local unsigned sampledCacheReclaimable = 0;
thread_local unsigned storageCacheReclaimable = 0;

struct CacheHold {
    unsigned& depth;
    explicit CacheHold(unsigned& depth) : depth(depth) { ++depth; }
    ~CacheHold() { --depth; }
    CacheHold(const CacheHold&) = delete;
    CacheHold& operator=(const CacheHold&) = delete;
};

struct UncachedImages {
    AgcDriver::Mutex mutex;
    std::vector<std::weak_ptr<StorageTexture>> images;
    std::atomic<bool> any{false};
};

UncachedImages& Uncached() {
    static UncachedImages uncached;
    return uncached;
}

void noteUncached(const std::shared_ptr<StorageTexture>& image) {
    auto& uncached = Uncached();
    std::lock_guard lock(uncached.mutex);
    uncached.images.push_back(image);
    uncached.any.store(true, std::memory_order_release);
}

std::atomic<bool>& TexturesReleased() {
    static std::atomic<bool> released{false};
    return released;
}

enum IndexCount : std::size_t { IndexAccepted, IndexPartial, IndexFallback, IndexBumped, IndexLookedUp, IndexRebound, IndexCompared, IndexBuilds, IndexCountCount };

std::array<std::atomic<std::uint64_t>, IndexCountCount>& IndexCounts() {
    static std::array<std::atomic<std::uint64_t>, IndexCountCount> counts{};
    return counts;
}

void countIndex(IndexCount which, std::uint64_t amount = 1) {
    IndexCounts()[which].fetch_add(amount, std::memory_order_relaxed);
}

void noteReleased(const std::shared_ptr<Texture>& texture, std::uint64_t address) {
    texture->MarkReleased();
    TexturesReleased().store(true, std::memory_order_release);
    GuestMemory::NoteTextureReleased(address);
}

bool SamplerCacheDisabled() {
    static const bool disabled = std::getenv("APS5_NO_SAMPLER_CACHE") != nullptr;
    return disabled;
}

bool TextureHashEnabled() {
    static const bool disabled = std::getenv("APS5_NO_TEXTURE_HASH") != nullptr;
    return !disabled;
}

std::list<CachedTexture>::iterator findTexture(TextureCache& cache, const TextureKey& key) {
    if (TextureHashEnabled()) {
        const auto found = cache.index.find(key);
        return found == cache.index.end() ? cache.entries.end() : found->second;
    }
    for (auto it = cache.entries.begin(); it != cache.entries.end(); ++it) {
        if (it->key == key) return it;
    }
    return cache.entries.end();
}

void eraseTexture(TextureCache& cache, std::list<CachedTexture>::iterator it) {
    cache.bytes -= it->accounted;
    if (it->source != nullptr) {
        for (auto [view, last] = cache.views.equal_range(it->source.get()); view != last; ++view) {
            if (view->second == it) {
                cache.views.erase(view);
                break;
            }
        }
    }
    if (it->texture.use_count() > 1) noteReleased(it->texture, it->address);
    cache.index.erase(it->key);
    cache.entries.erase(it);
    ++cache.erasures;
}

void dropUncachedViews(TextureCache& cache) {
    auto& uncached = Uncached();
    if (!uncached.any.load(std::memory_order_acquire)) return;
    std::vector<std::weak_ptr<StorageTexture>> images;
    {
        std::lock_guard lock(uncached.mutex);
        images.swap(uncached.images);
        uncached.any.store(false, std::memory_order_release);
    }
    for (const auto& weak : images) {
        const auto image = weak.lock();
        if (image == nullptr) continue;
        for (auto view = cache.views.find(image.get()); view != cache.views.end(); view = cache.views.find(image.get())) eraseTexture(cache, view->second);
    }
}

void touchTexture(TextureCache& cache, std::list<CachedTexture>::iterator it) {
    cache.entries.splice(cache.entries.begin(), cache.entries, it);
    it->present = Recorder::Presents();
}

constexpr std::uint64_t SpareSnapshotBytes = 256ull << 20;

void expireSpareSnapshots(TextureCache& cache, std::uint64_t present) {
    while (!cache.spare.empty() && present - cache.spare.front().second > SpareSnapshotPresents) {
        cache.spareBytes -= cache.spare.front().first.size();
        cache.spare.erase(cache.spare.begin());
    }
}

void parkSnapshot(TextureCache& cache, std::vector<std::byte>& bytes) {
    const auto size = bytes.size();
    if (size == 0) return;
    const auto present = Recorder::Presents();
    expireSpareSnapshots(cache, present);
    while (!cache.spare.empty() && cache.spareBytes + size > SpareSnapshotBytes) {
        cache.spareBytes -= cache.spare.front().first.size();
        cache.spare.erase(cache.spare.begin());
    }
    if (cache.spareBytes + size <= SpareSnapshotBytes) {
        cache.spareBytes += size;
        cache.spare.emplace_back(std::move(bytes), present);
    }
    bytes = std::vector<std::byte>{};
}

std::vector<std::byte> spareSnapshot(TextureCache& cache, std::size_t bytes) {
    expireSpareSnapshots(cache, Recorder::Presents());
    for (auto spare = cache.spare.begin(); spare != cache.spare.end(); ++spare) {
        if (spare->first.size() != bytes) continue;
        cache.spareBytes -= bytes;
        std::vector<std::byte> taken = std::move(spare->first);
        cache.spare.erase(spare);
        return taken;
    }
    return {};
}

bool admitTexture(TextureCache& cache, const TextureKey& key, std::uint64_t incoming, std::uint64_t budget) {
    if (cache.bytes + incoming <= budget) return true;
    if (const auto now = std::chrono::steady_clock::now(); now - cache.passedSince >= std::chrono::seconds(1)) {
        cache.passed = 0;
        cache.passedSince = now;
    }
    const auto present = Recorder::Presents();
    if (cache.firstUsesPresent != present) {
        cache.firstUses.clear();
        cache.firstUsesPresent = present;
    }
    const bool scan = incoming != 0 && present != 0;
    const bool firstUse = scan && !cache.firstUses.contains(key);
    const auto ceiling = budget + budget / 4u;
    while (!cache.entries.empty() && cache.bytes + incoming > budget) {
        const auto back = std::prev(cache.entries.end());
        const bool over = cache.bytes + incoming > ceiling;
        if (scan && !over && (firstUse ? back->present + 1u >= present : back->present == present)) {
            cache.firstUses.insert(key);
            ++cache.counts.uncached;
            return false;
        }
        const bool keep = back->accounted == 0 || (!over && back->texture.use_count() > 1);
        if (keep && cache.passed < cache.entries.size()) {
            cache.entries.splice(cache.entries.begin(), cache.entries, back);
            ++cache.passed;
            ++cache.counts.passed;
        } else if (keep && !over) {
            break;
        } else {
            eraseTexture(cache, back);
            ++cache.counts.evicted;
        }
    }
    return true;
}

void reportCacheCounts(TextureCache& cache, std::uint64_t budget) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - cache.countsReported < std::chrono::seconds(10)) return;
    cache.countsReported = now;
    const auto counts = std::exchange(cache.counts, {});
    AgcDriver::ProfilePrint_nid_no_patch("[texture-cache] (10 s): %llu textures cached, %llu used uncached (first use this frame with the least recently used entry used this frame or the last, or a later use with it used this frame), %llu evicted, %llu held ones passed over, %llu found changed; %llu MiB in %zu entries, budget %llu MiB, frame %llu\n", static_cast<unsigned long long>(counts.inserted), static_cast<unsigned long long>(counts.uncached), static_cast<unsigned long long>(counts.evicted), static_cast<unsigned long long>(counts.passed), static_cast<unsigned long long>(counts.stale), static_cast<unsigned long long>(cache.bytes >> 20u), cache.entries.size(), static_cast<unsigned long long>(budget >> 20u), static_cast<unsigned long long>(Recorder::Presents()));
}

struct ImageMemoEntry {
    VkDevice device = VK_NULL_HANDLE;
    std::array<std::uint32_t, 8> words{};
    GuestTextureResource resource{};
    std::uint64_t guestBytes = 0;
    AllocationIdentity allocation;
    VkComponentMapping components{};
    bool hasEntry = false;
    std::list<CachedTexture>::iterator entry{};
    std::uint64_t erasures = 0;
};

constexpr std::size_t ImageMemoSlots = 1024;
struct ImageMemoTag {};

ImageMemoEntry& imageMemoSlot(std::span<const std::uint32_t> words) {
    auto& imageMemo = HostThreadLocal<std::unique_ptr<std::array<ImageMemoEntry, ImageMemoSlots>>, ImageMemoTag>();
    if (imageMemo == nullptr) imageMemo = std::make_unique<std::array<ImageMemoEntry, ImageMemoSlots>>();
    const auto hash = hashWords(14695981039346656037ull, words.data(), words.size());
    return (*imageMemo)[(hash ^ (hash >> 29u)) % ImageMemoSlots];
}

struct SampledKeyProofEntry {
    std::uint64_t dccAddress = 0;
    std::uint64_t guestBytes = 0;
    std::uint32_t format = 0;
    bool dccAlphaOnMsb = false;
    DccKeyProof proof;
};

constexpr std::size_t SampledKeyProofSlots = 1024;
struct SampledKeyProofTag {};

SampledKeyProofEntry& sampledKeyProofSlot(std::uint64_t dccAddress, std::uint64_t guestBytes, std::uint32_t format, bool dccAlphaOnMsb) {
    auto& proofs = HostThreadLocal<std::unique_ptr<std::array<SampledKeyProofEntry, SampledKeyProofSlots>>, SampledKeyProofTag>();
    if (proofs == nullptr) proofs = std::make_unique<std::array<SampledKeyProofEntry, SampledKeyProofSlots>>();
    const auto hash = dccAddress ^ (guestBytes * 0x9e3779b97f4a7c15ull) ^ (static_cast<std::uint64_t>(format) << 32u) ^ (dccAlphaOnMsb ? 0x94d049bb133111ebull : 0ull);
    return (*proofs)[(hash ^ (hash >> 29u)) % SampledKeyProofSlots];
}

const ImageMemoEntry& decodedImage(VkDevice device, std::span<const std::uint32_t> words) {
    auto& slot = imageMemoSlot(words);
    const auto allocation = ImageAllocationIdentity(words);
    if (slot.device == device && slot.allocation == allocation && std::equal(words.begin(), words.end(), slot.words.begin())) return slot;
    slot.device = VK_NULL_HANDLE;
    slot.resource = DecodeTextureResource(words);
    slot.guestBytes = SurfaceGuestBytes(slot.resource);
    slot.allocation = allocation;
    slot.components = ViewComponents(slot.resource);
    std::copy(words.begin(), words.end(), slot.words.begin());
    slot.hasEntry = false;
    slot.device = device;
    return slot;
}

std::list<CachedTexture>::iterator findSampledEntry(TextureCache& cache, VkDevice device, const std::array<std::uint32_t, 8>& words, VkComponentMapping components) {
    auto& slot = imageMemoSlot(words);
    const bool same = slot.device == device && slot.allocation == ImageAllocationIdentity(words) && slot.words == words && slot.components.r == components.r && slot.components.g == components.g && slot.components.b == components.b && slot.components.a == components.a;
    if (same && slot.hasEntry && slot.erasures == cache.erasures) return slot.entry;
    const auto it = findTexture(cache, MakeTextureKey(device, words, components));
    if (same) {
        slot.hasEntry = it != cache.entries.end();
        slot.entry = it;
        slot.erasures = cache.erasures;
    }
    return it;
}

struct TextureCounters {
    std::atomic<std::uint64_t> fromStorage{0};
    std::atomic<std::uint64_t> snapshots{0};
    std::atomic<std::uint64_t> cleared{0};
    std::atomic<std::uint64_t> imported{0};
    std::atomic<std::uint64_t> pendingReads{0};
    std::atomic<std::uint64_t> storageFallbacks{0};
    std::atomic<std::uint64_t> records{0};
    std::atomic<std::uint64_t> fastHits{0};
    std::atomic<std::uint64_t> fastMisses{0};
    std::atomic<std::uint64_t> storageHits{0};
    std::atomic<std::uint64_t> storageCreated{0};
    std::atomic<std::uint64_t> ownRefreshes{0};
    std::atomic<std::uint64_t> tablesReused{0};
    std::atomic<std::uint64_t> tablesReplayed{0};
    std::atomic<std::uint64_t> tablesResolved{0};
    std::atomic<std::uint64_t> tableWritesKept{0};
    std::atomic<std::uint64_t> tableWritesMade{0};
    std::atomic<std::uint64_t> variantSurfaces{0};
    std::atomic<std::uint64_t> variantSubstituted{0};
    std::atomic<std::uint64_t> variantRejected{0};
    std::atomic<std::uint64_t> arrayViewsShared{0};
    std::atomic<std::uint64_t> untrackedKept{0};
    std::atomic<std::uint64_t> untrackedCompared{0};
    std::atomic<std::int64_t> lastReport{0};
};

TextureCounters& TextureCounts() {
    static TextureCounters counters;
    return counters;
}

thread_local std::uint64_t sampledLookupKeyScans = 0;
thread_local std::uint64_t sampledLookupKeyProofs = 0;

bool TextureCountersReported() {
    static const bool reported = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return reported;
}

void reportTextureCounters() {
    if (!TextureCountersReported()) return;
    auto& counters = TextureCounts();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = counters.lastReport.load();
    if (nowMs - last < 10000 || !counters.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto count = [](const std::atomic<std::uint64_t>& value) { return static_cast<unsigned long long>(value.load(std::memory_order_relaxed)); };
    AgcDriver::ProfilePrint_nid_no_patch("[textures] sampled created: %llu from storage images, %llu cleared on the GPU, %llu uploaded on the GPU from host imports, %llu snapshots (%llu snapshot reads over recorded writes inside cachedTexture, %llu storage-path fallbacks); stage-A records %llu: %llu fast hits, %llu full lookups; storage images %llu hits, %llu created, %llu own-object refreshes; image tables %llu reused, %llu resolved; dynamic-resolution variants: %llu surfaces, %llu entries substituted, %llu entries rejected; %llu one-layer 2D array entries bound to the 2D texture of their surface; untracked snapshots %llu kept by the write-watch hint, %llu compared\n", count(counters.fromStorage), count(counters.cleared), count(counters.imported), count(counters.snapshots), count(counters.pendingReads), count(counters.storageFallbacks), count(counters.records), count(counters.fastHits), count(counters.fastMisses), count(counters.storageHits), count(counters.storageCreated), count(counters.ownRefreshes), count(counters.tablesReused), count(counters.tablesResolved), count(counters.variantSurfaces), count(counters.variantSubstituted), count(counters.variantRejected), count(counters.arrayViewsShared), count(counters.untrackedKept), count(counters.untrackedCompared));
}

struct LookupRecord {
    const void* object;
    GuestTextureResource resource;
    std::uint64_t bytes;
    DccKeys keys;
    std::uint64_t generation;
    const StorageTexture* source;
    bool depth = false;
    std::uint64_t committedGeneration = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> committed;
};

thread_local std::vector<LookupRecord>* lookupLogSlot = nullptr;
constexpr std::size_t LookupLogBound = 1024;
constexpr std::size_t NoBuildLookups = std::numeric_limits<std::size_t>::max();
thread_local std::size_t buildLookupsStart = NoBuildLookups;
thread_local bool buildLookupsPastBound = false;
std::atomic<std::uint64_t> lookupBuildsPastBound{0};

std::vector<LookupRecord>& lookupLog() {
    return ShaderRecompiler::ThreadOwned(lookupLogSlot);
}

void logLookup(const LookupRecord& record) {
    auto& log = lookupLog();
    if (log.size() >= LookupLogBound) {
        const auto dropped = std::min(LookupLogBound / 2, buildLookupsStart);
        if (dropped < LookupLogBound / 2) buildLookupsPastBound = true;
        log.erase(log.begin(), log.begin() + static_cast<std::ptrdiff_t>(dropped));
        if (buildLookupsStart != NoBuildLookups) buildLookupsStart -= dropped;
    }
    log.push_back(record);
}

class BuildLookups {
public:
    BuildLookups() : outermost(buildLookupsStart == NoBuildLookups) { buildLookupsStart = std::min(buildLookupsStart, lookupLog().size()); }
    ~BuildLookups() {
        if (!outermost) return;
        buildLookupsStart = NoBuildLookups;
        if (buildLookupsPastBound) lookupBuildsPastBound.fetch_add(1, std::memory_order_relaxed);
        buildLookupsPastBound = false;
        auto& log = lookupLog();
        if (log.size() > LookupLogBound) log.erase(log.begin(), log.end() - static_cast<std::ptrdiff_t>(LookupLogBound));
        if (log.capacity() > LookupLogBound + LookupLogBound / 2) log.shrink_to_fit();
    }
    BuildLookups(const BuildLookups&) = delete;
    BuildLookups& operator=(const BuildLookups&) = delete;

private:
    bool outermost;
};

void refreshForView(StorageTexture& source, const GuestTextureResource& resource) {
    if (resource.dimension == TextureDimension::k2D && source.ImageLayers() > 1) source.RefreshLayers(resource.baseArray, 1);
    else source.Refresh();
}

std::shared_ptr<StorageTexture> cachedStorageTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, std::uint32_t mip, std::uint64_t guestBytes = 0, bool written = false, std::optional<std::pair<std::uint32_t, std::uint32_t>> layers = std::nullopt);

std::uint64_t heldBytes(const StorageTexture& image) {
    return image.BudgetBytes();
}

bool MetadataMoved(const StorageTexture& image, const GuestTextureResource& resource) {
    return resource.dccAddress != 0 && image.Descriptor().dccAddress != resource.dccAddress && !image.ServesKeysAt(resource.dccAddress);
}

std::atomic<bool> sampledFromStorageForTest{true};

bool SampledFromStorageEligible(const Context& context, std::uint32_t format, std::uint64_t address, std::uint64_t guestBytes) {
    static const bool disabled = std::getenv("APS5_NO_SAMPLED_FROM_STORAGE") != nullptr;
    if (disabled || !sampledFromStorageForTest.load(std::memory_order_relaxed) || IsBlockCompressed(format) || !StorageFormatAvailable(context, format)) return false;
    return HostImportCovers(context, address, static_cast<std::size_t>(guestBytes));
}

bool SampledFromStorageEligible(const Context& context, const GuestTextureResource& resource, std::uint64_t guestBytes) {
    return SampledFromStorageEligible(context, resource.format, resource.baseAddress, guestBytes);
}

bool ClearedViewEnabled() {
    static const bool disabled = std::getenv("APS5_NO_CLEARED_VIEW") != nullptr;
    return !disabled;
}

std::optional<VkClearColorValue> SampledClearColor(const Context& context, const GuestTextureResource& resource, DccKeys keys, bool depthCompare) {
    if (depthCompare || !IsDccClear(keys) || keys == DccKeys::ClearRegister) return std::nullopt;
    if (IsBlockCompressed(resource.format) || IsRepackedTextureFormat(resource.format) || SrgbExpandsToRgba8(context, resource.format)) return std::nullopt;
    const auto format = SampledTextureFormat(context, resource.format);
    if (format != DccClearTextureFormat(resource.format)) return std::nullopt;
    std::array<std::uint32_t, 4> pattern{};
    if (!FillDccClear(format, keys, resource.dccAlphaOnMsb, std::as_writable_bytes(std::span(pattern)))) return std::nullopt;
    VkClearColorValue clear{};
    if (!ClearColorForTexel(format, BytesPerElement(resource.format), pattern, clear)) return std::nullopt;
    return clear;
}

std::uint64_t DepthCopyMinBytes() {
    static const std::uint64_t bytes = [] {
        const char* text = std::getenv("APS5_DEPTH_COPY_MIN_KIB");
        return (text != nullptr ? std::strtoull(text, nullptr, 10) : 1024ull) << 10u;
    }();
    return bytes;
}

struct StorageFailures {
    AgcDriver::Mutex mutex;
    std::map<std::pair<std::uint64_t, std::uint64_t>, std::uint64_t> generations;
};

StorageFailures& StorageFailed() {
    static StorageFailures failures;
    return failures;
}

std::shared_ptr<StorageTexture> sampledStorageSource(const Context& context, const GuestTextureResource& resource, std::uint64_t guestBytes) {
    auto& counters = TextureCounts();
    auto& failures = StorageFailed();
    const auto surface = std::make_pair(resource.baseAddress, guestBytes);
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    {
        std::lock_guard lock(failures.mutex);
        if (const auto it = failures.generations.find(surface); it != failures.generations.end()) {
            if (it->second == generation) {
                counters.storageFallbacks.fetch_add(1, std::memory_order_relaxed);
                return nullptr;
            }
            failures.generations.erase(it);
        }
    }
    try {
        return cachedStorageTexture(context, {}, resource, 0, guestBytes);
    } catch (const std::exception& error) {
        counters.storageFallbacks.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(failures.mutex);
        if (failures.generations.size() < 4096 && failures.generations.emplace(surface, generation).second) std::fprintf(stderr, "[textures] sampled texture 0x%llx (%ux%u format %u) keeps the snapshot path: %s\n", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, error.what());
        return nullptr;
    }
}

std::uint64_t storageCacheBytes();

}
}
