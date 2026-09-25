#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libc/include/General.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include <cstring>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include "prx/libc/include/GuestAllocations.hpp"

namespace AgcDriver::Graphics {
namespace {

bool overlap(std::uint64_t first, std::size_t firstSize, std::uint64_t second, std::size_t secondSize) {
    return first < second + secondSize && second < first + firstSize;
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

// Sampled textures are reused across draws and dispatches while their guest bytes are unchanged; a
// byte copy of the guest surface validates each reuse, so CPU or GPU writes to it force a re-upload.
// The key is everything a lookup matches: the device, the eight descriptor words and the swizzle.
struct TextureKey {
    VkDevice device;
    std::array<std::uint32_t, 8> words;
    std::array<std::uint32_t, 4> components;
    bool operator==(const TextureKey&) const = default;
};

std::uint64_t hashWords(std::uint64_t hash, const std::uint32_t* words, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) hash = (hash ^ words[i]) * 1099511628211ull;
    return hash;
}

struct TextureKeyHash {
    std::size_t operator()(const TextureKey& key) const noexcept {
        auto hash = 14695981039346656037ull ^ reinterpret_cast<std::uintptr_t>(key.device);
        hash = hashWords(hash, key.words.data(), key.words.size());
        return static_cast<std::size_t>(hashWords(hash, key.components.data(), key.components.size()));
    }
};

TextureKey MakeTextureKey(VkDevice device, std::span<const std::uint32_t> words, VkComponentMapping components) {
    TextureKey key{device, {}, {static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a)}};
    std::copy(words.begin(), words.end(), key.words.begin());
    return key;
}

struct CachedTexture {
    TextureKey key;
    std::uint64_t address;
    std::vector<std::byte> bytes;
    std::shared_ptr<Texture> texture;
    // A fast-cleared surface is cached as its clear texels; it stays valid while the keys are unchanged.
    DccKeys keys = DccKeys::Uncompressed;
    // Write generation the snapshot is known current at (see GuestMemory::CollectWrites).
    std::uint64_t generation = 0;
    // A texture viewing a storage image on the GPU has no snapshot; it stays valid while that image
    // is still its surface's cached image and nothing wrote the guest memory since it matched.
    std::shared_ptr<StorageTexture> source;
    std::uint64_t sourceVersion = 0;
    std::uint64_t accounted = 0;
};

// Entries in use order (front = most recent) with a hash index by key: a lookup is O(1) and the
// eviction takes the back. APS5_NO_TEXTURE_HASH=1 finds entries by scanning the list (the index is
// still kept), the linear lookup of before.
struct TextureCache {
    std::mutex mutex;
    std::list<CachedTexture> entries;
    std::unordered_map<TextureKey, std::list<CachedTexture>::iterator, TextureKeyHash> index;
    std::uint64_t bytes = 0;
};

TextureCache& Textures() {
    static TextureCache cache;
    return cache;
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
    cache.index.erase(it->key);
    cache.entries.erase(it);
}

// Moves an entry to the front (most recently used).
void touchTexture(TextureCache& cache, std::list<CachedTexture>::iterator it) {
    cache.entries.splice(cache.entries.begin(), cache.entries, it);
}

// APS5_PROFILE_DRAW: what the sampled-texture and storage-image lookups did, printed as [textures]
// every 10 s (cumulative). Storage-sourced sampled textures need no CPU read; snapshots do, and a
// pending read is a snapshot compare or read made while recorded work still wrote the surface (the
// flush hook may find that work already signaled, so this bounds the hook syncs from above; the
// [hooksync] line attributes the actual waits).
struct TextureCounters {
    std::atomic<std::uint64_t> fromStorage{0};
    std::atomic<std::uint64_t> snapshots{0};
    std::atomic<std::uint64_t> pendingReads{0};
    std::atomic<std::uint64_t> storageFallbacks{0};
    std::atomic<std::uint64_t> records{0};
    std::atomic<std::uint64_t> fastHits{0};
    std::atomic<std::uint64_t> fastMisses{0};
    std::atomic<std::uint64_t> storageHits{0};
    std::atomic<std::uint64_t> storageCreated{0};
    std::atomic<std::int64_t> lastReport{0};
};

TextureCounters& TextureCounts() {
    static TextureCounters counters;
    return counters;
}

void reportTextureCounters() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& counters = TextureCounts();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = counters.lastReport.load();
    if (nowMs - last < 10000 || !counters.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto count = [](const std::atomic<std::uint64_t>& value) { return static_cast<unsigned long long>(value.load(std::memory_order_relaxed)); };
    std::fprintf(stderr, "[textures] sampled created: %llu from storage images, %llu snapshots (%llu snapshot reads over recorded writes inside cachedTexture, %llu storage-path fallbacks); stage-A records %llu: %llu fast hits, %llu full lookups; storage images %llu hits, %llu created\n", count(counters.fromStorage), count(counters.snapshots), count(counters.pendingReads), count(counters.storageFallbacks), count(counters.records), count(counters.fastHits), count(counters.fastMisses), count(counters.storageHits), count(counters.storageCreated));
}

// What the sampled-texture lookups on this thread proved their returned objects current against,
// taken by object into the ShaderResources being built or revalidated (captureValidation), which
// then repeats the proof from write stamps on its next Revalidate. The lookups run inside one build
// on one thread; records nobody takes (a build that threw, a resident target looked up outside a
// build, a thread that never captures) are dropped by the next capture or by the bound below.
struct LookupRecord {
    const void* object;
    GuestTextureResource resource;
    std::uint64_t bytes;
    DccKeys keys;
    std::uint64_t generation;
    const StorageTexture* source;
};

thread_local std::vector<LookupRecord> lookupLog;

void logLookup(const LookupRecord& record) {
    // The bound drops the oldest half, never everything: one build's lookups (a few dozen at most)
    // are the newest records, and a capture must find them even when the bound trips mid-build.
    constexpr std::size_t bound = 1024;
    if (lookupLog.size() >= bound) lookupLog.erase(lookupLog.begin(), lookupLog.begin() + bound / 2);
    lookupLog.push_back(record);
}

std::shared_ptr<StorageTexture> cachedStorageTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, std::uint32_t mip, std::uint64_t guestBytes = 0);

// Whether a sampled texture over `resource` can be a view of the surface's cached storage image
// instead of a CPU snapshot (see cachedTexture): the format has a storage form and is not block
// compressed, and the surface lives in host-imported memory, where the image uploads and refreshes
// GPU-direct (a snapshot of such a surface reads and compares its bytes on the CPU, waiting for
// the recorded work that wrote them). APS5_NO_SAMPLED_FROM_STORAGE=1 keeps every sampled texture a
// snapshot as before.
bool SampledFromStorageEligible(const Context& context, std::uint32_t format, std::uint64_t address, std::uint64_t guestBytes) {
    static const bool disabled = std::getenv("APS5_NO_SAMPLED_FROM_STORAGE") != nullptr;
    if (disabled || IsBlockCompressed(format) || !StorageFormatAvailable(context, format)) return false;
    return HostImportCovers(context, address, static_cast<std::size_t>(guestBytes));
}

bool SampledFromStorageEligible(const Context& context, const GuestTextureResource& resource, std::uint64_t guestBytes) {
    return SampledFromStorageEligible(context, resource.format, resource.baseAddress, guestBytes);
}

// Surfaces whose storage image could not be made (no writable committed pages, say): remembered
// so the sampled lookup does not throw and fall back on every use. A failure is keyed by the surface
// (address and size: the heap reuses addresses) and holds only while the guest mappings are what
// they were when it failed (the allocation generation): pages committed later, or another surface
// at the address, get a fresh attempt.
struct StorageFailures {
    std::mutex mutex;
    std::map<std::pair<std::uint64_t, std::uint64_t>, std::uint64_t> generations;
};

StorageFailures& StorageFailed() {
    static StorageFailures failures;
    return failures;
}

// The cached storage image for a sampled descriptor, or null when it cannot be made (then the
// snapshot path serves the descriptor, as before). Every null return counts as a fallback.
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

