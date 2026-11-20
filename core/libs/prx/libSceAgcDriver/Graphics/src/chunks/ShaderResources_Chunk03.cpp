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
StorageTextureCache& StorageTextures() {
    static StorageTextureCache cache;
    return cache;
}

std::uint64_t storageCacheBytes() {
    return StorageTextures().bytes.load(std::memory_order_relaxed);
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
std::shared_ptr<StorageTexture> evictStorage(StorageTextureCache& cache, std::list<CachedStorageTexture>::iterator it, bool store = true, const char* reason = "cache eviction") {
    auto texture = it->texture;
    it->texture->SetCached(false);
    noteUncached(it->texture);
    if (store) it->texture->Flush(reason);
    else it->texture->Detach();
    cache.bytes -= it->accounted;
    cache.index.erase(it->key);
    cache.byImage.erase(it->texture.get());
    cache.entries.erase(it);
    return texture;
}

// Storage images are shared by every descriptor of one surface (address, extent, layers, format, tile
// mode): the image holds the whole mip chain, and render targets in the same memory attach to it.
std::array<std::uint32_t, 8> SurfaceKey(const Context& context, const GuestTextureResource& resource) {
    // Guest formats that store in the same Vulkan format share the image (views carry the difference).
    return {static_cast<std::uint32_t>(resource.baseAddress | resource.pipeBankXor), static_cast<std::uint32_t>(resource.baseAddress >> 32u), resource.width, resource.height, (resource.depthOrLastArray << 16u) | (resource.mipCount & 0xffffu), (static_cast<std::uint32_t>(resource.tileMode) << 12u) | (static_cast<std::uint32_t>(resource.dimension) << 20u), resource.baseArray, static_cast<std::uint32_t>(StorageFormatForGuest(context, resource.format))};
}

struct ExtendedSurfaces {
    AgcDriver::Mutex mutex;
    std::map<std::array<std::uint32_t, 8>, std::uint32_t> levels;
    std::atomic<bool> any{false};
};

ExtendedSurfaces& ExtendedChains() {
    static ExtendedSurfaces surfaces;
    return surfaces;
}

std::uint32_t AllocatedLevels(const GuestTextureResource& resource) {
    return resource.allocatedMipCount != 0 ? resource.allocatedMipCount : resource.mipCount;
}

GuestTextureResource StorageSurface(const Context& context, const GuestTextureResource& viewed) {
    auto& surfaces = ExtendedChains();
    const bool extended = viewed.mipCount > AllocatedLevels(viewed);
    if (!extended && !surfaces.any.load(std::memory_order_acquire)) return viewed;
    auto surface = viewed;
    surface.mipCount = AllocatedLevels(viewed);
    const auto identity = SurfaceKey(context, surface);
    std::lock_guard lock(surfaces.mutex);
    if (extended) {
        auto& levels = surfaces.levels[identity];
        levels = std::max(levels, viewed.mipCount);
        surfaces.any.store(true, std::memory_order_release);
        surface.mipCount = levels;
    } else if (const auto it = surfaces.levels.find(identity); it != surfaces.levels.end()) {
        surface.mipCount = it->second;
    } else {
        surface.mipCount = viewed.mipCount;
    }
    return surface;
}

struct WrittenStorage {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> bytes;
    std::vector<WrittenStorageExtent> extents;
};

static WrittenStorage WrittenStorageBytes(const GuestTextureResource& viewed, std::uint32_t mip) {
    WrittenStorage written;
    const auto geometry = DescribeSurface(viewed);
    if (geometry.thick || mip >= geometry.mips.size()) return written;
    const auto& level = geometry.mips[mip];
    const auto bytesPerElement = BytesPerElement(viewed.format);
    const auto covered = CoveredMipBytes(viewed.tileMode, bytesPerElement, level);
    const bool blocks = viewed.tileMode != TextureTileMode::kLinear && !level.tail;
    const auto first = geometry.layers > 1u && geometry.imageDepth <= 1u ? std::min(viewed.baseArray, geometry.layers) : 0u;
    for (auto layer = first; layer < geometry.layers; ++layer) {
        if (!geometry.HasLayer(mip, layer)) continue;
        const auto begin = viewed.baseAddress + geometry.GuestOffset(layer, level);
        for (const auto& [rangeBegin, rangeEnd] : covered) {
            if (!written.bytes.empty() && written.bytes.back().second == begin + rangeBegin) written.bytes.back().second = begin + rangeEnd;
            else written.bytes.emplace_back(begin + rangeBegin, begin + rangeEnd);
        }
        if (blocks) written.extents.push_back({begin, level.width, level.height, bytesPerElement, ThinBlockLayout(viewed.tileMode, bytesPerElement), level.blocksPerRow});
    }
    return written;
}

static void claimStoredMultisampleMemory(const Context& context, const GuestTextureResource& resource, std::uint64_t guestBytes, std::uint32_t mip) {
    if (!MultisampleColorTargetOverlaps(resource.baseAddress, guestBytes)) return;
    for (const auto& [begin, end] : WrittenStorageBytes(ArrayLayerWindow(resource, context.limits.maxImageArrayLayers), mip).bytes) ClaimMultisampleTargetStore(begin, end - begin);
}

// `guestBytes` is the surface size when the caller described the surface already (0: described here).
std::shared_ptr<StorageTexture> cachedStorageTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& requested, std::uint32_t mip, std::uint64_t guestBytes, bool written, std::optional<std::pair<std::uint32_t, std::uint32_t>> layers) {
    const auto viewed = ArrayLayerWindow(requested, context.limits.maxImageArrayLayers);
    if (viewed.baseAddress != requested.baseAddress) guestBytes = 0;
    Require(viewed.dimension != TextureDimension::k2DMsaa, "storage image access to a multisampled texture is not implemented");
    const auto viewedBytes = guestBytes != 0 ? guestBytes : SurfaceGuestBytes(viewed);
    RequireNoMultisampleTarget(viewed.baseAddress, static_cast<std::size_t>(viewedBytes), "a storage image access");
    if (auto depth = DepthSurfaceStorage(context, words, viewed, mip)) return depth;
    if (viewedBytes >= (1ull << 30u)) {
        static std::atomic<std::uint64_t> large{0};
        const auto count = large.fetch_add(1) + 1;
        if (count <= 5 || (count & (count - 1)) == 0) {
            std::fprintf(stderr, "[gpu] storage view 0x%llx (%ux%u format %u, dimension %d, tile %d, slices %u..%u, mip %u) covers %llu MiB; %llu such views\n", static_cast<unsigned long long>(viewed.baseAddress), viewed.width, viewed.height, viewed.format, static_cast<int>(viewed.dimension), static_cast<int>(viewed.tileMode), viewed.baseArray, viewed.depthOrLastArray, mip, static_cast<unsigned long long>(viewedBytes >> 20u), static_cast<unsigned long long>(count));
            reportGuestAllocation(viewed, viewedBytes);
        }
    }
    static const bool disabled = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    if ((DepthPlanesStale() || written) && (written || !layers || disabled)) static_cast<void>(FlushDepthPlanes(viewed.baseAddress, static_cast<std::size_t>(viewedBytes), written ? DepthPlaneScope::Whole : DepthPlaneScope::Overlapped));
    if (written && DepthSurfaceOverlaps(context.device, viewed.baseAddress, viewedBytes)) {
        const auto storage = WrittenStorageBytes(viewed, mip);
        RetireCoveredDepthSurfaces(context.device, storage.bytes, storage.extents);
    }
    if (disabled) return StorageTexture::Create(context, *context.detiler, viewed, mip);
    static_cast<void>(words);
    if (auto* recorder = Recorder::Active(); recorder != nullptr) recorder->BoundKeptBytes();
    const bool profile = LookupOutcomes::Profiled();
    auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    auto& counters = TextureCounts();
    const auto resource = StorageSurface(context, viewed);
    const StorageKey key{context.device, SurfaceKey(context, resource)};
    auto& cache = StorageTextures();
    std::vector<std::shared_ptr<StorageTexture>> evicted;
    std::shared_ptr<StorageTexture> texture;
    {
        std::lock_guard lock(cache.mutex);
        const CacheHold held(storageCacheHeld);
        if (resource.mipCount > AllocatedLevels(resource)) {
            auto allocated = resource;
            allocated.mipCount = AllocatedLevels(resource);
            if (const auto found = findStorage(cache, {context.device, SurfaceKey(context, allocated)}); found != cache.entries.end()) evicted.push_back(evictStorage(cache, found));
        }
        auto it = findStorage(cache, key);
        std::shared_ptr<StorageTexture> detached;
        if (it != cache.entries.end() && MetadataMoved(*it->texture, resource)) {
            detached = evictStorage(cache, it, false);
            evicted.push_back(detached);
            it = cache.entries.end();
        }
        if (it != cache.entries.end()) {
            if (layers) it->texture->RefreshLayers(layers->first, layers->second);
            else it->texture->Refresh();
            cache.entries.splice(cache.entries.begin(), cache.entries, it);
            texture = it->texture;
            if (TextureCountersReported()) counters.storageHits.fetch_add(1, std::memory_order_relaxed);
            if (profile) LookupOutcomes::Add(LookupOutcomes::StorageHit, start);
        } else {
            if (guestBytes == 0) guestBytes = SurfaceGuestBytes(resource);
            bool flushed = false;
            auto handOver = StorageTexture::PrepareCreation(context, resource, guestBytes, std::move(detached), flushed);
            if (flushed && profile) start = LookupOutcomes::Add(LookupOutcomes::PendingFlush, start);
            std::shared_ptr<StorageTexture> created;
            {
                const CacheHold reclaimable(storageCacheReclaimable);
                created = StorageTexture::Create(context, *context.detiler, resource, mip);
            }
            created->AdoptHandOver(std::move(handOver));
            CachedStorageTexture entry{key, mip, std::move(created)};
            static const bool keepNew = std::getenv("APS5_NO_KEEP_NEW_STORAGE") == nullptr;
            if (auto* recorder = Recorder::Active(); keepNew && recorder != nullptr && GuestMemory::GpuMutex().HeldByThisThread() && recorder->Recording()) recorder->Keep(entry.texture);
            entry.accounted = heldBytes(*entry.texture);
            const auto budget = storageBudget(context, cache.bytes + entry.accounted);
            while (!cache.entries.empty()) {
                const bool overBudget = cache.bytes + entry.accounted > budget;
                if (!overBudget && cache.entries.size() < storageEntries()) break;
                const auto back = std::prev(cache.entries.end());
                if (back->texture->TakeUsed()) cache.entries.splice(cache.entries.begin(), cache.entries, back);
                else evicted.push_back(evictStorage(cache, back, true, overBudget ? "cache eviction" : "entry cap"));
            }
            cache.bytes += entry.accounted;
            texture = entry.texture;
            cache.entries.push_front(std::move(entry));
            cache.index[key] = cache.entries.begin();
            cache.byImage[texture.get()] = cache.entries.begin();
            texture->SetCached(true);
            counters.storageCreated.fetch_add(1, std::memory_order_relaxed);
            if (profile) LookupOutcomes::Add(LookupOutcomes::StorageMade, start);
        }
    }
    for (const auto& image : evicted) {
        std::vector<std::shared_ptr<ShaderResources>> resources;
        SharedResourceCache().RemoveImage(image.get(), &resources);
        if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->Recording()) {
            if (image.use_count() > 1) recorder->KeepBytes(image.get(), static_cast<std::size_t>(heldBytes(*image)));
            for (auto& resource : resources) {
                resource->KeepStorageImageBytes(*recorder);
                recorder->Keep(std::move(resource));
            }
        }
    }
    if (!evicted.empty()) {
        auto& sampled = Textures();
        std::lock_guard lock(sampled.mutex);
        dropUncachedViews(sampled);
    }
    return texture;
}

}