// `guestBytes` is the surface size when the caller described the surface already (0: described here).
std::shared_ptr<Texture> cachedTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components, std::uint64_t guestBytes = 0) {
    static const bool disabled = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    if (guestBytes == 0) guestBytes = DescribeSurface(resource).guestBytes;
    auto& counters = TextureCounts();
    const auto address = resource.baseAddress;
    const auto bytes = static_cast<std::size_t>(guestBytes);
    // A storage image whose results for this surface (or for a mip chain containing it) are still on
    // the GPU supplies the texture by a view of it; anything else needs those results in guest
    // memory first.
    auto source = StorageTexture::FindPending(address, guestBytes);
    if (source != nullptr && !Texture::CanCopyFrom(*source, resource)) source.reset();
    // Otherwise a surface in host-imported memory is viewed through its cached storage image (made
    // here when there is none): its refresh after a CPU or GPU write is a GPU-direct detile from the
    // import, recorded behind the producer, so no bytes are read or compared on the CPU and nothing
    // waits for the producer. A fast-cleared surface (keys) stays a snapshot: its texels are not
    // read either, and the image's own descriptor may not carry the DCC address.
    std::optional<DccKeys> keys;
    if (source == nullptr && SampledFromStorageEligible(context, resource, guestBytes)) {
        keys = TextureClearKeys(resource, guestBytes);
        if (*keys == DccKeys::Uncompressed) source = sampledStorageSource(context, resource, guestBytes);
    }
    // Pages of the surface written since they were last collected are stamped now, before any
    // UnchangedSince (here and on a cache hit) looks at them: the checks only compare stamped blocks,
    // so a write the game made since the last walk is invisible until a collect stamps its page. The
    // walk is memoized per packet, so a texture viewed from a storage image repeats it for free.
    auto generation = GuestMemory::CollectWrites(address, bytes);
    if (source != nullptr && !GuestMemory::UnchangedSince(address, bytes, source->Generation())) {
        // The CPU wrote the memory while results were pending: Refresh merges them the usual way.
        source->Refresh();
    }
    // Results stored to guest memory just now are stamped newer than `generation`; a snapshot read
    // after them is current at the generation of a second (memoized) collect. The store marks the
    // surface's DCC keys uncompressed, so the keys are read after it.
    if (source == nullptr && StorageTexture::FlushPending(address, bytes, nullptr, "sampled texture")) {
        generation = GuestMemory::CollectWrites(address, bytes);
        keys.reset();
    }
    if (!keys.has_value()) keys = TextureClearKeys(resource, guestBytes);
    if (disabled) {
        if (source != nullptr) return std::make_shared<Texture>(context, source, resource, components);
        std::vector<std::byte> snapshot(bytes);
        ReadTextureSurface(resource, *keys, snapshot);
        return std::make_shared<Texture>(context, *context.detiler, resource, components, snapshot);
    }
    auto& cache = Textures();
    const auto key = MakeTextureKey(context.device, words, components);
    std::lock_guard lock(cache.mutex);
    if (auto it = findTexture(cache, key); it != cache.entries.end()) {
        if (it->source != nullptr) {
            // The view follows the storage image, whatever the GPU wrote to it since; guest memory
            // written meanwhile is taken in by refreshing the image. A fast clear the image cannot
            // see (keys, and no pending results to prefer) ends the view: a snapshot holds the clear.
            if ((source == nullptr || source == it->source) && (source != nullptr || *keys == DccKeys::Uncompressed)) {
                if (!GuestMemory::UnchangedSince(address, bytes, it->source->Generation())) it->source->Refresh();
                touchTexture(cache, it);
                logLookup({it->texture.get(), resource, guestBytes, *keys, 0, it->source.get()});
                reportTextureCounters();
                return it->texture;
            }
        } else if (source == nullptr && it->bytes.size() == guestBytes && it->keys == *keys) {
            // Unwritten pages need no comparison; partially resident textures compare only committed
            // pages. The compare goes through the flush hook: it waits for recorded work over the
            // surface (counted, and named for the [hooksync] line).
            const auto equalsCommitted = [&] {
                if (Recorder::SnapshotWriteOverlaps(address, bytes)) counters.pendingReads.fetch_add(1, std::memory_order_relaxed);
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureCompare);
                return GuestMemory::EqualsCommitted(address, it->bytes);
            };
            if (*keys != DccKeys::Uncompressed || GuestMemory::UnchangedSince(address, it->bytes.size(), it->generation) || equalsCommitted()) {
                it->generation = generation;
                touchTexture(cache, it);
                logLookup({it->texture.get(), resource, guestBytes, *keys, generation, nullptr});
                reportTextureCounters();
                return it->texture;
            }
        }
        eraseTexture(cache, it);
    }
    CachedTexture entry{key, address, std::vector<std::byte>(source != nullptr ? 0u : bytes), nullptr, *keys, generation};
    entry.accounted = guestBytes;
    if (source != nullptr) {
        entry.source = source;
        entry.sourceVersion = source->Version();
        entry.texture = std::make_shared<Texture>(context, source, resource, components);
        counters.fromStorage.fetch_add(1, std::memory_order_relaxed);
    } else {
        // Snapshot before the upload so a write racing with it is caught by the next comparison.
        if (*keys == DccKeys::Uncompressed && Recorder::SnapshotWriteOverlaps(address, bytes)) counters.pendingReads.fetch_add(1, std::memory_order_relaxed);
        ReadTextureSurface(resource, *keys, entry.bytes);
        static const bool traceTextures = std::getenv("APS5_TRACE_TEXTURES") != nullptr;
        if (traceTextures) {
            std::size_t nonzero = 0;
            for (std::size_t i = 0; i < entry.bytes.size(); i += 64) nonzero += entry.bytes[i] != std::byte{0};
            std::fprintf(stderr, "[texture] 0x%llx %ux%u format %u tile %d: %zu of %zu sampled bytes nonzero\n", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, static_cast<int>(resource.tileMode), nonzero, entry.bytes.size() / 64);
        }
        entry.texture = std::make_shared<Texture>(context, *context.detiler, resource, components, entry.bytes);
        counters.snapshots.fetch_add(1, std::memory_order_relaxed);
    }
    constexpr std::uint64_t budget = 2048ull << 20u;
    while (!cache.entries.empty() && cache.bytes + entry.accounted > budget) eraseTexture(cache, std::prev(cache.entries.end()));
    cache.bytes += entry.accounted;
    auto texture = entry.texture;
    logLookup({texture.get(), resource, guestBytes, *keys, generation, source.get()});
    cache.entries.push_front(std::move(entry));
    cache.index[key] = cache.entries.begin();
    reportTextureCounters();
    return texture;
}

// Storage images stay on the GPU between dispatches: while guest memory still holds what an image was
// last uploaded from or written back as, its next use skips the upload and detile.
struct StorageKey {
    VkDevice device;
    std::array<std::uint32_t, 8> words;
    bool operator==(const StorageKey&) const = default;
};

struct StorageKeyHash {
    std::size_t operator()(const StorageKey& key) const noexcept {
        return static_cast<std::size_t>(hashWords(14695981039346656037ull ^ reinterpret_cast<std::uintptr_t>(key.device), key.words.data(), key.words.size()));
    }
};

struct CachedStorageTexture {
    StorageKey key;
    std::uint32_t mip;
    std::shared_ptr<StorageTexture> texture;
};

// As TextureCache: use order with a hash index by key, plus one by image for StorageImageCached.
struct StorageTextureCache {
    std::mutex mutex;
    std::list<CachedStorageTexture> entries;
    std::unordered_map<StorageKey, std::list<CachedStorageTexture>::iterator, StorageKeyHash> index;
    std::unordered_map<const StorageTexture*, std::list<CachedStorageTexture>::iterator> byImage;
    std::uint64_t bytes = 0;
};

StorageTextureCache& StorageTextures() {
    static StorageTextureCache cache;
    return cache;
}

std::list<CachedStorageTexture>::iterator findStorage(StorageTextureCache& cache, const StorageKey& key) {
    if (TextureHashEnabled()) {
        const auto found = cache.index.find(key);
        return found == cache.index.end() ? cache.entries.end() : found->second;
    }
    for (auto it = cache.entries.begin(); it != cache.entries.end(); ++it) {
        if (it->key == key) return it;
    }
    return cache.entries.end();
}

std::list<CachedStorageTexture>::iterator findStorageByImage(StorageTextureCache& cache, VkDevice device, const StorageTexture* image) {
    if (TextureHashEnabled()) {
        const auto found = cache.byImage.find(image);
        return found == cache.byImage.end() || found->second->key.device != device ? cache.entries.end() : found->second;
    }
    for (auto it = cache.entries.begin(); it != cache.entries.end(); ++it) {
        if (it->key.device == device && it->texture.get() == image) return it;
    }
    return cache.entries.end();
}

// Evicts an entry: its pending results go to guest memory first (the image may die with the entry).
void evictStorage(StorageTextureCache& cache, std::list<CachedStorageTexture>::iterator it) {
    it->texture->Flush();
    cache.bytes -= it->texture->GuestBytes();
    cache.index.erase(it->key);
    cache.byImage.erase(it->texture.get());
    cache.entries.erase(it);
}

// Storage images are shared by every descriptor of one surface (address, extent, layers, format, tile
// mode): the image holds the whole mip chain, and render targets in the same memory attach to it.
std::array<std::uint32_t, 8> SurfaceKey(const Context& context, const GuestTextureResource& resource) {
    // Guest formats that store in the same Vulkan format share the image (views carry the difference).
    return {static_cast<std::uint32_t>(resource.baseAddress), static_cast<std::uint32_t>(resource.baseAddress >> 32u), resource.width, resource.height, (resource.depthOrLastArray << 16u) | (resource.mipCount & 0xffffu), (static_cast<std::uint32_t>(resource.tileMode) << 12u) | (static_cast<std::uint32_t>(resource.dimension) << 20u), resource.baseArray, static_cast<std::uint32_t>(StorageFormatForGuest(context, resource.format))};
}

// `guestBytes` is the surface size when the caller described the surface already (0: described here).
std::shared_ptr<StorageTexture> cachedStorageTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, std::uint32_t mip, std::uint64_t guestBytes) {
    static const bool disabled = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    if (disabled) return std::make_shared<StorageTexture>(context, *context.detiler, resource, mip);
    static_cast<void>(words);
    auto& counters = TextureCounts();
    const StorageKey key{context.device, SurfaceKey(context, resource)};
    auto& cache = StorageTextures();
    std::lock_guard lock(cache.mutex);
    if (auto it = findStorage(cache, key); it != cache.entries.end()) {
        it->texture->Refresh();
        cache.entries.splice(cache.entries.begin(), cache.entries, it);
        counters.storageHits.fetch_add(1, std::memory_order_relaxed);
        return it->texture;
    }
    // Results other images hold over this memory reach it before the new image reads it: a hit's
    // Refresh flushes them, the constructor's upload does not, and a GPU-direct upload reads the
    // import buffer without the flush hook. The flush is recorded ahead of the upload in the batch.
    if (guestBytes == 0) guestBytes = DescribeSurface(resource).guestBytes;
    StorageTexture::FlushPending(resource.baseAddress, static_cast<std::size_t>(guestBytes), nullptr, "storage image creation");
    CachedStorageTexture entry{key, mip, std::make_shared<StorageTexture>(context, *context.detiler, resource, mip)};
    constexpr std::uint64_t budget = 2048ull << 20u;
    while (!cache.entries.empty() && cache.bytes + entry.texture->GuestBytes() > budget) evictStorage(cache, std::prev(cache.entries.end()));
    cache.bytes += entry.texture->GuestBytes();
    auto texture = entry.texture;
    cache.entries.push_front(std::move(entry));
    cache.index[key] = cache.entries.begin();
    cache.byImage[texture.get()] = cache.entries.begin();
    counters.storageCreated.fetch_add(1, std::memory_order_relaxed);
    return texture;
}

// Whether `image` is still the storage cache's image of its surface, i.e. what a lookup would return
// (an evicted image next to a newer one of the same memory must not be reused: two images of one
// surface would hold results). A use through the fast Revalidate counts as a use for the cache's
// eviction order, as a lookup would.
bool StorageImageCached(const Context& context, const StorageTexture* image) {
    auto& cache = StorageTextures();
    std::lock_guard lock(cache.mutex);
    const auto it = findStorageByImage(cache, context.device, image);
    if (it == cache.entries.end()) return false;
    cache.entries.splice(cache.entries.begin(), cache.entries, it);
    return true;
}

}

std::shared_ptr<StorageTexture> CachedStorageSurface(const Context& context, const GuestTextureResource& resource) {
    return cachedStorageTexture(context, {}, resource, 0);
}

namespace {

const char* roleName(ShaderRecompiler::DescriptorRole role) {
    switch (role) {
        case ShaderRecompiler::DescriptorRole::GuestBuffers: return "GuestBuffers";
        case ShaderRecompiler::DescriptorRole::GuestImages: return "GuestImages";
        case ShaderRecompiler::DescriptorRole::GuestSamplers: return "GuestSamplers";
        case ShaderRecompiler::DescriptorRole::Gds: return "Gds";
        case ShaderRecompiler::DescriptorRole::BdaPagetable: return "BdaPagetable";
        case ShaderRecompiler::DescriptorRole::FaultBuffer: return "FaultBuffer";
        case ShaderRecompiler::DescriptorRole::FlattenedSrt: return "FlattenedSrt";
        case ShaderRecompiler::DescriptorRole::ShaderData: return "ShaderData";
    }
    throw std::runtime_error("AGC graphics: unknown descriptor role");
}

const char* kindName(ShaderRecompiler::DescriptorKind kind) {
    switch (kind) {
        case ShaderRecompiler::DescriptorKind::UniformBuffer: return "UniformBuffer";
        case ShaderRecompiler::DescriptorKind::StorageBuffer: return "StorageBuffer";
        case ShaderRecompiler::DescriptorKind::UniformTexelBuffer: return "UniformTexelBuffer";
        case ShaderRecompiler::DescriptorKind::StorageTexelBuffer: return "StorageTexelBuffer";
        case ShaderRecompiler::DescriptorKind::SampledImage: return "SampledImage";
        case ShaderRecompiler::DescriptorKind::StorageImage: return "StorageImage";
        case ShaderRecompiler::DescriptorKind::Sampler: return "Sampler";
    }
    throw std::runtime_error("AGC graphics: unknown descriptor kind");
}

// Consecutive identical storage descriptors address successive mips of one texture (dynamic-mip
// storage writes), so the second and later ones share the first one's image lookup.
// APS5_NO_STORAGE_DEDUPE=1 looks each one up (and refreshes the image) separately as before.
bool StorageDedupeEnabled() {
    static const bool disabled = std::getenv("APS5_NO_STORAGE_DEDUPE") != nullptr;
    return !disabled;
}

// Whether storage element `element` of `binding` repeats the previous element's eight words.
bool SameAsPreviousStorageElement(const ShaderRecompiler::DescriptorBinding& binding, std::uint32_t element) {
    if (element == 0) return false;
    const auto words = binding.guestDescriptor.begin() + static_cast<std::size_t>(element) * 8u;
    return std::equal(words, words + 8, words - 8);
}

}

ShaderResources::ShaderResources(const Context& context, const ShaderRecompiler::RecompileResult& vertex, const ShaderRecompiler::RecompileResult& fragment, const ColorTarget& target, std::uint64_t indexAddress, std::size_t indexBytes) : ShaderResources(context, std::array<CompiledShader, 2>{{{ShaderRecompiler::ShaderStage::Vertex, &vertex, 0}, {ShaderRecompiler::ShaderStage::Fragment, &fragment, static_cast<std::uint32_t>(vertex.pushConstants.size())}}}, target, indexAddress, indexBytes) {}

ShaderResources::ShaderResources(const Context& context, std::span<const CompiledShader> shaders, const ColorTarget& target, std::uint64_t indexAddress, std::size_t indexBytes, std::span<const GuestMemorySnapshot> snapshots) : context(context), guestMemory(context) {
    prepareAddressBindings(shaders, snapshots);
    build(shaders, &target, indexAddress, indexBytes);
}

ShaderResources::ShaderResources(const Context& context, const CompiledShader& compute, std::span<const GuestMemorySnapshot> snapshots) : ShaderResources(context, compute, snapshots, false) {}

namespace {

// Whether a compute stage maps registered guest memory through BDA tables (see prepareAddressBindings).
bool UsesAddressTables(const CompiledShader& compute) {
    Require(compute.program != nullptr, "missing compiled shader");
    return std::any_of(compute.program->bindings.begin(), compute.program->bindings.end(), [](const auto& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; });
}

}

ShaderResources::ShaderResources(const Context& context, const CompiledShader& compute, std::span<const GuestMemorySnapshot> snapshots, bool deferred) : context(context), guestMemory(context), deferredCompute(compute), deferredSnapshots(snapshots) {
    Require(compute.stage == ShaderRecompiler::ShaderStage::Compute, "compute resources require a compute shader");
    const std::span<const CompiledShader> shaders(&deferredCompute, 1);
    if (!deferred) {
        prepareAddressBindings(shaders, snapshots);
        build(shaders, nullptr, 0, 0);
        forgetDeferredInputs();
        return;
    }
    // Address-based shaders pin and mirror registered memory in prepareAddressBindings (reconciling
    // imports retires buffers to the recorder, refreshing mirrors waits for recorded work): that is
    // device-lock work, so their whole build waits for Complete(). Without tables the call only
    // checks the bindings.
    lockedBuild = UsesAddressTables(compute);
    if (lockedBuild) return;
    unlockedPrepare = true;
    prepareAddressBindings(shaders, snapshots);
    buildPrepare(shaders, nullptr, 0, 0);
}

void ShaderResources::Complete() {
    Require(!completed, "shader resources were already completed");
    const std::span<const CompiledShader> shaders(&deferredCompute, 1);
    if (lockedBuild) {
        prepareAddressBindings(shaders, deferredSnapshots);
        buildPrepare(shaders, nullptr, 0, 0);
    }
    buildComplete();
    forgetDeferredInputs();
}

void ShaderResources::forgetDeferredInputs() {
    // The compiled shader and the captured regions belong to the dispatch, which returns while this
    // object lives on in the resource cache and the recorder: nothing may reach for them after the
    // build, so they are dropped rather than left dangling.
    deferredCompute = {};
    deferredSnapshots = {};
}

void ShaderResources::build(std::span<const CompiledShader> shaders, const ColorTarget* target, std::uint64_t indexAddress, std::size_t indexBytes) {
    buildPrepare(shaders, target, indexAddress, indexBytes);
    buildComplete();
}

namespace {

// APS5_PROFILE_DRAW: the [resources] phase totals, under a mutex because stage A of several builds
// runs at once.
struct BuildProfile {
    std::mutex mutex;
    std::map<std::string, double> phaseTotals;
    std::uint64_t builds = 0;
};

BuildProfile& Builds() {
    static BuildProfile profile;
    return profile;
}

bool BuildProfiled() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

void addBuildPhase(const char* name, double ms) {
    auto& profile = Builds();
    std::lock_guard lock(profile.mutex);
    profile.phaseTotals[name] += ms;
}

// APS5_PROFILE_DRAW: guest buffer elements bound by descriptor (addGuestBuffer) and how many the
// recompiler proved read-only, plus the pending-write notes their uses skipped (MarkGpuWrites),
// printed as [buffers] every 10 s from MarkGpuWrites. Cumulative.
struct BufferWriteCounters {
    std::atomic<std::uint64_t> elements{0};
    std::atomic<std::uint64_t> readOnly{0};
    std::atomic<std::uint64_t> notesSkipped{0};
    std::atomic<std::int64_t> lastReport{0};
};

BufferWriteCounters& BufferWrites() {
    static BufferWriteCounters counters;
    return counters;
}

}

double ShaderResources::phase(const char* name) {
    if (!BuildProfiled()) return 0.0;
    const auto now = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration<double, std::milli>(now - phaseStart).count();
    addBuildPhase(name, ms);
    if (ms > 50) std::fprintf(stderr, "[resources] %s took %.0f ms (%zu textures, %zu storage images, %zu buffers, bda %d)\n", name, ms, textures.size(), storageTextures.size(), allocations.size(), usesBda ? 1 : 0);
    phaseStart = now;
    return ms;
}

void ShaderResources::buildPrepare(std::span<const CompiledShader> shaders, const ColorTarget* target, std::uint64_t indexAddress, std::size_t indexBytes) {
    const auto stageStart = std::chrono::steady_clock::now();
    phaseStart = stageStart;
    if (BuildProfiled()) {
        auto& profile = Builds();
        std::lock_guard lock(profile.mutex);
        if (++profile.builds % 1000 == 0) {
            std::string report;
            for (const auto& [name, ms] : profile.phaseTotals) report += " " + name + "=" + std::to_string(static_cast<long long>(ms)) + "ms";
            std::fprintf(stderr, "[resources] %llu builds, phase totals:%s\n", static_cast<unsigned long long>(profile.builds), report.c_str());
        }
    }
    try {
        Require(!shaders.empty() && context.limits.maxBoundDescriptorSets >= 1, "shader descriptor set exceeds device limits");
        std::set<std::uint32_t> occupied;
        for (const auto& shader : shaders) {
            Require(shader.program != nullptr, "missing compiled shader");
            const VkShaderStageFlags flags = VulkanStage(shader.stage);
            std::uint64_t stageDescriptors = 0;
            for (const auto& binding : shader.program->bindings) {
                Require(binding.descriptorSet == 0, "unexpected descriptor set: every shader resource must use descriptor set zero");
                Require(occupied.insert(binding.binding).second, "duplicate shader binding");
                const bool addressRole = binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable || binding.role == ShaderRecompiler::DescriptorRole::FaultBuffer;
                const bool bufferRole = addressRole || binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers || binding.role == ShaderRecompiler::DescriptorRole::ShaderData || binding.role == ShaderRecompiler::DescriptorRole::FlattenedSrt;
                const bool imageRole = binding.role == ShaderRecompiler::DescriptorRole::GuestImages || binding.role == ShaderRecompiler::DescriptorRole::GuestSamplers;
                if (imageRole) {
                    addImageBinding(binding, flags);
                    continue;
                }
                Require(bufferRole, std::string("unsupported descriptor role ") + roleName(binding.role));
                Require(binding.kind == ShaderRecompiler::DescriptorKind::StorageBuffer, std::string("unsupported descriptor kind ") + kindName(binding.kind) + " for role " + roleName(binding.role) + ": only StorageBuffer is supported");
                Require(!binding.readOnly, "read-only descriptors are unsupported because the recompiler emits no NonWritable decoration");
                Require(binding.count != 0, "empty descriptor binding");
                stageDescriptors += binding.count;
                storageBuffers += binding.count;
                Require(stageDescriptors <= context.limits.maxPerStageDescriptorStorageBuffers && stageDescriptors <= context.limits.maxPerStageResources, "shader descriptors exceed per-stage limits");
                Binding item{{binding.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, binding.count, flags, nullptr}, {}};
                if (binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers) {
                    Require(binding.guestDescriptor.size() == static_cast<std::uint64_t>(binding.count) * 4, "guest buffer descriptor must contain four DWORDs per array element");
                    for (std::uint32_t element = 0; element < binding.count; ++element) {
                        // An element the recompiler did not classify (a producer without the
                        // vector) counts as written, like imageWritten below.
                        const bool written = element >= binding.bufferWritten.size() || binding.bufferWritten[element];
                        item.allocations.push_back(addGuestBuffer(std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 4, 4), target, indexAddress, indexBytes, written));
                    }
                } else if (addressRole) {
                    item.allocations.push_back(allocations.size());
                    allocations.push_back({0, 0, false, nullptr, binding.role});
                } else {
                    Require(binding.count == 1, "shader data and flattened SRT descriptors must not be arrays");
                    Require(!binding.guestDescriptor.empty(), "empty shader data descriptor");
                    item.allocations.push_back(addDataBuffer(binding.guestDescriptor));
                }
                bindings.push_back(std::move(item));
            }
        }
        Require(storageBuffers <= context.limits.maxDescriptorSetStorageBuffers, "pipeline descriptors exceed device limits");
        timing.bindingsMs = phase("bindings");
        // For every build, locked ones included: their stage B then takes the fast path too, and the
        // collects cost the same wherever they run.
        if (precollectImages()) phase("precollect");
        guestMemory.UploadPrepare(usesBda);
        timing.uploadMs = phase("guest memory upload");
        std::vector<VkDescriptorSetLayoutBinding> description;
        for (const auto& binding : bindings) {
            description.push_back(binding.layout);
            layoutKey.insert(layoutKey.end(), {binding.layout.binding, static_cast<std::uint32_t>(binding.layout.descriptorType), binding.layout.descriptorCount, binding.layout.stageFlags});
        }
        static const bool noLayoutCache = std::getenv("APS5_NO_LAYOUT_CACHE") != nullptr;
        static const bool noPoolCache = std::getenv("APS5_NO_POOL_CACHE") != nullptr;
        if (context.descriptorCache != nullptr && !noLayoutCache) {
            _layout = context.descriptorCache->Layout(layoutKey, description);
        } else {
            VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            info.bindingCount = static_cast<std::uint32_t>(description.size());
            info.pBindings = description.data();
            Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &info, nullptr, &_layout), "vkCreateDescriptorSetLayout");
            ownsLayout = true;
        }
        if (!bindings.empty()) {
            // The set is sized from the plan: every image element becomes exactly one descriptor
            // when stage B looks it up.
            std::vector<VkDescriptorPoolSize> sizes;
            if (storageBuffers != 0) sizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<std::uint32_t>(storageBuffers)});
            if (plannedSampledImages != 0) sizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, plannedSampledImages});
            if (plannedStorageImages != 0) sizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, plannedStorageImages});
            if (!samplers.empty()) sizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, static_cast<std::uint32_t>(samplers.size())});
            if (context.descriptorCache != nullptr && !noPoolCache) {
                const auto allocated = context.descriptorCache->Allocate(_layout, sizes);
                _set = allocated.set;
                cachePool = allocated.pool;
            }
            if (_set == VK_NULL_HANDLE) {
                VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
                poolInfo.maxSets = 1;
                poolInfo.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
                poolInfo.pPoolSizes = sizes.data();
                Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
                VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                allocation.descriptorPool = pool;
                allocation.descriptorSetCount = 1;
                allocation.pSetLayouts = &_layout;
                Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocation, &_set), "vkAllocateDescriptorSets");
            }
        }
        timing.descriptorsMs = phase("descriptors");
        if (BuildProfiled()) {
            timing.prepareMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - stageStart).count();
            addBuildPhase("stage A", timing.prepareMs);
        }
    } catch (...) {
        release();
        throw;
    }
}