namespace {
constexpr std::uint64_t SampledBudgetFloor = 2048ull << 20u;
constexpr std::uint64_t DemandStorageSlack = 256ull << 20u;
constexpr std::uint64_t DemandHeadroomDivisor = 16;
}

std::uint64_t TextureCacheBudget(const VkPhysicalDeviceMemoryProperties& memory) {
    const auto heap = largestDeviceLocalHeap(memory);
    return std::max<std::uint64_t>(2048ull << 20u, heap ? memory.memoryHeaps[*heap].size / 4u : 0u);
}

std::uint64_t StorageTextureCeiling(const VkPhysicalDeviceMemoryProperties& memory, const VkPhysicalDeviceMemoryBudgetPropertiesEXT* reported) {
    return CalculateStorageTextureBudget(memory, reported, 0, 0).ceiling;
}

std::uint64_t StorageTextureBudget(const VkPhysicalDeviceMemoryProperties& memory, const VkPhysicalDeviceMemoryBudgetPropertiesEXT* reported, std::uint64_t sampledBytes, std::uint64_t storageBytes, bool demand) {
    return CalculateStorageTextureBudget(memory, reported, sampledBytes, storageBytes, demand).selectedBudget;
}

StorageTextureBudgetSnapshot CalculateStorageTextureBudget(const VkPhysicalDeviceMemoryProperties& memory, const VkPhysicalDeviceMemoryBudgetPropertiesEXT* reported, std::uint64_t sampledBytes, std::uint64_t storageBytes, bool demand) {
    StorageTextureBudgetSnapshot snapshot;
    snapshot.sampledBytes = sampledBytes;
    snapshot.storageBytes = storageBytes;
    snapshot.ceiling = TextureCacheBudget(memory);
    snapshot.selectedBudget = snapshot.ceiling;
    const auto heap = largestDeviceLocalHeap(memory);
    if (reported == nullptr || !heap || reported->heapBudget[*heap] == 0) return snapshot;
    snapshot.heap = heap;
    const auto heapBudget = reported->heapBudget[*heap];
    const auto usage = reported->heapUsage[*heap];
    const auto other = usage - std::min<std::uint64_t>(usage, sampledBytes + storageBytes);
    snapshot.reserved = demand ? other + std::min<std::uint64_t>(sampledBytes, SampledBudgetFloor) + heapBudget / DemandHeadroomDivisor : other + sampledBytes + heapBudget / 8u;
    snapshot.available = heapBudget > snapshot.reserved ? heapBudget - snapshot.reserved : 0u;
    snapshot.ceiling = std::max<std::uint64_t>(512ull << 20u, std::min<std::uint64_t>(memory.memoryHeaps[*heap].size / 2u, heapBudget / 2u));
    snapshot.selectedBudget = std::min<std::uint64_t>(snapshot.ceiling, std::max<std::uint64_t>(512ull << 20u, snapshot.available));
    return snapshot;
}