void ShaderResources::buildComplete() {
    const auto stageStart = std::chrono::steady_clock::now();
    phaseStart = stageStart;
    try {
        // The lookups run in plan order: Revalidate walks the bindings the same way, and consecutive
        // storage elements of one mip chain share the previous element's image.
        for (const auto& deferred : deferredImages) resolveImageBinding(*deferred.binding, bindings[deferred.index]);
        deferredImages.clear();
        // The records served their purpose: each holds the cache entry's objects as of stage A,
        // which would otherwise keep a replaced texture or an evicted storage image (and its device
        // memory) alive, outside the caches' budgets, for as long as this object is cached.
        imageRecords.clear();
        imageRecords.shrink_to_fit();
        nextImageRecord = 0;
        Require(textures.size() == plannedSampledImages && storageTextures.size() == plannedStorageImages, "image lookups disagree with the descriptor plan");
        timing.bindingsMs += phase("images");
        guestMemory.UploadFinish(usesBda);
        timing.uploadMs += phase("guest memory upload");
        if (usesBda) bda = std::make_unique<BdaResources>(context, guestMemory);
        else if (usesFaultBuffer) bda = std::make_unique<BdaResources>(context);
        phase("bda");
        if (_set != VK_NULL_HANDLE) {
            // One update call for the whole set: the info arrays are sized up front so every write's
            // pointer into them stays valid until the call.
            std::size_t bufferCount = 0;
            std::size_t imageCount = 0;
            for (const auto& binding : bindings) {
                bufferCount += binding.allocations.size();
                imageCount += binding.imageAllocations.size();
            }
            std::vector<VkDescriptorBufferInfo> buffers;
            std::vector<VkDescriptorImageInfo> images;
            buffers.reserve(bufferCount);
            images.reserve(imageCount);
            std::vector<VkWriteDescriptorSet> writes;
            writes.reserve(bindings.size());
            for (const auto& binding : bindings) {
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = _set;
                write.dstBinding = binding.layout.binding;
                write.descriptorCount = binding.layout.descriptorCount;
                write.descriptorType = binding.layout.descriptorType;
                switch (binding.layout.descriptorType) {
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                        write.pBufferInfo = buffers.data() + buffers.size();
                        for (const auto index : binding.allocations) buffers.push_back(descriptor(allocations[index]));
                        break;
                    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                        write.pImageInfo = images.data() + images.size();
                        for (const auto index : binding.imageAllocations) images.push_back({VK_NULL_HANDLE, textures[index]->View(), textures[index]->Layout()});
                        break;
                    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                        write.pImageInfo = images.data() + images.size();
                        for (const auto index : binding.imageAllocations) images.push_back({VK_NULL_HANDLE, storageTextures[index]->View(storageMips[index]), VK_IMAGE_LAYOUT_GENERAL});
                        break;
                    case VK_DESCRIPTOR_TYPE_SAMPLER:
                        write.pImageInfo = images.data() + images.size();
                        for (const auto index : binding.imageAllocations) images.push_back({samplers[index]->Handle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});
                        break;
                    default: throw std::runtime_error("AGC graphics: ShaderResources encountered an unknown descriptor type while writing the descriptor set");
                }
                writes.push_back(write);
            }
            context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
        timing.descriptorsMs += phase("descriptors");
        noteReusable();
        completed = true;
        if (BuildProfiled()) {
            timing.completeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - stageStart).count();
            addBuildPhase("stage B", timing.completeMs);
            reportDescriptorCaches();
        }
    } catch (...) {
        release();
        throw;
    }
}

// Whether this build can serve later dispatches of the same content (see ContentKey): nothing to do
// once the GPU completed (no BDA, no copied written buffers, no lease) and every guest buffer bound
// in place through a host import whose identity is recorded for Revalidate.
void ShaderResources::noteReusable() {
    // Taken whether or not the object turns out reusable: the lookups' records are this build's.
    captureValidation();
    reusable = false;
    directRegions.clear();
    if (NeedsCompletion() || HoldsLease()) return;
    const auto regions = guestMemory.DirectRegions();
    if (!regions.has_value()) return;
    for (const auto& [begin, end] : *regions) {
        // No reconcile here: the upload just took these imports, and the set is about to be recorded
        // against them.
        auto serial = HostImportSerial(context, begin, static_cast<std::size_t>(end - begin), false);
        // Read-only image mirrors (exe ranges) are as stable as imports; their serials have the top
        // bit set, so the two spaces never collide.
        if (serial == 0) serial = ImageMirrorSerial(context, begin, static_cast<std::size_t>(end - begin));
        if (serial == 0) return;
        directRegions.push_back({begin, end, serial});
    }
    reusable = true;
}

// APS5_PROFILE_DRAW: the per-device descriptor caches' counters, every 10 s.
void ShaderResources::reportDescriptorCaches() const {
    static std::mutex reportMutex;
    static auto lastReport = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(reportMutex);
    if (now - lastReport < std::chrono::seconds(10)) return;
    lastReport = now;
    const auto descriptors = context.descriptorCache != nullptr ? context.descriptorCache->Counters() : DescriptorCache::Stats{};
    const auto samplerHits = context.samplerCache != nullptr ? context.samplerCache->Hits() : 0;
    const auto samplerMisses = context.samplerCache != nullptr ? context.samplerCache->Misses() : 0;
    std::fprintf(stderr, "[descriptors] layouts %llu hits / %llu created, sets %llu from %llu pools, samplers %llu hits / %llu created\n", static_cast<unsigned long long>(descriptors.layoutHits), static_cast<unsigned long long>(descriptors.layoutMisses), static_cast<unsigned long long>(descriptors.sets), static_cast<unsigned long long>(descriptors.pools), static_cast<unsigned long long>(samplerHits), static_cast<unsigned long long>(samplerMisses));
}

std::vector<std::uint32_t> ShaderResources::ContentKey(const CompiledShader& shader) {
    Require(shader.program != nullptr, "missing compiled shader");
    const auto& program = *shader.program;
    std::vector<std::uint32_t> key;
    key.reserve(8 + program.bindings.size() * 12);
    key.push_back(static_cast<std::uint32_t>(shader.stage));
    key.push_back(static_cast<std::uint32_t>(program.variantId));
    key.push_back(static_cast<std::uint32_t>(program.variantId >> 32u));
    key.push_back(static_cast<std::uint32_t>(program.bindings.size()));
    const auto packBits = [&](const std::vector<bool>& bits) {
        key.push_back(static_cast<std::uint32_t>(bits.size()));
        std::uint32_t word = 0;
        for (std::size_t i = 0; i < bits.size(); ++i) {
            if (bits[i]) word |= 1u << (i % 32u);
            if (i % 32u == 31u || i + 1 == bits.size()) {
                key.push_back(word);
                word = 0;
            }
        }
    };
    for (const auto& binding : program.bindings) {
        key.insert(key.end(), {static_cast<std::uint32_t>(binding.kind), static_cast<std::uint32_t>(binding.role), binding.descriptorSet, binding.binding, binding.count, binding.readOnly ? 1u : 0u, binding.imageShape.has_value() ? static_cast<std::uint32_t>(*binding.imageShape) + 1u : 0u, static_cast<std::uint32_t>(binding.guestDescriptor.size())});
        key.insert(key.end(), binding.guestDescriptor.begin(), binding.guestDescriptor.end());
        packBits(binding.imageWritten);
        packBits(binding.samplerDepthCompare);
        // Read-only elements are bound without a write set: an object built for one written set
        // must not serve a build with another (the variant implies it, this makes it explicit).
        packBits(binding.bufferWritten);
    }
    return key;
}

namespace {

// APS5_PROFILE_DRAW: Revalidate outcomes and time, printed as [rescache] every 10 s next to the
// device's hit/miss line: failed = the object could not be reused (whichever image path it took);
// of the reused ones, fast = every image proved current from stamps, full = the lookups were repeated.
struct RevalidateProfile {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> nanoseconds{0};
    std::atomic<std::uint64_t> fast{0};
    std::atomic<std::uint64_t> full{0};
    std::atomic<std::uint64_t> failed{0};
    std::atomic<std::int64_t> lastReport{0};
};

RevalidateProfile& Revalidations() {
    static RevalidateProfile profile;
    return profile;
}

void countRevalidate(bool fast, bool ok, std::chrono::steady_clock::time_point start) {
    auto& profile = Revalidations();
    const auto now = std::chrono::steady_clock::now();
    profile.calls.fetch_add(1, std::memory_order_relaxed);
    profile.nanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count()), std::memory_order_relaxed);
    (!ok ? profile.failed : fast ? profile.fast : profile.full).fetch_add(1, std::memory_order_relaxed);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto last = profile.lastReport.load();
    if (nowMs - last < 10000 || !profile.lastReport.compare_exchange_strong(last, nowMs)) return;
    std::fprintf(stderr, "[rescache] revalidate %llu calls %.1f ms: fast %llu, full %llu, failed %llu\n", static_cast<unsigned long long>(profile.calls.load()), profile.nanoseconds.load() / 1e6, static_cast<unsigned long long>(profile.fast.load()), static_cast<unsigned long long>(profile.full.load()), static_cast<unsigned long long>(profile.failed.load()));
}

}

// Turns the lookups' records into this object's per-texture validation records (see
// ValidatedSurface); the last record of an object is the freshest.
void ShaderResources::captureValidation() {
    const auto record = [](const void* object) -> const LookupRecord* {
        for (auto it = lookupLog.rbegin(); it != lookupLog.rend(); ++it) {
            if (it->object == object) return &*it;
        }
        return nullptr;
    };
    validatedTextures.assign(textures.size(), {});
    for (std::size_t i = 0; i < textures.size(); ++i) {
        if (const auto* found = record(textures[i].get())) validatedTextures[i] = {found->resource, found->bytes, found->keys, found->generation, 0, found->source, true};
    }
    lookupLog.clear();
}