std::uint64_t SampledTextureBudget(const VkPhysicalDeviceMemoryProperties& memory, const VkPhysicalDeviceMemoryBudgetPropertiesEXT* reported, std::uint64_t textureBytes, std::uint64_t storageBytes, bool demand) {
    const auto heap = largestDeviceLocalHeap(memory);
    if (reported == nullptr || !heap || reported->heapBudget[*heap] == 0) return TextureCacheBudget(memory);
    const auto budget = reported->heapBudget[*heap];
    const auto usage = reported->heapUsage[*heap];
    const auto other = usage - std::min<std::uint64_t>(usage, textureBytes);
    const auto reserved = demand ? other + storageBytes + DemandStorageSlack + budget / DemandHeadroomDivisor : other + std::max(TextureCacheBudget(memory), storageBytes) + budget / 8u;
    return std::max<std::uint64_t>(SampledBudgetFloor, budget > reserved ? budget - reserved : 0u);
}

bool SampledBudgetReportDue(std::uint64_t reported, std::uint64_t budget, std::chrono::steady_clock::duration sinceReport) {
    const auto change = budget > reported ? budget - reported : reported - budget;
    return change > reported / 10u && sinceReport >= std::chrono::seconds(10);
}

std::optional<std::uint32_t> LargestDeviceLocalHeap(const VkPhysicalDeviceMemoryProperties& memory) {
    return largestDeviceLocalHeap(memory);
}

std::uint64_t StorageTextureCacheBudget(const Context& context) {
    return storageBudget(context);
}

void SampledFromStorageForTest(bool enabled) {
    sampledFromStorageForTest.store(enabled, std::memory_order_relaxed);
}

void OverrideCacheBudgetForTest(std::uint64_t bytes) {
    budgetOverride.store(bytes, std::memory_order_relaxed);
}

TextureCacheUse TextureCacheUsage() {
    TextureCacheUse use;
    {
        auto& sampled = Textures();
        std::lock_guard lock(sampled.mutex);
        dropUncachedViews(sampled);
        use.sampledEntries = sampled.entries.size();
        use.sampledBytes = sampled.bytes;
        use.spareEntries = sampled.spare.size();
        use.spareBytes = sampled.spareBytes;
    }
    use.storageBytes = storageCacheBytes();
    return use;
}

std::uint64_t SampledTextureCacheBudget(const Context& context) {
    auto& cache = Textures();
    std::lock_guard lock(cache.mutex);
    return sampledBudget(context, cache);
}

std::shared_ptr<Texture> CachedSampledTexture(const Context& context, std::span<const std::uint32_t> words, VkImageViewType multisampleViewType) {
    const auto resource = DecodeTextureResource(words);
    return cachedTexture(context, words, resource, ViewComponents(resource), 0, false, multisampleViewType);
}