// Proves every texture and storage image still current without repeating its lookup: exactly the
// "unchanged" branches of cachedTexture and StorageTexture::Refresh, evaluated from write stamps,
// the pending-results registry and the DCC keys. Per element: collect first (stamps come only from
// collects, and the generation a record moves to must predate the checks, so a write landing between
// them is stamped newer: the rule of Driver's dispatch cache), then no other image may have results
// pending over the memory (a lookup would flush them into it), a storage image must still be the
// cache's image of its surface, the memory must be unchanged since the object's content matched it,
// and the DCC keys must be what the content was made under whenever the surface has any (a fast
// clear touches only the keys). Anything else, including a failed collect (memory outside the arena,
// uncommitted pages), is left to the full walk.
bool ShaderResources::fastRevalidate() {
    if (validatedTextures.size() != textures.size()) return false;
    for (std::size_t i = 0; i < textures.size(); ++i) {
        auto& surface = validatedTextures[i];
        if (!surface.valid) return false;
        const auto address = surface.resource.baseAddress;
        const auto bytes = static_cast<std::size_t>(surface.bytes);
        surface.collected = GuestMemory::CollectWrites(address, bytes);
        if (surface.collected == 0) return false;
        if (PendingStorageOverlaps(address, bytes, surface.source)) return false;
        if (surface.source != nullptr) {
            // The view follows the image: it needs the image current with guest memory, as the
            // lookup's Refresh would make it.
            if (!StorageImageCached(context, surface.source) || !GuestMemory::UnchangedSince(address, bytes, surface.source->Generation())) return false;
        } else if (!GuestMemory::UnchangedSince(address, bytes, surface.generation)) {
            return false;
        }
        if (surface.resource.dccAddress != 0 && TextureClearKeys(surface.resource, surface.bytes) != surface.keys) return false;
        // A view made under fast-clear keys stays one only while its image still has results pending
        // over the surface (the lookup prefers them to the clear); once they are flushed the clear
        // the image cannot see wins and the lookup makes a snapshot (cachedTexture's hit rule).
        if (surface.source != nullptr && surface.keys != DccKeys::Uncompressed && StorageTexture::FindPending(address, surface.bytes).get() != surface.source) return false;
    }
    for (std::size_t i = 0; i < storageTextures.size(); ++i) {
        // Consecutive elements of one mip chain share the image (see addStorageImageBinding).
        if (i != 0 && storageTextures[i] == storageTextures[i - 1]) continue;
        const auto* image = storageTextures[i].get();
        if (image == nullptr) return false;
        // StorageTexture::Refresh checks the image's own surface, not the element's descriptor (one
        // image serves every descriptor of its memory, with or without a DCC address), and compares
        // its keys with the keys it was uploaded under, which only the image knows (they move to
        // Uncompressed on every write-back; a fast clear after it puts the key bytes back to what a
        // record saw while the image holds last frame's results), so a surface with keys is left to
        // the full walk. Without a DCC address both sides of that compare are Uncompressed whatever
        // happened, and the memory checks below are Refresh's.
        const auto& own = image->Descriptor();
        if (own.dccAddress != 0) return false;
        const auto address = own.baseAddress;
        const auto bytes = static_cast<std::size_t>(image->GuestBytes());
        if (GuestMemory::CollectWrites(address, bytes) == 0) return false;
        if (PendingStorageOverlaps(address, bytes, image)) return false;
        if (!StorageImageCached(context, image)) return false;
        if (!GuestMemory::UnchangedSince(address, bytes, image->Generation())) return false;
    }
    // Every check passed: snapshot textures are current at the collects issued before them. Storage
    // images keep their own generation: nothing was stamped above it, so the blocks a later write
    // stamps are the same set either way.
    for (auto& surface : validatedTextures) {
        if (surface.source == nullptr) surface.generation = surface.collected;
    }
    return true;
}

bool ShaderResources::Revalidate(std::span<const CompiledShader> shaders) {
    if (!reusable || shaders.empty()) return false;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    // APS5_NO_FAST_REVALIDATE=1 always repeats the lookups.
    static const bool noFast = std::getenv("APS5_NO_FAST_REVALIDATE") != nullptr;
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const auto finish = [&](bool fast, bool ok) {
        if (profile) countRevalidate(fast, ok, start);
        return ok;
    };
    // (1) Textures and storage images, from stamps when every element allows it (fastRevalidate),
    // else by the same lookups as the build (which refresh or replace them as guest memory changed):
    // they must hand back the very objects the set's views belong to. The build appended them stage
    // by stage, binding by binding, so the walk repeats that order.
    const bool fast = !noFast && fastRevalidate();
    if (!fast) {
        lookupLog.clear();
        std::size_t textureIndex = 0;
        std::size_t storageIndex = 0;
        for (const auto& shader : shaders) {
            if (shader.program == nullptr) return finish(false, false);
            for (const auto& binding : shader.program->bindings) {
                if (binding.role != ShaderRecompiler::DescriptorRole::GuestImages) continue;
                if (binding.kind == ShaderRecompiler::DescriptorKind::SampledImage) {
                    const auto elementWords = binding.count != 0 ? binding.guestDescriptor.size() / binding.count : 0;
                    for (std::uint32_t element = 0; element < binding.count; ++element) {
                        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
                        const auto resource = DecodeTextureResource(words);
                        const VkComponentMapping components{ComponentSwizzleFor(resource.dstSelX), ComponentSwizzleFor(resource.dstSelY), ComponentSwizzleFor(resource.dstSelZ), ComponentSwizzleFor(resource.dstSelW)};
                        if (textureIndex >= textures.size() || cachedTexture(context, words, resource, components) != textures[textureIndex]) return finish(false, false);
                        ++textureIndex;
                    }
                } else if (binding.kind == ShaderRecompiler::DescriptorKind::StorageImage) {
                    for (std::uint32_t element = 0; element < binding.count; ++element) {
                        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
                        if (storageIndex >= storageTextures.size()) return finish(false, false);
                        std::shared_ptr<StorageTexture> expected;
                        if (SameAsPreviousStorageElement(binding, element) && StorageDedupeEnabled()) expected = storageTextures[storageIndex - 1];
                        else expected = cachedStorageTexture(context, words, DecodeTextureResource(words), storageMips[storageIndex]);
                        if (expected != storageTextures[storageIndex]) return finish(false, false);
                        ++storageIndex;
                    }
                }
            }
        }
        if (textureIndex != textures.size() || storageIndex != storageTextures.size()) return finish(false, false);
        // The walk proved every object current again: the records move to what it proved, or one
        // spurious stamp (a label sharing a 64 KiB block with a surface's edge) would keep this
        // object on the full walk for good.
        captureValidation();
    }
    // (2) The imported buffers the set reads in place: results of storage images pending in them go
    // to guest memory first (as an upload does), then each import must still be the one the set was
    // written against. This comes last because the lookups and flushes above can reconcile imports
    // themselves; nothing else touches them between here and the dispatch being recorded.
    for (const auto& region : directRegions) StorageTexture::FlushPending(region.begin, static_cast<std::size_t>(region.end - region.begin), nullptr, "imported buffer region");
    for (const auto& region : directRegions) {
        auto serial = HostImportSerial(context, region.begin, static_cast<std::size_t>(region.end - region.begin), true);
        if (serial == 0) serial = ImageMirrorSerial(context, region.begin, static_cast<std::size_t>(region.end - region.begin));
        if (serial != region.serial) return finish(fast, false);
    }
    return finish(fast, true);
}

namespace {

// APS5_PROFILE_DRAW: what makes content keys miss. On a miss the key is compared with the last key
// seen for the same shader variant(s) and the first differing word is charged to its binding's role
// (and to its index within the V# or T# element), printed as [rescache] every 10 s: it says whether
// V# bases of ring allocations, SRT words or image descriptors churn.
struct ChurnProfile {
    std::mutex mutex;
    std::unordered_map<std::uint64_t, ResourceCache::Key> lastByVariant;
    std::array<std::uint64_t, 8> roles{};
    std::array<std::uint64_t, 4> bufferWords{};
    std::array<std::uint64_t, 8> imageWords{};
    std::uint64_t firstSeen = 0;
    std::uint64_t sameKey = 0;
    std::uint64_t layout = 0;
    std::uint64_t drawWords = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

ChurnProfile& Churn() {
    static ChurnProfile profile;
    return profile;
}

// Charges word `diff` of `key` to the binding of the ContentKey that starts at `at` (the layout of
// ShaderResources::ContentKey), moving `at` past that key; false when `diff` lies beyond it.
bool chargeShaderKey(ChurnProfile& profile, const ResourceCache::Key& key, std::size_t& at, std::size_t diff) {
    if (at + 4 > key.size() || diff < at + 4) {
        ++profile.layout;
        return true;
    }
    const auto bindings = key[at + 3];
    at += 4;
    for (std::uint32_t binding = 0; binding < bindings; ++binding) {
        if (at + 8 > key.size() || diff < at + 8) {
            ++profile.layout;
            return true;
        }
        const auto kind = key[at];
        const auto role = key[at + 1];
        const auto descriptorWords = key[at + 7];
        at += 8;
        if (diff < at + descriptorWords) {
            if (role < profile.roles.size()) ++profile.roles[role];
            const auto index = diff - at;
            if (role == static_cast<std::uint32_t>(ShaderRecompiler::DescriptorRole::GuestBuffers)) ++profile.bufferWords[index % 4];
            else if (role == static_cast<std::uint32_t>(ShaderRecompiler::DescriptorRole::GuestImages) && (kind == static_cast<std::uint32_t>(ShaderRecompiler::DescriptorKind::SampledImage) || kind == static_cast<std::uint32_t>(ShaderRecompiler::DescriptorKind::StorageImage))) ++profile.imageWords[index % 8];
            return true;
        }
        at += descriptorWords;
        // imageWritten, samplerDepthCompare and bufferWritten: a count word, then one word per 32 bits.
        for (int flags = 0; flags < 3; ++flags) {
            if (at >= key.size()) {
                ++profile.layout;
                return true;
            }
            const auto words = 1 + (static_cast<std::size_t>(key[at]) + 31) / 32;
            if (diff < at + words) {
                ++profile.layout;
                return true;
            }
            at += words;
        }
    }
    return false;
}

}

std::shared_ptr<ShaderResources> ResourceCache::Find(const Key& key) {
    std::lock_guard lock(mutex);
    const auto found = index.find(key);
    if (found == index.end()) {
        noteMiss(key);
        return nullptr;
    }
    entries.splice(entries.begin(), entries, found->second);
    return found->second->second;
}

void ResourceCache::noteMiss(const Key& key) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile || key.size() < 4) return;
    // The variant(s) the key belongs to: a compute key starts with the stage and its variant, a draw
    // key with a marker, the device and its stages' keys (Draw.cpp DrawResourceKey).
    const bool draw = key[0] == 0xffffffffu;
    std::uint64_t variants = 0;
    if (draw) {
        std::size_t at = 4;
        for (std::uint32_t stage = 0; stage < key[3] && at + 3 < key.size(); ++stage) {
            variants = variants * 1000003ull ^ (key[at + 2] | (static_cast<std::uint64_t>(key[at + 3]) << 32u));
            at += 1 + key[at];
        }
    } else {
        variants = key[1] | (static_cast<std::uint64_t>(key[2]) << 32u);
    }
    auto& churn = Churn();
    std::lock_guard lock(churn.mutex);
    const auto found = churn.lastByVariant.find(variants);
    if (found == churn.lastByVariant.end()) {
        ++churn.firstSeen;
        churn.lastByVariant.emplace(variants, key);
    } else {
        const auto& previous = found->second;
        const auto common = std::min(key.size(), previous.size());
        std::size_t diff = 0;
        while (diff < common && key[diff] == previous[diff]) ++diff;
        if (diff == common && key.size() == previous.size()) {
            ++churn.sameKey;
        } else if (draw) {
            std::size_t at = 4;
            bool charged = false;
            for (std::uint32_t stage = 0; stage < key[3] && at < key.size() && !charged; ++stage) {
                ++at;
                charged = chargeShaderKey(churn, key, at, diff);
            }
            if (!charged) ++churn.drawWords;
        } else {
            std::size_t at = 0;
            if (!chargeShaderKey(churn, key, at, diff)) ++churn.layout;
        }
        found->second = key;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - churn.lastReport < std::chrono::seconds(10)) return;
    churn.lastReport = now;
    std::string line = "[rescache] miss churn, first differing word by role:";
    char item[256];
    for (std::size_t role = 0; role < churn.roles.size(); ++role) {
        if (churn.roles[role] == 0) continue;
        std::snprintf(item, sizeof(item), " %s %llu", roleName(static_cast<ShaderRecompiler::DescriptorRole>(role)), static_cast<unsigned long long>(churn.roles[role]));
        line += item;
        if (role == static_cast<std::size_t>(ShaderRecompiler::DescriptorRole::GuestBuffers)) {
            std::snprintf(item, sizeof(item), " (V# word %llu/%llu/%llu/%llu)", static_cast<unsigned long long>(churn.bufferWords[0]), static_cast<unsigned long long>(churn.bufferWords[1]), static_cast<unsigned long long>(churn.bufferWords[2]), static_cast<unsigned long long>(churn.bufferWords[3]));
            line += item;
        } else if (role == static_cast<std::size_t>(ShaderRecompiler::DescriptorRole::GuestImages)) {
            std::snprintf(item, sizeof(item), " (T# word %llu/%llu/%llu/%llu/%llu/%llu/%llu/%llu)", static_cast<unsigned long long>(churn.imageWords[0]), static_cast<unsigned long long>(churn.imageWords[1]), static_cast<unsigned long long>(churn.imageWords[2]), static_cast<unsigned long long>(churn.imageWords[3]), static_cast<unsigned long long>(churn.imageWords[4]), static_cast<unsigned long long>(churn.imageWords[5]), static_cast<unsigned long long>(churn.imageWords[6]), static_cast<unsigned long long>(churn.imageWords[7]));
            line += item;
        }
    }
    std::snprintf(item, sizeof(item), "; layout %llu, draw target/index %llu, same key %llu (not inserted or evicted), first seen %llu", static_cast<unsigned long long>(churn.layout), static_cast<unsigned long long>(churn.drawWords), static_cast<unsigned long long>(churn.sameKey), static_cast<unsigned long long>(churn.firstSeen));
    line += item;
    std::fprintf(stderr, "%s\n", line.c_str());
}

void ResourceCache::Insert(const Key& key, std::shared_ptr<ShaderResources> resources) {
    std::lock_guard lock(mutex);
    erase(key);
    entries.emplace_front(key, std::move(resources));
    index.emplace(key, entries.begin());
    // Entries pin their textures and storage images past the texture caches' budgets, so the bound
    // stays modest: APS5_RESOURCE_CACHE_ENTRIES (default 1024) covers several frames of distinct
    // dispatch and draw content.
    static const std::size_t capacity = [] {
        const char* value = std::getenv("APS5_RESOURCE_CACHE_ENTRIES");
        const auto parsed = value ? std::strtoull(value, nullptr, 10) : 1024ull;
        return static_cast<std::size_t>(parsed != 0 ? parsed : 1024ull);
    }();
    while (entries.size() > capacity) {
        index.erase(entries.back().first);
        entries.pop_back();
    }
}

void ResourceCache::Remove(const Key& key) {
    std::lock_guard lock(mutex);
    erase(key);
}

void ResourceCache::Clear() {
    std::lock_guard lock(mutex);
    index.clear();
    entries.clear();
}

std::size_t ResourceCache::Size() const {
    std::lock_guard lock(mutex);
    return entries.size();
}

void ResourceCache::erase(const Key& key) {
    const auto found = index.find(key);
    if (found == index.end()) return;
    entries.erase(found->second);
    index.erase(found);
}

ResourceCache& SharedResourceCache() {
    static auto* cache = new ResourceCache();
    return *cache;
}

DescriptorCache::DescriptorCache(const Context& context) : context(context), destroyLayout(context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")), destroyPool(context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")), freeSets(context.Function<PFN_vkFreeDescriptorSets>("vkFreeDescriptorSets")) {}

DescriptorCache::~DescriptorCache() {
    for (const auto pool : pools) destroyPool(context.device, pool, nullptr);
    for (const auto& [key, layout] : layouts) destroyLayout(context.device, layout, nullptr);
}

VkDescriptorSetLayout DescriptorCache::Layout(std::span<const std::uint32_t> key, std::span<const VkDescriptorSetLayoutBinding> bindings) {
    std::lock_guard lock(mutex);
    std::vector<std::uint32_t> keyCopy(key.begin(), key.end());
    if (const auto found = layouts.find(keyCopy); found != layouts.end()) {
        ++stats.layoutHits;
        return found->second;
    }
    ++stats.layoutMisses;
    VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.bindingCount = static_cast<std::uint32_t>(bindings.size());
    info.pBindings = bindings.data();
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &info, nullptr, &layout), "vkCreateDescriptorSetLayout");
    layouts.emplace(std::move(keyCopy), layout);
    return layout;
}

namespace {
// What one chain pool holds; a set needing more of any type gets a dedicated pool.
constexpr std::uint32_t ChainPoolSets = 1024;
constexpr std::array<VkDescriptorPoolSize, 4> ChainPoolSizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024}, {VK_DESCRIPTOR_TYPE_SAMPLER, 512}}};
}

DescriptorCache::SetAllocation DescriptorCache::Allocate(VkDescriptorSetLayout layout, std::span<const VkDescriptorPoolSize> sizes) {
    for (const auto& size : sizes) {
        const auto capacity = std::find_if(ChainPoolSizes.begin(), ChainPoolSizes.end(), [&](const auto& item) { return item.type == size.type; });
        if (capacity == ChainPoolSizes.end() || size.descriptorCount > capacity->descriptorCount) return {};
    }
    std::lock_guard lock(mutex);
    const auto allocate = context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets");
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorSetCount = 1;
    allocation.pSetLayouts = &layout;
    // Newest pool first: it has the most room; a full or fragmented pool is left for its sets to
    // drain and tried again later.
    for (auto it = pools.rbegin(); it != pools.rend(); ++it) {
        allocation.descriptorPool = *it;
        VkDescriptorSet set = VK_NULL_HANDLE;
        const auto result = allocate(context.device, &allocation, &set);
        if (result == VK_SUCCESS) {
            ++stats.sets;
            return {set, *it};
        }
        if (result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL) Check(result, "vkAllocateDescriptorSets");
    }
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = ChainPoolSets;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(ChainPoolSizes.size());
    poolInfo.pPoolSizes = ChainPoolSizes.data();
    VkDescriptorPool pool = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
    pools.push_back(pool);
    ++stats.pools;
    allocation.descriptorPool = pool;
    VkDescriptorSet set = VK_NULL_HANDLE;
    Check(allocate(context.device, &allocation, &set), "vkAllocateDescriptorSets");
    ++stats.sets;
    return {set, pool};
}

void DescriptorCache::Free(const SetAllocation& allocation) noexcept {
    if (allocation.set == VK_NULL_HANDLE || allocation.pool == VK_NULL_HANDLE) return;
    std::lock_guard lock(mutex);
    static_cast<void>(freeSets(context.device, allocation.pool, 1, &allocation.set));
}

DescriptorCache::Stats DescriptorCache::Counters() const {
    std::lock_guard lock(mutex);
    return stats;
}

std::size_t ShaderResources::addGuestBuffer(std::span<const std::uint32_t> words, const ColorTarget* target, std::uint64_t indexAddress, std::size_t indexBytes, bool written) {
    Require(words.size() == 4, "buffer descriptor must contain four DWORDs");
    Require((words[1] & 0x40000000u) == 0, "buffer descriptor has reserved bits set");
    const ShaderRecompiler::ShaderBufferResource descriptor{{words[0], words[1], words[2], words[3]}};
    Require(descriptor.Type() == 0u, "buffer descriptor uses an unsupported type");
    const auto address = descriptor.Base48();
    const auto byteSize = descriptor.GetSize();
    Require(address != 0, "null shader buffer descriptor address");
    Require(byteSize != 0, "empty shader buffer descriptor");
    Require(byteSize <= context.limits.maxStorageBufferRange, "shader buffer exceeds descriptor range limit");
    Require(byteSize <= std::numeric_limits<std::size_t>::max(), "shader buffer size exceeds host address space");
    const auto size = static_cast<std::size_t>(byteSize);
    Require(target == nullptr || !overlap(address, size, target->address, target->bytes), "shader buffer aliases the render target");
    // APS5_ALL_BUFFERS_WRITTEN=1: every element is noted as written, as before bufferWritten existed.
    static const bool allWritten = std::getenv("APS5_ALL_BUFFERS_WRITTEN") != nullptr;
    written = written || allWritten;
    // Unconditional (also for read-only elements): Draw.cpp's CheckBufferAliases repeats this check
    // on every resource-cache hit, and a draw must fail its hit exactly when it fails its build.
    Require(!overlap(address, size, indexAddress, indexBytes), "writable shader buffer aliases the index buffer");
    if (written) guestMemory.AddWritable(address, size);
    else {
        guestMemory.AddReadable(address, size);
        ++readOnlyBuffers;
    }
    if (BuildProfiled()) {
        auto& counters = BufferWrites();
        counters.elements.fetch_add(1, std::memory_order_relaxed);
        if (!written) counters.readOnly.fetch_add(1, std::memory_order_relaxed);
    }
    allocations.push_back({address, size, true, nullptr, ShaderRecompiler::DescriptorRole::ShaderData, written});
    return allocations.size() - 1;
}