bool SampledTexturesShareEntry(std::span<const std::uint32_t> first, std::span<const std::uint32_t> second) {
    Require(first.size() == 8 && second.size() == 8, "sampled texture keys take eight descriptor words");
    const VkComponentMapping components{VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    const auto firstKey = MakeTextureKey(VK_NULL_HANDLE, first, components);
    const auto secondKey = MakeTextureKey(VK_NULL_HANDLE, second, components);
    return firstKey == secondKey && TextureKeyHash{}(firstKey) == TextureKeyHash{}(secondKey);
}

void FlushCachedTextures(VkDevice device) {
    Require(device != VK_NULL_HANDLE, "cannot flush textures without a Vulkan device");
    GuestMemory::AssertGpuLockHeld("FlushCachedTextures");
    auto& cache = StorageTextures();
    std::lock_guard lock(cache.mutex);
    const CacheHold held(storageCacheHeld);
    for (const auto& entry : cache.entries) {
        if (entry.key.device == device) entry.texture->Flush();
    }
}

WriteIndexOutcomes WriteIndexOutcomesCounted() {
    const auto& counts = IndexCounts();
    const auto at = [&](IndexCount which) { return counts[which].load(std::memory_order_relaxed); };
    return {at(IndexAccepted), at(IndexPartial), at(IndexFallback), at(IndexBumped), at(IndexLookedUp), at(IndexRebound), at(IndexCompared), at(IndexBuilds)};
}

void ClearCachedTextures(VkDevice device) {
    Require(device != VK_NULL_HANDLE, "cannot clear textures without a Vulkan device");
    auto& sampled = Textures();
    {
        std::lock_guard lock(sampled.mutex);
        const CacheHold held(sampledCacheHeld);
        for (auto it = sampled.entries.begin(); it != sampled.entries.end();) {
            if (it->key.device == device) eraseTexture(sampled, it++);
            else ++it;
        }
        sampled.spare.clear();
        sampled.spareBytes = 0;
    }
    auto& storage = StorageTextures();
    std::vector<std::shared_ptr<StorageTexture>> cleared;
    {
        std::lock_guard lock(storage.mutex);
        const CacheHold held(storageCacheHeld);
        for (auto it = storage.entries.begin(); it != storage.entries.end();) {
            if (it->key.device != device) {
                ++it;
                continue;
            }
            cleared.push_back(it->texture);
            it->texture->SetCached(false);
            noteUncached(it->texture);
            storage.bytes -= it->accounted;
            storage.index.erase(it->key);
            storage.byImage.erase(it->texture.get());
            it = storage.entries.erase(it);
        }
    }
    for (const auto& image : cleared) {
        std::vector<std::shared_ptr<ShaderResources>> resources;
        SharedResourceCache().RemoveImage(image.get(), &resources);
        if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->Recording()) {
            for (auto& resource : resources) {
                resource->KeepStorageImageBytes(*recorder);
                recorder->Keep(std::move(resource));
            }
        }
    }
}