std::string ShaderResources::Describe() const {
    const auto sample = [](std::uint64_t address, std::uint64_t bytes) {
        // Heaps bound whole can include uncommitted pages; those ranges are not sampled.
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes))) return -1.0;
        std::size_t nonzero = 0;
        std::size_t samples = 0;
        const auto* data = reinterpret_cast<const std::uint32_t*>(address);
        const auto words = bytes / 4;
        const auto step = std::max<std::uint64_t>(1, words / 4096);
        for (std::uint64_t i = 0; i < words; i += step, ++samples) nonzero += data[i] != 0;
        return samples == 0 ? 0.0 : static_cast<double>(nonzero) / samples;
    };
    std::string text;
    char line[160];
    for (const auto& range : describedRanges) {
        std::snprintf(line, sizeof(line), " %s 0x%llx+0x%llx(%ux%u f%u t%d) nz=%.2f", range.kind, static_cast<unsigned long long>(range.address), static_cast<unsigned long long>(range.bytes), range.width, range.height, range.format, range.tileMode, sample(range.address, range.bytes));
        text += line;
        if (range.dccAddress != 0) {
            const auto keys = ReadDccKeys(range.dccAddress, range.bytes);
            std::snprintf(line, sizeof(line), " dcc=%s@0x%llx", DccKeysName(keys), static_cast<unsigned long long>(range.dccAddress));
            text += line;
            if (keys == DccKeys::Mixed) {
                // Where the keys change: first key, how many leading keys match it, and the next key.
                const auto* bytes = reinterpret_cast<const std::uint8_t*>(range.dccAddress);
                const auto count = static_cast<std::size_t>(range.bytes / 256u);
                std::size_t run = 1;
                while (run < count && bytes[run] == bytes[0]) ++run;
                std::snprintf(line, sizeof(line), "(%02x x%zu then %02x of %zu)", bytes[0], run, run < count ? bytes[run] : 0u, count);
                text += line;
            }
        }
    }
    // APS5_TRACE_DISPATCH_IO=2 also lists the words of small buffers (constants).
    static const bool words = [] { const char* value = std::getenv("APS5_TRACE_DISPATCH_IO"); return value != nullptr && value[0] == '2'; }();
    const auto appendWords = [&](const std::uint32_t* data, std::size_t bytes) {
        if (!words || bytes > 0x200) return;
        text += " [";
        for (std::size_t i = 0; i < bytes / 4; ++i) {
            char word[12];
            std::snprintf(word, sizeof(word), "%s%08x", i == 0 ? "" : " ", data[i]);
            text += word;
        }
        text += "]";
    };
    for (const auto& allocation : allocations) {
        if (allocation.guest) {
            std::snprintf(line, sizeof(line), " buffer%s 0x%llx+0x%zx nz=%.2f", allocation.written ? "" : "(ro)", static_cast<unsigned long long>(allocation.address), allocation.size, sample(allocation.address, allocation.size));
            text += line;
            if (GuestMemory::Accessible(reinterpret_cast<const void*>(allocation.address), allocation.size)) appendWords(reinterpret_cast<const std::uint32_t*>(allocation.address), allocation.size);
        } else if (allocation.buffer) {
            const auto bytes = allocation.buffer->Bytes();
            std::snprintf(line, sizeof(line), " data+0x%zx nz=%.2f", allocation.size, sample(reinterpret_cast<std::uint64_t>(bytes.data()), allocation.size));
            text += line;
            appendWords(reinterpret_cast<const std::uint32_t*>(bytes.data()), allocation.size);
        }
    }
    return text;
}

std::size_t ShaderResources::addDataBuffer(std::span<const std::uint32_t> words) {
    const auto size = words.size() * sizeof(std::uint32_t);
    Require(size <= context.limits.maxStorageBufferRange, "shader data buffer exceeds descriptor range limit");
    auto buffer = std::make_unique<Buffer>(context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memcpy(buffer->Bytes().data(), words.data(), size);
    allocations.push_back({0, size, false, std::move(buffer)});
    return allocations.size() - 1;
}

void ShaderResources::addImageBinding(const ShaderRecompiler::DescriptorBinding& binding, VkShaderStageFlags flags) {
    Require(binding.count != 0, "empty descriptor binding");
    if (binding.kind == ShaderRecompiler::DescriptorKind::StorageImage) {
        // Storage images are guest textures the shader writes (looked up in stage B).
        Require(binding.role == ShaderRecompiler::DescriptorRole::GuestImages, "storage image binding has a non-image role");
        Require(binding.guestDescriptor.size() == static_cast<std::size_t>(binding.count) * 8u, "guest storage image descriptors must contain 8 dwords each");
        Require(context.detiler != nullptr, "device texture detiler is unavailable");
        Require(binding.count <= context.limits.maxPerStageDescriptorStorageImages, "shader storage-image descriptors exceed per-stage limits");
        bindings.push_back({{binding.binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, binding.count, flags, nullptr}, {}, {}});
        plannedStorageImages += binding.count;
        deferredImages.push_back({&binding, bindings.size() - 1});
        return;
    }
    const bool sampledImage = binding.kind == ShaderRecompiler::DescriptorKind::SampledImage;
    const bool samplerKind = binding.kind == ShaderRecompiler::DescriptorKind::Sampler;
    Require(sampledImage || samplerKind, std::string("unsupported descriptor kind ") + kindName(binding.kind) + " for role " + roleName(binding.role));
    Require((sampledImage && binding.role == ShaderRecompiler::DescriptorRole::GuestImages) || (samplerKind && binding.role == ShaderRecompiler::DescriptorRole::GuestSamplers), "guest image descriptor role disagrees with its kind");
    Require(binding.guestDescriptor.size() % binding.count == 0, "guest image descriptor size is not a multiple of the binding count");
    const auto elementWords = binding.guestDescriptor.size() / binding.count;

    Binding item{{binding.binding, sampledImage ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER, binding.count, flags, nullptr}, {}, {}};

    if (sampledImage) {
        Require(elementWords == 8, "guest texture descriptor must contain 8 dwords");
        Require(binding.imageShape.has_value(), "guest image binding is missing an image shape");
        Require(context.detiler != nullptr, "device texture detiler is unavailable");
        Require(context.textureCache != nullptr, "device texture cache is unavailable");
        Require(binding.count <= context.limits.maxPerStageDescriptorSampledImages, "shader sampled-image descriptors exceed per-stage limits");
        plannedSampledImages += binding.count;
        Require(plannedSampledImages <= context.limits.maxDescriptorSetSampledImages, "pipeline sampled-image descriptors exceed device limits");
    } else {
        Require(elementWords == 4, "guest sampler descriptor must contain 4 dwords");
        Require(binding.count <= context.limits.maxPerStageDescriptorSamplers, "shader sampler descriptors exceed per-stage limits");
        Require(binding.samplerDepthCompare.size() == binding.count, "guest sampler binding is missing depth comparison metadata");
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
            const bool compareEnable = binding.samplerDepthCompare.at(element);
            static const bool noSamplerCache = std::getenv("APS5_NO_SAMPLER_CACHE") != nullptr;
            if (context.samplerCache != nullptr && !noSamplerCache) {
                samplers.push_back(context.samplerCache->Get(context, words, compareEnable));
            } else {
                auto resource = DecodeSamplerResource(words);
                resource.compareEnable = compareEnable;
                samplers.push_back(std::make_shared<Sampler>(context, resource));
            }
            item.imageAllocations.push_back(samplers.size() - 1);
        }
        Require(samplers.size() <= context.limits.maxDescriptorSetSamplers, "pipeline sampler descriptors exceed device limits");
    }

    bindings.push_back(std::move(item));
    if (sampledImage) deferredImages.push_back({&binding, bindings.size() - 1});
}

namespace {

// Calls `visit(binding, element, words)` for every sampled and storage image element of a compiled
// shader's bindings, in plan order (the order addImageBinding and resolveImageBinding walk).
template <typename Visit>
void forEachImageElement(const ShaderRecompiler::RecompileResult& program, Visit&& visit) {
    for (const auto& binding : program.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestImages || binding.count == 0) continue;
        if (binding.kind != ShaderRecompiler::DescriptorKind::SampledImage && binding.kind != ShaderRecompiler::DescriptorKind::StorageImage) continue;
        const auto elementWords = binding.guestDescriptor.size() / binding.count;
        for (std::uint32_t element = 0; element < binding.count; ++element) visit(binding, element, std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords));
    }
}

}

bool ShaderResources::precollectImages() {
    // The image lookups of stage B (cachedTexture, StorageTexture::Refresh) each start with a
    // GuestMemory::CollectWrites of their surface, the GetWriteWatch walk that is most of the
    // lookups' time under the lock. The walk is memoized per thread epoch, and the worker thread
    // passes no ordering point between the stages (see GuestMemory::BumpCollectEpoch), so walking
    // every surface here makes stage B's collects memo hits: the walk leaves the lock, the lookups'
    // checks stay where they were. A memo miss (the ring overflowed, an uncommitted page) just walks
    // under the lock as before; a descriptor stage B rejects is left to it. APS5_NO_PRECOLLECT=1
    // disables the pass.
    // The pass also leaves one ImageRecord per element: the decode and the surface description are
    // made once for the build, and a sampled element's cache entry is taken here (a hash lookup
    // under the cache's own mutex) so that stage B, under the device lock, runs only the
    // fastRevalidate predicate on it (see fastTexture). The collect comes before the entry is read,
    // as the predicate's rule demands (the generation the entry moves to must predate the checks).
    static const bool disabled = std::getenv("APS5_NO_PRECOLLECT") != nullptr;
    static const bool noRecords = std::getenv("APS5_NO_STAGE_A_IMAGES") != nullptr;
    if (disabled || deferredImages.empty()) return false;
    imageRecords.clear();
    nextImageRecord = 0;
    auto& counters = TextureCounts();
    for (const auto& deferred : deferredImages) {
        const auto& binding = *deferred.binding;
        const auto elementWords = binding.guestDescriptor.size() / binding.count;
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
            ImageRecord record;
            record.sampled = binding.kind == ShaderRecompiler::DescriptorKind::SampledImage;
            try {
                record.resource = DecodeTextureResource(words);
                record.guestBytes = DescribeSurface(record.resource).guestBytes;
                record.generation = GuestMemory::CollectWrites(record.resource.baseAddress, static_cast<std::size_t>(record.guestBytes));
                record.decoded = true;
                if (record.sampled && !noRecords && words.size() == 8) {
                    std::copy(words.begin(), words.end(), record.words.begin());
                    record.components = {ComponentSwizzleFor(record.resource.dstSelX), ComponentSwizzleFor(record.resource.dstSelY), ComponentSwizzleFor(record.resource.dstSelZ), ComponentSwizzleFor(record.resource.dstSelW)};
                    record.keys = TextureClearKeys(record.resource, record.guestBytes);
                    auto& cache = Textures();
                    std::lock_guard lock(cache.mutex);
                    if (const auto it = findTexture(cache, MakeTextureKey(context.device, words, record.components)); it != cache.entries.end()) {
                        record.texture = it->texture;
                        record.source = it->source;
                        record.entryKeys = it->keys;
                        record.entryGeneration = it->generation;
                        counters.records.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            } catch (const std::exception&) {
                record.decoded = false;
            }
            imageRecords.push_back(std::move(record));
        }
    }
    return true;
}

std::shared_ptr<Texture> ShaderResources::fastTexture(const ImageRecord& record) {
    if (record.texture == nullptr) return nullptr;
    // Exactly the "unchanged" branches of cachedTexture, from write stamps, the pending-results
    // registry and the DCC keys, the way fastRevalidate proves a built object current: the collect
    // first (a memo hit: stage A walked the range), then no other image may have results pending
    // over the memory (a lookup would flush them into it, or view them instead), a storage-sourced
    // view needs its image to be the cache's image of the surface and current with guest memory,
    // a snapshot needs the memory unchanged since its content matched, and the keys must be what
    // the content was made under whenever the surface has any (re-read under the lock: a flush
    // between the stages marks them uncompressed).
    const auto address = record.resource.baseAddress;
    const auto bytes = static_cast<std::size_t>(record.guestBytes);
    if (GuestMemory::CollectWrites(address, bytes) == 0) return nullptr;
    if (PendingStorageOverlaps(address, bytes, record.source.get())) return nullptr;
    auto keys = record.keys;
    if (record.resource.dccAddress != 0) keys = TextureClearKeys(record.resource, record.guestBytes);
    // The keys the entry was made under (a view of a pending render target was made under its clear
    // keys and stays valid while they are unchanged); a change is the full lookup's to judge.
    if (keys != record.entryKeys) return nullptr;
    if (record.source != nullptr) {
        if (!StorageImageCached(context, record.source.get()) || !GuestMemory::UnchangedSince(address, bytes, record.source->Generation())) return nullptr;
        // Under fast-clear keys the view holds only while its image's results are still pending over
        // the surface; flushed, the clear the image cannot see makes the lookup take a snapshot.
        if (keys != DccKeys::Uncompressed && StorageTexture::FindPending(address, record.guestBytes) != record.source) return nullptr;
    } else if (keys == DccKeys::Uncompressed && !GuestMemory::UnchangedSince(address, bytes, record.entryGeneration)) {
        return nullptr;
    }
    // The entry must still be the cache's, holding this object (an evicted object is not reused: the
    // next lookup would make another), and it takes the stage-A generation like a hit would.
    auto& cache = Textures();
    std::lock_guard lock(cache.mutex);
    const auto it = findTexture(cache, MakeTextureKey(context.device, record.words, record.components));
    if (it == cache.entries.end() || it->texture != record.texture) return nullptr;
    if (it->source == nullptr) it->generation = record.generation;
    touchTexture(cache, it);
    logLookup({record.texture.get(), record.resource, record.guestBytes, keys, record.source != nullptr ? 0 : record.generation, record.source.get()});
    return record.texture;
}

void ShaderResources::resolveImageBinding(const ShaderRecompiler::DescriptorBinding& binding, Binding& item) {
    auto& counters = TextureCounts();
    // The element's stage-A record, when the pass ran (records follow the plan order exactly).
    const auto nextRecord = [&]() -> const ImageRecord* { return nextImageRecord < imageRecords.size() ? &imageRecords[nextImageRecord++] : nullptr; };
    if (binding.kind == ShaderRecompiler::DescriptorKind::SampledImage) {
        const auto elementWords = binding.guestDescriptor.size() / binding.count;
        for (std::uint32_t element = 0; element < binding.count; ++element) {
            const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * elementWords, elementWords);
            const auto* record = nextRecord();
            const auto resource = record != nullptr && record->decoded ? record->resource : DecodeTextureResource(words);
            Require(MatchesGuestDimension(*binding.imageShape, resource.dimension), "guest texture dimension disagrees with the shader's declared image shape");
            const VkComponentMapping components{ComponentSwizzleFor(resource.dstSelX), ComponentSwizzleFor(resource.dstSelY), ComponentSwizzleFor(resource.dstSelZ), ComponentSwizzleFor(resource.dstSelW)};
            const auto guestBytes = record != nullptr && record->decoded ? record->guestBytes : DescribeSurface(resource).guestBytes;
            std::shared_ptr<Texture> texture;
            if (record != nullptr && record->texture != nullptr) {
                texture = fastTexture(*record);
                (texture != nullptr ? counters.fastHits : counters.fastMisses).fetch_add(1, std::memory_order_relaxed);
            }
            if (texture == nullptr) texture = cachedTexture(context, words, resource, components, guestBytes);
            textures.push_back(std::move(texture));
            describedRanges.push_back({"texture", resource.baseAddress, guestBytes, resource.width, resource.height, resource.format, static_cast<int>(resource.tileMode), resource.dccAddress});
            item.imageAllocations.push_back(textures.size() - 1);
        }
        // The line is due even when every element took the fast path (cachedTexture reports too).
        reportTextureCounters();
        return;
    }
    // Consecutive identical storage descriptors address successive mips of one texture (dynamic-mip
    // storage writes).
    std::uint32_t mipOffset = 0;
    for (std::uint32_t element = 0; element < binding.count; ++element) {
        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
        const bool sameAsPrevious = SameAsPreviousStorageElement(binding, element);
        if (sameAsPrevious) ++mipOffset;
        else mipOffset = 0;
        const auto* record = nextRecord();
        const auto resource = record != nullptr && record->decoded ? record->resource : DecodeTextureResource(words);
        if (binding.imageShape.has_value()) Require(MatchesGuestDimension(*binding.imageShape, resource.dimension), "guest storage texture dimension disagrees with the shader's declared image shape");
        const auto mip = std::min(resource.baseLevel + mipOffset, resource.mipCount - 1u);
        const auto guestBytes = record != nullptr && record->decoded ? record->guestBytes : DescribeSurface(resource).guestBytes;
        // The same surface as the previous element: its image was just looked up and refreshed.
        if (sameAsPrevious && StorageDedupeEnabled()) storageTextures.push_back(storageTextures.back());
        else storageTextures.push_back(cachedStorageTexture(context, words, resource, mip, guestBytes));
        storageMips.push_back(mip);
        // Images the shader only reads have nothing to store back.
        storageWritten.push_back(element >= binding.imageWritten.size() || binding.imageWritten[element]);
        describedRanges.push_back({"storage", resource.baseAddress, guestBytes, resource.width, resource.height, resource.format, static_cast<int>(resource.tileMode), resource.dccAddress});
        item.imageAllocations.push_back(storageTextures.size() - 1);
    }
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> ShaderResources::PresyncSurfaces() const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> surfaces;
    // A surface's lookup reads guest memory on the CPU unless it is served GPU-direct from a host
    // import: a storage image of imported memory uploads and refreshes from the import, and a
    // sampled texture of one views that image (SampledFromStorageEligible), so neither waits for
    // the producer.
    const auto consider = [&](std::uint32_t format, std::uint64_t address, std::uint64_t guestBytes, bool sampled) {
        const bool gpuDirect = sampled ? SampledFromStorageEligible(context, format, address, guestBytes) : HostImportCovers(context, address, static_cast<std::size_t>(guestBytes));
        if (!gpuDirect) surfaces.emplace_back(address, guestBytes);
    };
    if (completed) {
        // A cached object (the resource cache): Revalidate repeats the lookups of the objects the
        // build made, which never change afterwards (a lookup returning another object fails the
        // revalidation instead), so the surfaces come from the build's own record of them. A
        // sampled texture viewing a storage image refreshes like that image; a snapshot texture
        // over an eligible surface is replaced by a view without reading. Storage images are
        // checked on their own surface (a mip chain can exceed the element's descriptor). Read
        // without the device lock: these members are fixed once the build completed.
        std::size_t textureIndex = 0;
        for (const auto& range : describedRanges) {
            if (std::strcmp(range.kind, "texture") == 0 && textureIndex < textures.size()) {
                const auto& texture = textures[textureIndex++];
                if (texture->ViewsStorageImage()) consider(range.format, range.address, range.bytes, false);
                else consider(range.format, range.address, range.bytes, true);
            }
        }
        for (std::size_t index = 0; index < storageTextures.size(); ++index) {
            if (index != 0 && storageTextures[index] == storageTextures[index - 1]) continue;
            const auto& image = *storageTextures[index];
            consider(image.Descriptor().format, image.Descriptor().baseAddress, image.GuestBytes(), false);
        }
        return surfaces;
    }
    if (!imageRecords.empty()) {
        for (const auto& record : imageRecords) {
            if (record.decoded) consider(record.resource.format, record.resource.baseAddress, record.guestBytes, record.sampled);
        }
        return surfaces;
    }
    // Stage A has not run (an address-based build, or the pass is off): decoded from the shader.
    if (deferredCompute.program == nullptr) return surfaces;
    forEachImageElement(*deferredCompute.program, [&](const ShaderRecompiler::DescriptorBinding& binding, std::uint32_t, std::span<const std::uint32_t> words) {
        try {
            const auto resource = DecodeTextureResource(words);
            consider(resource.format, resource.baseAddress, DescribeSurface(resource).guestBytes, binding.kind == ShaderRecompiler::DescriptorKind::SampledImage);
        } catch (const std::exception&) {
            // Stage B reports the bad descriptor.
        }
    });
    return surfaces;
}

ShaderResources::~ShaderResources() {
    release();
}

void ShaderResources::release() noexcept {
    // A set from the cache's pool chain goes back to it; a dedicated pool dies with its set.
    if (cachePool && context.descriptorCache != nullptr) context.descriptorCache->Free({_set, cachePool});
    if (pool) context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
    if (_layout && ownsLayout) context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, _layout, nullptr);
    cachePool = VK_NULL_HANDLE;
    pool = VK_NULL_HANDLE;
    _set = VK_NULL_HANDLE;
    _layout = VK_NULL_HANDLE;
}

VkDescriptorSetLayout ShaderResources::Layout() const {
    return _layout;
}

void ShaderResources::Bind(VkCommandBuffer commands, VkPipelineBindPoint bindPoint, VkPipelineLayout layout) const {
    if (_set == VK_NULL_HANDLE) return;
    context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, bindPoint, layout, 0, 1, &_set, 0, nullptr);
}

namespace {
// Debug aid: APS5_GPU_NO_WRITEBACK=1 keeps GPU results out of guest memory.
bool SkipWriteBack() {
    static const bool skip = std::getenv("APS5_GPU_NO_WRITEBACK") != nullptr;
    return skip;
}
}

void ShaderResources::WriteBack() {
    WriteBackBuffers();
    if (SkipWriteBack()) return;
    for (std::size_t index = 0; index < storageTextures.size(); ++index) {
        if (storageWritten[index]) storageTextures[index]->MarkDirty();
    }
}

void ShaderResources::MarkGpuWrites(Recorder& recorder) {
    if (SkipWriteBack()) return;
    for (std::size_t index = 0; index < storageTextures.size(); ++index) {
        if (storageWritten[index]) storageTextures[index]->MarkDirty();
    }
    // Written sub-ranges of buffers the GPU copied out of a host import go back into it by the GPU,
    // recorded here after the work: those regions then need no CPU write-back (HasCopiedWrites),
    // and the note and mark below cover them like direct writes.
    guestMemory.RecordCopyBacks(recorder);
    // Only the written elements' ranges (AddWritable): a read-only element is neither noted here
    // nor marked as a direct write, so CPU reads of its memory never wait for this work.
    recorder.NotePendingWrites(guestMemory.Writes());
    guestMemory.MarkDirectWrites();
    if (!BuildProfiled()) return;
    auto& counters = BufferWrites();
    // Recorded uses only (dispatches and recorded draws): a synchronous draw writes back in
    // WriteBack() and publishes no pending writes, so it has no notes to skip.
    counters.notesSkipped.fetch_add(readOnlyBuffers, std::memory_order_relaxed);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = counters.lastReport.load();
    if (nowMs - last < 10000 || !counters.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto elements = counters.elements.load();
    const auto readOnly = counters.readOnly.load();
    std::fprintf(stderr, "[buffers] descriptor elements bound: %llu total, %llu written, %llu read-only; %llu pending-write notes skipped\n", static_cast<unsigned long long>(elements), static_cast<unsigned long long>(elements - readOnly), static_cast<unsigned long long>(readOnly), static_cast<unsigned long long>(counters.notesSkipped.load()));
}

void ShaderResources::WriteBackBuffers() {
    if (bda) bda->CheckFault();
    if (SkipWriteBack()) return;
    guestMemory.WriteBack();
}

}