std::uint64_t ReleaseTextureCaches(std::uint64_t bytes, bool newImage) {
    thread_local bool releasing = false;
    if (releasing) return 0;
    releasing = true;
    struct Done {
        bool& flag;
        ~Done() { flag = false; }
    } done{releasing};
    const auto releasedBefore = ImageMemoryReleasedBytes();
    std::uint64_t freed = 0;
    std::size_t sampledEntries = 0;
    std::size_t storageEntries = 0;
    std::size_t storageKept = 0;
    bool sampledSkipped = false;
    bool storageSkipped = false;
    {
        auto& cache = Textures();
        std::unique_lock lock(cache.mutex, std::defer_lock);
        const bool inherited = sampledCacheHeld != 0;
        if ((inherited && (!newImage || sampledCacheReclaimable == 0)) || (!inherited && !lock.try_lock())) {
            sampledSkipped = true;
        } else {
            for (auto it = cache.entries.end(); it != cache.entries.begin() && freed < bytes;) {
                --it;
                if (it->texture.use_count() != 1 || (it->source != nullptr && it->source->Dirty())) continue;
                const auto next = std::next(it);
                eraseTexture(cache, it);
                ++cache.counts.evicted;
                ++sampledEntries;
                freed = ImageMemoryReleasedBytes() - releasedBefore;
                it = next;
            }
        }
    }
    if (freed < bytes) {
        auto& cache = StorageTextures();
        std::unique_lock<GuestMemory::GpuMutexType> gpu(GuestMemory::GpuMutex(), std::defer_lock);
        const bool inherited = storageCacheHeld != 0;
        if ((inherited && (!newImage || storageCacheReclaimable == 0)) || !gpu.try_lock()) {
            storageSkipped = true;
        } else {
            try {
                while (freed < bytes) {
                    std::shared_ptr<StorageTexture> image;
                    {
                        std::unique_lock lock(cache.mutex, std::defer_lock);
                        if (!inherited && !lock.try_lock()) {
                            storageSkipped = true;
                            break;
                        }
                        storageKept = 0;
                        bool spared = false;
                        for (auto it = cache.entries.end(); it != cache.entries.begin();) {
                            --it;
                            if (it->texture->Dirty()) {
                                ++storageKept;
                                continue;
                            }
                            if (it->texture->TakeUsed()) {
                                spared = true;
                                continue;
                            }
                            image = evictStorage(cache, it);
                            break;
                        }
                        if (image == nullptr) {
                            if (spared) continue;
                            break;
                        }
                    }
                    std::vector<std::shared_ptr<ShaderResources>> resources;
                    const bool removed = SharedResourceCache().TryRemoveImage(image.get(), &resources);
                    if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->Recording()) {
                        for (auto& resource : resources) recorder->Keep(std::move(resource));
                    }
                    resources.clear();
                    ++storageEntries;
                    image.reset();
                    freed = ImageMemoryReleasedBytes() - releasedBefore;
                    if (!removed) {
                        storageSkipped = true;
                        break;
                    }
                }
            } catch (const std::exception& error) {
                std::fprintf(stderr, "[gpu] texture cache release stopped: %s\n", error.what());
            }
        }
    }
    freed = ImageMemoryReleasedBytes() - releasedBefore;
    std::fprintf(stderr, "[gpu] texture caches released %llu MiB of the %llu MiB asked for: %zu sampled entries%s, %zu storage images%s, %zu with pending results kept\n", static_cast<unsigned long long>(freed >> 20u), static_cast<unsigned long long>(bytes >> 20u), sampledEntries, sampledSkipped ? " (cache busy, skipped)" : "", storageEntries, storageSkipped ? " (cache or device busy, skipped)" : "", storageKept);
    return freed;
}

}
