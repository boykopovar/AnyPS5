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
std::optional<std::uint32_t> largestDeviceLocalHeap(const VkPhysicalDeviceMemoryProperties& memory) {
    std::optional<std::uint32_t> heap;
    for (std::uint32_t i = 0; i < std::min<std::uint32_t>(memory.memoryHeapCount, VK_MAX_MEMORY_HEAPS); ++i) {
        if ((memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0 && (!heap || memory.memoryHeaps[i].size > memory.memoryHeaps[*heap].size)) heap = i;
    }
    return heap;
}

std::atomic<std::uint64_t> budgetOverride{0};

std::uint64_t forcedCacheBudget() {
    if (const auto overridden = budgetOverride.load(std::memory_order_relaxed); overridden != 0) return overridden;
    static const std::uint64_t forced = [] {
        const char* text = std::getenv("APS5_TEXTURE_CACHE_MIB");
        return text != nullptr ? std::strtoull(text, nullptr, 10) << 20u : 0u;
    }();
    return forced;
}

bool demandBudget() {
    static const bool enabled = [] {
        const char* text = std::getenv("APS5_TEXTURE_BUDGET_DEMAND");
        return text != nullptr && *text != '\0' && *text != '0';
    }();
    return enabled;
}

std::uint64_t storageBudget(const Context& context, std::uint64_t required = 0, std::optional<std::chrono::steady_clock::time_point>* calculatedAt = nullptr) {
    if (calculatedAt != nullptr) calculatedAt->reset();
    const auto forced = forcedCacheBudget();
    if (forced != 0) return forced;
    if (context.memoryProperties2 == nullptr || context.physical == VK_NULL_HANDLE) return TextureCacheBudget(context.memory);
    static struct {
        AgcDriver::Mutex mutex;
        VkPhysicalDevice device{VK_NULL_HANDLE};
        std::chrono::steady_clock::time_point read{};
        std::uint64_t budget{0};
        std::uint64_t ceiling{0};
        std::chrono::steady_clock::time_point calculatedAt{};
    } cached;
    const auto now = std::chrono::steady_clock::now();
    std::optional<VkPhysicalDeviceMemoryBudgetPropertiesEXT> reported;
    StorageTextureBudgetSnapshot snapshot;
    std::uint64_t budget;
    {
        std::lock_guard lock(cached.mutex);
        const auto read = [&] {
            reported.emplace(VkPhysicalDeviceMemoryBudgetPropertiesEXT{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT});
            VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
            properties.pNext = &*reported;
            context.memoryProperties2(context.physical, &properties);
            cached.device = context.physical;
            cached.read = now;
            const auto sampled = SampledTextureMemory();
            const auto storage = storageCacheBytes();
            const auto owned = UsageWithoutHostImports(context.memory, *reported, HostImportLiveBytes(), context.hostImportsInHeapUsage);
            snapshot = CalculateStorageTextureBudget(context.memory, &owned, sampled, storage, demandBudget());
            cached.budget = snapshot.selectedBudget;
            cached.ceiling = snapshot.ceiling;
            cached.calculatedAt = std::chrono::steady_clock::now();
        };
        if (cached.device != context.physical || now - cached.read >= std::chrono::seconds(1)) read();
        const auto needed = std::min(required, cached.ceiling);
        if (needed > cached.budget && context.bufferPool != nullptr && context.bufferPool->ReclaimIdleDevice(needed - cached.budget, now) != 0) read();
        budget = cached.budget;
        if (calculatedAt != nullptr) *calculatedAt = cached.calculatedAt;
    }
    if (reported) MemoryAccounting::Report(context, *reported, snapshot);
    return budget;
}

std::size_t storageEntries() {
    static const std::size_t entries = [] {
        const char* text = std::getenv("APS5_STORAGE_CACHE_ENTRIES");
        if (text == nullptr || *text == '\0') return std::size_t{2048};
        const auto parsed = std::strtoull(text, nullptr, 10);
        return parsed != 0 ? static_cast<std::size_t>(parsed) : std::numeric_limits<std::size_t>::max();
    }();
    return entries;
}

std::uint64_t sampledBudget(const Context& context, TextureCache& cache) {
    if (const auto forced = forcedCacheBudget(); forced != 0) return forced;
    if (context.memoryProperties2 == nullptr || context.physical == VK_NULL_HANDLE) return TextureCacheBudget(context.memory);
    const auto now = std::chrono::steady_clock::now();
    if (cache.budgetDevice == context.physical && now - cache.budgetRead < std::chrono::seconds(1)) return cache.budget;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    properties.pNext = &reported;
    context.memoryProperties2(context.physical, &properties);
    const auto sampled = SampledTextureMemory();
    const auto storage = storageCacheBytes();
    const auto owned = UsageWithoutHostImports(context.memory, reported, HostImportLiveBytes(), context.hostImportsInHeapUsage);
    const auto budget = SampledTextureBudget(context.memory, &owned, sampled + storage, storage, demandBudget());
    const bool fresh = cache.budgetDevice != context.physical;
    std::optional<std::chrono::steady_clock::time_point> storageCalculatedAt;
    const auto storageLimit = storageBudget(context, 0, &storageCalculatedAt);
    if (const auto heap = largestDeviceLocalHeap(context.memory); heap && (fresh || SampledBudgetReportDue(cache.reportedBudget, budget, now - cache.reportedAt) || SampledBudgetReportDue(cache.reportedStorage, storageLimit, now - cache.reportedAt))) {
        char was[48] = "";
        char wasStorage[48] = "";
        char storageLimitAge[32] = "unavailable";
        if (storageCalculatedAt) {
            const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - *storageCalculatedAt);
            std::snprintf(storageLimitAge, sizeof(storageLimitAge), "%llu", static_cast<unsigned long long>(age.count()));
        }
        if (!fresh) {
            std::snprintf(was, sizeof(was), " (was %llu MiB)", static_cast<unsigned long long>(cache.reportedBudget >> 20u));
            std::snprintf(wasStorage, sizeof(wasStorage), " (was %llu MiB)", static_cast<unsigned long long>(cache.reportedStorage >> 20u));
        }
        std::fprintf(stderr, "[gpu] sampled texture cache budget %llu MiB%s: device-local heap %u has a VK_EXT_memory_budget budget of %llu MiB and uses %llu MiB, %llu MiB of it sampled textures (%llu MiB in the cache) and %llu MiB cached storage images; active storage image cache budget %llu MiB%s; storage_limit_age_ms=%s\n", static_cast<unsigned long long>(budget >> 20u), was, *heap, static_cast<unsigned long long>(reported.heapBudget[*heap] >> 20u), static_cast<unsigned long long>(reported.heapUsage[*heap] >> 20u), static_cast<unsigned long long>(sampled >> 20u), static_cast<unsigned long long>(cache.bytes >> 20u), static_cast<unsigned long long>(storage >> 20u), static_cast<unsigned long long>(storageLimit >> 20u), wasStorage, storageLimitAge);
        cache.reportedBudget = budget;
        cache.reportedStorage = storageLimit;
        cache.reportedAt = now;
    }
    cache.budgetDevice = context.physical;
    cache.budgetRead = now;
    cache.budget = budget;
    return budget;
}

// A snapshot over memory outside the write watch is compared with guest memory again only when this
// key changed: a write-watch mark in the range, a driver store, a mapping change or a recorded GPU
// write (as StorageTexture::compareUntracked). Taken before the snapshot is read or compared, so a
// CPU store racing with that leaves a mark for the next key. Zero when the range has no hint.
std::array<std::uint64_t, 4> untrackedKey(std::uint64_t address, std::size_t bytes) {
    if (GuestMemory::CollectEpoch() == 0) return {};
    const auto hint = GuestMemory::UntrackedHint(address, bytes);
    if (hint == 0) return {};
    const auto* recorder = Recorder::Active();
    const std::array<std::uint64_t, 4> key{hint, GuestMemory::NewestStamp(address, bytes), GuestMemory::ForgetSerial(), recorder != nullptr ? recorder->NewestWriteNote(address, bytes) : 0};
    return (key[2] & 1u) == 0 ? key : std::array<std::uint64_t, 4>{};
}

std::span<const std::uint32_t> firstLayerWords(std::span<const std::uint32_t> words, std::optional<ShaderRecompiler::DescriptorImageShape> shape, std::array<std::uint32_t, 8>& narrowed, bool report) {
    if (shape != ShaderRecompiler::DescriptorImageShape::Image2D || words.size() != narrowed.size() || IsNullSampledTextureResource(words)) return words;
    try {
        if (!FirstLayerDescriptor(words, narrowed)) return words;
    } catch (const std::runtime_error&) {
        return words;
    }
    if (report) {
        static std::atomic<std::uint64_t> narrowings{0};
        const auto count = narrowings.fetch_add(1) + 1;
        if (count <= 5 || (count & (count - 1)) == 0) {
            const auto resource = DecodeTextureResource(words);
            std::fprintf(stderr, "[gpu] 2D read of the 2D array 0x%llx (%ux%u format %u, slices %u..%u) built from slice %u alone; %llu such reads\n", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, resource.baseArray, resource.depthOrLastArray, resource.baseArray, static_cast<unsigned long long>(count));
        }
    }
    return narrowed;
}

bool layerRowsBinding(std::optional<ShaderRecompiler::DescriptorImageShape> shape, const GuestTextureResource& resource, bool depthCompare) {
    return !depthCompare && shape == ShaderRecompiler::DescriptorImageShape::Image2D && resource.dimension == TextureDimension::k1DArray;
}

void reportGuestAllocation(const GuestTextureResource& resource, std::uint64_t guestBytes) {
    const auto range = GuestAllocations::GuestAllocationsLookup_nid_postfix(resource.baseAddress, false);
    const auto geometry = DescribeSurface(resource);
    if (range == nullptr) {
        std::fprintf(stderr, "[gpu] view 0x%llx of %llu MiB lies in no registered guest allocation\n", static_cast<unsigned long long>(resource.baseAddress), static_cast<unsigned long long>(guestBytes >> 20u));
        return;
    }
    const auto end = range->address + range->bytes;
    const auto fitting = geometry.thick || geometry.layerBytes == 0 ? 0ull : std::min<std::uint64_t>(geometry.layers, (end - resource.baseAddress) / geometry.layerBytes);
    std::fprintf(stderr, "[gpu] view 0x%llx of %llu MiB lies in the guest allocation 0x%llx..0x%llx (%llu MiB, range 0x%llx..0x%llx, %s%s%s): %llu of its %u slices fit in the range\n", static_cast<unsigned long long>(resource.baseAddress), static_cast<unsigned long long>(guestBytes >> 20u), static_cast<unsigned long long>(range->allocationAddress), static_cast<unsigned long long>(range->allocationAddress + range->allocationBytes), static_cast<unsigned long long>(range->allocationBytes >> 20u), static_cast<unsigned long long>(range->address), static_cast<unsigned long long>(end), range->readable ? "r" : "-", range->writable ? "w" : "-", range->gpu ? "g" : "-", static_cast<unsigned long long>(fitting), geometry.layers);
}

std::optional<CommittedSlabs> committedSlabs(const GuestTextureResource& resource, std::uint64_t guestBytes, const std::optional<std::vector<std::pair<std::uint64_t, std::uint64_t>>>& committed) {
    if (resource.dimension != TextureDimension::k3D) return std::nullopt;
    const auto geometry = DescribeSurface(resource);
    const auto slabBytes = geometry.layerBytes;
    if (slabBytes == 0 || geometry.layers == 0 || geometry.guestBytes != guestBytes || geometry.GuestLayerOffset(geometry.layers - 1u) + slabBytes != guestBytes) return std::nullopt;
    const auto described = committed.has_value() ? GuestMemory::Commitment{} : GuestMemory::DescribeCommitted(resource.baseAddress, static_cast<std::size_t>(guestBytes));
    const auto& ranges = committed.has_value() ? *committed : described.ranges;
    CommittedSlabs result{{}, slabBytes};
    const auto count = guestBytes / slabBytes;
    auto range = ranges.begin();
    for (std::uint64_t slab = 0; slab < count; ++slab) {
        const auto begin = resource.baseAddress + slab * slabBytes;
        while (range != ranges.end() && range->second <= begin) ++range;
        if (range != ranges.end() && range->first < begin + slabBytes) result.slabs.push_back(static_cast<std::uint32_t>(slab));
    }
    if (result.slabs.size() == count) return std::nullopt;
    const auto end = resource.baseAddress + guestBytes;
    for (const auto& [first, last] : ranges) {
        if (last > resource.baseAddress && first < end) result.ranges.emplace_back(std::max(first, resource.baseAddress) - resource.baseAddress, std::min(last, end) - resource.baseAddress);
    }
    return result;
}

bool stillCommitted(const std::optional<CommittedSlabs>& now, const std::optional<CommittedSlabs>& snapshot) {
    if (!now.has_value() || !snapshot.has_value()) return now.has_value() == snapshot.has_value();
    auto range = now->ranges.begin();
    for (const auto& [begin, end] : snapshot->ranges) {
        while (range != now->ranges.end() && range->second <= begin) ++range;
        if (range == now->ranges.end() || range->first > begin || range->second < end) return false;
    }
    return true;
}

std::size_t snapshotBytes(const std::optional<CommittedSlabs>& slabs, std::size_t guestBytes) {
    return slabs.has_value() ? static_cast<std::size_t>(slabs->slabs.size() * slabs->slabBytes) : guestBytes;
}

std::optional<std::span<const std::uint32_t>> packedSlabs(const std::optional<CommittedSlabs>& slabs) {
    if (!slabs.has_value()) return std::nullopt;
    return std::span<const std::uint32_t>(slabs->slabs);
}

std::vector<std::array<std::uint64_t, 3>> slabRuns(const CommittedSlabs& committed) {
    std::vector<std::array<std::uint64_t, 3>> runs;
    for (std::size_t index = 0; index < committed.slabs.size(); ++index) {
        const auto guest = std::uint64_t{committed.slabs[index]} * committed.slabBytes;
        if (!runs.empty() && runs.back()[0] + runs.back()[2] == guest) runs.back()[2] += committed.slabBytes;
        else runs.push_back({guest, index * committed.slabBytes, committed.slabBytes});
    }
    return runs;
}

void readSnapshot(const GuestTextureResource& resource, DccKeys keys, const std::optional<CommittedSlabs>& slabs, std::span<std::byte> bytes) {
    if (!slabs.has_value()) return ReadTextureSurface(resource, keys, bytes);
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureRead);
    for (const auto& [guest, packed, size] : slabRuns(*slabs)) GuestMemory::ReadCommitted(resource.baseAddress + guest, bytes.subspan(static_cast<std::size_t>(packed), static_cast<std::size_t>(size)));
}

bool equalsCommittedSlabs(std::uint64_t address, const CommittedSlabs& slabs, std::span<const std::byte> bytes) {
    for (const auto& [guest, packed, size] : slabRuns(slabs)) {
        if (!GuestMemory::EqualsCommitted(address + guest, bytes.subspan(static_cast<std::size_t>(packed), static_cast<std::size_t>(size)))) return false;
    }
    return true;
}

void reportCommittedSlabs(const GuestTextureResource& resource, const CommittedSlabs& slabs, std::uint64_t guestBytes) {
    static std::atomic<std::uint64_t> views{0};
    const auto count = views.fetch_add(1) + 1;
    if (count > 5 && (count & (count - 1)) != 0) return;
    const auto geometry = DescribeSurface(resource);
    std::uint32_t zeroed = 0;
    for (std::uint32_t layer = 0; layer < geometry.layers; ++layer) zeroed += std::ranges::binary_search(slabs.slabs, geometry.GuestLayerOffset(layer) / slabs.slabBytes) ? 0u : 1u;
    std::fprintf(stderr, "[gpu] sampled view 0x%llx (%ux%u format %u, dimension %d, tile %d, slices %u..%u) uploads the %zu of its %llu slabs holding committed guest memory (%llu of %llu MiB) and %u zero-filled slices; %llu such views\n", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, static_cast<int>(resource.dimension), static_cast<int>(resource.tileMode), resource.baseArray, resource.depthOrLastArray, slabs.slabs.size(), static_cast<unsigned long long>(guestBytes / slabs.slabBytes), static_cast<unsigned long long>((slabs.slabs.size() * slabs.slabBytes) >> 20u), static_cast<unsigned long long>(guestBytes >> 20u), zeroed, static_cast<unsigned long long>(count));
}

struct PendingLookup {
    std::shared_ptr<StorageTexture> source;
    bool depthCopy = false;
    bool srgbExpand = false;
    bool blockCopy = false;
    bool repackCopy = false;
};

PendingLookup pendingLookupSource(const Context& context, const GuestTextureResource& resource, std::uint64_t guestBytes, bool depthCompare, bool layerRows = false) {
    PendingLookup lookup;
    lookup.depthCopy = depthCompare && guestBytes >= DepthCopyMinBytes() && Texture::DepthCopyFormat(ResolveTextureFormat(resource.format)) != VK_FORMAT_UNDEFINED && resource.dimension != TextureDimension::k3D;
    lookup.srgbExpand = !depthCompare && SrgbExpandsToRgba8(context, resource.format);
    lookup.source = StorageTexture::FindPending(resource.baseAddress, guestBytes);
    if (layerRows) lookup.source.reset();
    auto& source = lookup.source;
    lookup.blockCopy = source != nullptr && !depthCompare && !lookup.srgbExpand && Texture::CanBlockCopyFrom(*source, resource);
    lookup.repackCopy = source != nullptr && !depthCompare && Texture::CanRepackCopyFrom(*source, resource);
    if (source != nullptr && !lookup.blockCopy && !lookup.repackCopy && (depthCompare ? !lookup.depthCopy || !Texture::CanDepthCopyFrom(*source, resource) : lookup.srgbExpand ? !Texture::CanSrgbExpandFrom(context, *source, resource) : !Texture::CanCopyFrom(*source, resource))) source.reset();
    if (source != nullptr && MetadataMoved(*source, resource)) source.reset();
    return lookup;
}

// `guestBytes` is the surface size when the caller described the surface already (0: described here).
std::shared_ptr<Texture> cachedTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& requested, VkComponentMapping components, std::uint64_t guestBytes = 0, bool depthCompare = false, VkImageViewType multisampleViewType = VK_IMAGE_VIEW_TYPE_2D, bool layerRows = false) {
    Require(!layerRows || (requested.dimension == TextureDimension::k1DArray && !depthCompare), "only a sampled 1D array is laid out as rows of a 2D image");
    const auto resource = layerRows ? requested : ArrayLayerWindow(requested, context.limits.maxImageArrayLayers);
    if (resource.baseAddress != requested.baseAddress) guestBytes = 0;
    if (resource.dimension == TextureDimension::k2DMsaa) {
        Require(!depthCompare, "native multisampled depth comparison sampling is not implemented");
        if (auto depth = DepthSurfaceTexture(context, words, resource, components, multisampleViewType)) {
            logLookup({depth.get(), resource, 0, DccKeys::Uncompressed, 0, nullptr, true});
            return depth;
        }
        auto target = MultisampleTargetTexture(context, resource, components, multisampleViewType);
        logLookup({target.get(), resource, 0, DccKeys::Uncompressed, 0, nullptr, true});
        return target;
    }
    if (resource.fmaskCmaskAddress != 0u) {
        Require(!depthCompare, "FMASK depth comparison sampling is not implemented");
        Require(resource.baseArray == 0u && resource.depthOrLastArray == 0u, "native FMASK array sampling is not implemented");
        CompletePendingCpuWrites(context, resource.fmaskCmaskAddress, CmaskBytes(resource.width, resource.height));
        auto native = MultisampleTargetFmaskTexture_nid_no_patch(context, resource, components);
        Require(native != nullptr, "compressed FMASK sampling without a matching resident native target is not implemented");
        return native;
    }
    CaptureTrace::Log("sampled-lookup address=%llx width=%u height=%u dcc=%llx", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, static_cast<unsigned long long>(resource.dccAddress));
    RequireNoMultisampleTarget(resource.baseAddress, guestBytes != 0 ? static_cast<std::size_t>(guestBytes) : 1u, "a texture read");
    if (auto* recorder = Recorder::Active(); recorder != nullptr) recorder->BoundKeptBytes();
    Require(!layerRows || !DepthSurfaceAt(resource.baseAddress), "sampling a native depth surface as a 1D array past maxImageArrayLayers is not implemented");
    if (auto depth = layerRows ? nullptr : DepthSurfaceTexture(context, words, resource, components)) {
        logLookup({depth.get(), resource, 0, DccKeys::Uncompressed, 0, nullptr, true});
        return depth;
    }
    const auto depthBitsWidth = words.size() >= 4 ? ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) : 0u;
    constexpr auto float32 = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format32Float);
    if (depthBitsWidth == 32u && resource.format != float32) {
        auto bits = resource;
        bits.format = float32;
        return cachedTexture(context, words, bits, components, guestBytes, depthCompare, VK_IMAGE_VIEW_TYPE_2D, layerRows);
    }
    constexpr auto unorm16 = static_cast<std::uint32_t>(ShaderRecompiler::IrBufferFormat::Format16UNorm);
    if (depthBitsWidth == 16u && resource.format != unorm16) {
        auto normalized = resource;
        normalized.format = unorm16;
        return cachedTexture(context, words, normalized, components, guestBytes, depthCompare, VK_IMAGE_VIEW_TYPE_2D, layerRows);
    }
    const auto viewedWindow = [&] {
        const auto window = ViewedArrayWindow(resource, false);
        if (window.baseAddress != resource.baseAddress || resource.dccAddress == 0) return window;
        if (ViewedArrayWindow(resource, true).baseAddress == resource.baseAddress) return window;
        if (TextureClearKeys(resource, guestBytes != 0 ? guestBytes : SurfaceGuestBytes(resource)) != DccKeys::Uncompressed) return window;
        return ViewedArrayWindow(resource, true);
    };
    if (const auto window = viewedWindow(); window.baseAddress != resource.baseAddress) {
        static std::atomic<std::uint64_t> windows{0};
        const auto count = windows.fetch_add(1) + 1;
        if (count <= 5 || (count & (count - 1)) == 0) std::fprintf(stderr, "[gpu] sampled array view 0x%llx (%ux%u format %u, dimension %d, slices %u..%u) served from its viewed slices at 0x%llx; %llu such views\n", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, static_cast<int>(resource.dimension), resource.baseArray, resource.depthOrLastArray, static_cast<unsigned long long>(window.baseAddress), static_cast<unsigned long long>(count));
        return cachedTexture(context, words, window, components, 0, depthCompare, VK_IMAGE_VIEW_TYPE_2D, layerRows);
    }
    if (guestBytes == 0) guestBytes = SurfaceGuestBytes(resource);
    if (guestBytes >= (1ull << 30u)) {
        static std::atomic<std::uint64_t> large{0};
        const auto count = large.fetch_add(1) + 1;
        if (count <= 5 || (count & (count - 1)) == 0) {
            std::fprintf(stderr, "[gpu] sampled view 0x%llx (%ux%u format %u, dimension %d, tile %d, slices %u..%u) covers %llu MiB; %llu such views\n", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, static_cast<int>(resource.dimension), static_cast<int>(resource.tileMode), resource.baseArray, resource.depthOrLastArray, static_cast<unsigned long long>(guestBytes >> 20u), static_cast<unsigned long long>(count));
            reportGuestAllocation(resource, guestBytes);
        }
    }
    static const bool disabled = std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    const bool profile = LookupOutcomes::Profiled();
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    std::optional<DccKeys> scanned;
    const auto scanKeys = [&] {
        if (scanned.has_value()) return *scanned;
        if (resource.dccAddress == 0) {
            scanned = DccKeys::Uncompressed;
            return *scanned;
        }
        const auto scanStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        auto& memo = sampledKeyProofSlot(resource.dccAddress, guestBytes, resource.format, resource.dccAlphaOnMsb);
        if (memo.dccAddress != resource.dccAddress || memo.guestBytes != guestBytes || memo.format != resource.format || memo.dccAlphaOnMsb != resource.dccAlphaOnMsb) {
            memo.dccAddress = resource.dccAddress;
            memo.guestBytes = guestBytes;
            memo.format = resource.format;
            memo.dccAlphaOnMsb = resource.dccAlphaOnMsb;
            memo.proof = {};
        }
        const auto scans = KeyProofCounts().scanned;
        const auto proofs = KeyProofCounts().proved;
        scanned = ProvedClearKeys(resource, guestBytes, memo.proof);
        if (KeyProofCounts().scanned != scans) ++sampledLookupKeyScans;
        if (KeyProofCounts().proved != proofs) ++sampledLookupKeyProofs;
        if (profile) LookupOutcomes::Add(LookupOutcomes::DccScan, scanStart);
        return *scanned;
    };
    auto& counters = TextureCounts();
    const auto address = resource.baseAddress;
    const auto bytes = static_cast<std::size_t>(guestBytes);
    static_cast<void>(FlushDepthPlanes(address, bytes));
    // A storage image whose results for this surface (or for a mip chain containing it) are still on
    // the GPU supplies the texture by a view of it; anything else needs those results in guest
    // memory first.
    auto lookup = pendingLookupSource(context, resource, guestBytes, depthCompare, layerRows);
    const bool depthCopy = lookup.depthCopy;
    const bool srgbExpand = lookup.srgbExpand;
    auto source = std::move(lookup.source);
    const bool blockCopy = lookup.blockCopy;
    std::optional<DccKeys> keys;
    bool clearThroughKeys = false;
    if (source != nullptr && resource.dccAddress != 0 && IsDccClear(source->FilledKeys())) {
        keys = scanKeys();
        if (*keys == source->FilledKeys()) {
            std::array<std::byte, 16> probe{};
            if (!FillDccClear(DccClearTextureFormat(resource.format), *keys, resource.dccAlphaOnMsb, probe)) {
                char text[256];
                std::snprintf(text, sizeof(text), "AGC graphics: sampled texture 0x%llx (%ux%u format %u, dcc 0x%llx) reads %s DCC keys filled over its pending image, a clear value the format has no encoding for", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, static_cast<unsigned long long>(resource.dccAddress), DccKeysName(*keys));
                throw std::runtime_error(text);
            }
            static const bool traceKeys = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
            if (traceKeys) std::fprintf(stderr, "[dcc-keys] sampled 0x%llx through keys 0x%llx reads the %s fill, not the pending image\n", static_cast<unsigned long long>(address), static_cast<unsigned long long>(resource.dccAddress), DccKeysName(*keys));
            source.reset();
            clearThroughKeys = true;
        }
    }
    if (source != nullptr && resource.dccAddress != 0 && (keys.has_value() ? *keys : scanKeys()) == DccKeys::ClearSingle) source.reset();
    if (source != nullptr && resource.dccAddress != 0 && source->Descriptor().dccAddress != resource.dccAddress && (keys.has_value() ? IsDccClear(*keys) : IsDccClear(scanKeys()))) source.reset();

    // Otherwise a surface in host-imported memory is viewed through its cached storage image (made
    // here when there is none): its refresh after a CPU or GPU write is a GPU-direct detile from the
    // import, recorded behind the producer, so no bytes are read or compared on the CPU and nothing
    // waits for the producer. A fast-cleared surface (keys) is viewed only through an image whose own
    // descriptor carries the DCC address (below); it stays a snapshot otherwise, its texels not read.
    if ((!depthCompare || depthCopy) && !layerRows && source == nullptr && !clearThroughKeys && SampledFromStorageEligible(context, resource, guestBytes)) {

        keys = scanKeys();
        if (*keys == DccKeys::Uncompressed) {
            source = sampledStorageSource(context, resource, guestBytes);
            if (source != nullptr && depthCompare && !Texture::CanDepthCopyFrom(*source, resource)) source.reset();
            if (source != nullptr && srgbExpand && !Texture::CanSrgbExpandFrom(context, *source, resource)) source.reset();
        } else if (!depthCompare && ClearedViewEnabled() && StorageClearAvailable(context, resource.format, *keys)) {
            // A fast-cleared surface is viewed as well, through its image cleared on the GPU (the
            // lookup's Refresh, see StorageTexture::upload), when the image's own descriptor names
            // the same DCC metadata so the refresh sees the keys. A snapshot of the clear texels
            // (filled and uploaded on the CPU, then replaced by a view once results are pending: two
            // 4K uploads per clear at the movie stage) serves the surface otherwise, as before.
            auto candidate = sampledStorageSource(context, resource, guestBytes);
            if (candidate != nullptr && candidate->Descriptor().dccAddress == resource.dccAddress) source = std::move(candidate);
        }
        scanned.reset();
        // The lookup's Refresh may have flushed another image's results over the memory (which
        // marks the keys uncompressed): a surface that stays a snapshot re-reads them behind that
        // flush, as it does behind its own. APS5_NO_KEYS_RESCAN=1 keeps the keys scanned above.
        if (source != nullptr && IsRepackedTextureFormat(resource.format) && !Texture::CanRepackCopyFrom(*source, resource)) source.reset();
        if (source != nullptr && !depthCompare && !srgbExpand && !IsRepackedTextureFormat(resource.format) && !Texture::CanCopyFrom(*source, resource)) source.reset();
        static const bool rescan = std::getenv("APS5_NO_KEYS_RESCAN") == nullptr;
        if (rescan && source == nullptr) keys.reset();
    }
    // Pages of the surface written since they were last collected are stamped now, before any
    // UnchangedSince (here and on a cache hit) looks at them: the checks only compare stamped blocks,
    // so a write the game made since the last walk is invisible until a collect stamps its page. The
    // walk is memoized per packet, so a texture viewed from a storage image repeats it for free.
    auto generation = GuestMemory::CollectWrites(address, bytes);
    if (source != nullptr && !GuestMemory::UnchangedSince(address, bytes, source->GenerationFor(address, bytes))) {
        // The CPU wrote the memory while results were pending: Refresh merges them the usual way.
        refreshForView(*source, resource);
        scanned.reset();
    }
    // Results stored to guest memory just now are stamped newer than `generation`; a snapshot read
    // after them is current at the generation of a second (memoized) collect. The store marks the
    // surface's DCC keys uncompressed, so the keys are read after it.
    const auto flushStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool stagedClear = source != nullptr && source->AnyLayerPending() && resource.dccAddress != 0 && source->Descriptor().dccAddress == resource.dccAddress && [&] {
        const auto observedKeys = keys.has_value() ? *keys : scanKeys();
        return IsDccClear(observedKeys) || observedKeys == DccKeys::ClearSingle;
    }();
    bool stagedFlushed = false;
    const bool flushAsked = (source == nullptr || stagedClear) && !clearThroughKeys;
    if (flushAsked && StorageTexture::FlushPending(address, bytes, nullptr, "sampled texture")) {
        if (profile) LookupOutcomes::Add(LookupOutcomes::PendingFlush, flushStart);
        generation = GuestMemory::CollectWrites(address, bytes);
        if (stagedClear) {
            stagedFlushed = true;
            keys = DccKeys::Uncompressed;
        } else {
            keys.reset();
        }
        if (stagedFlushed) source.reset();
    }
    if (flushAsked) scanned.reset();
    if (!keys.has_value()) keys = scanKeys();
    const bool slabbed = source == nullptr && generation == 0 && *keys == DccKeys::Uncompressed && resource.dccAddress == 0 && !depthCompare && !srgbExpand && resource.dimension == TextureDimension::k3D;
    if (disabled) {
        if (source != nullptr && depthCompare) {
            auto texture = std::make_shared<Texture>(context, source, resource, components, Texture::DepthCopyTag{});
            texture->RefreshDepthCopy();
            return texture;
        }
        if (source != nullptr && blockCopy) {
            auto texture = std::make_shared<Texture>(context, source, resource, components, Texture::BlockCopyTag{});
            texture->RefreshDepthCopy();
            return texture;
        }
        if (source != nullptr && IsRepackedTextureFormat(resource.format)) {
            auto texture = std::make_shared<Texture>(context, source, resource, components, Texture::RepackCopyTag{});
            texture->RefreshDepthCopy();
            return texture;
        }
        if (source != nullptr && srgbExpand) {
            auto texture = std::make_shared<Texture>(context, source, resource, components, Texture::SrgbExpandTag{});
            texture->RefreshDepthCopy();
            return texture;
        }
        if (source != nullptr) return std::make_shared<Texture>(context, source, resource, components);
        if (const auto clear = SampledClearColor(context, resource, *keys, depthCompare)) return std::make_shared<Texture>(context, *context.detiler, resource, components, std::span<const std::byte>{}, false, layerRows, &*clear);
        const auto slabs = slabbed ? committedSlabs(resource, guestBytes, std::nullopt) : std::nullopt;
        if (slabs.has_value()) reportCommittedSlabs(resource, *slabs, guestBytes);
        std::vector<std::byte> snapshot(snapshotBytes(slabs, bytes));
        readSnapshot(resource, *keys, slabs, snapshot);
        return std::make_shared<Texture>(context, *context.detiler, resource, components, snapshot, depthCompare, layerRows, nullptr, packedSlabs(slabs));
    }
    auto snapshotGeneration = generation;
    std::optional<std::vector<std::pair<std::uint64_t, std::uint64_t>>> committed;
    if (source == nullptr && generation == 0) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        if (const auto partial = GuestMemory::CollectCommittedWrites(address, bytes, ranges); partial != 0) {
            snapshotGeneration = partial;
            committed = std::move(ranges);
        }
    }
    const auto untracked = source == nullptr && generation == 0 && !committed.has_value() && *keys == DccKeys::Uncompressed ? untrackedKey(address, bytes) : std::array<std::uint64_t, 4>{};
    auto slabs = slabbed ? committedSlabs(resource, guestBytes, committed) : std::nullopt;
    const auto snapshotSize = snapshotBytes(slabs, bytes);
    auto& cache = Textures();
    const auto key = MakeTextureKey(context.device, words, components, depthCompare);
    std::lock_guard lock(cache.mutex);
    const CacheHold held(sampledCacheHeld);
    dropUncachedViews(cache);
    std::vector<std::byte> recycled;
    if (auto it = findTexture(cache, key); it != cache.entries.end()) {
        if (it->source != nullptr) {
            // The view follows the storage image, whatever the GPU wrote to it since; guest memory
            // written meanwhile is taken in by refreshing the image. A fast clear the image cannot
            // see (keys, and no pending results to prefer) ends the view: a snapshot holds the clear.
            if (!stagedFlushed && (source == nullptr || source == it->source) && (source != nullptr || *keys == DccKeys::Uncompressed) && StorageImageServesKeys(*it->source, resource.dccAddress)) {
                if (!GuestMemory::UnchangedSince(address, bytes, it->source->GenerationFor(address, bytes))) refreshForView(*it->source, resource);
                if (it->texture->IsDepthCopy()) it->texture->RefreshDepthCopy();
                it->keys = *keys;
                touchTexture(cache, it);
                logLookup({it->texture.get(), resource, guestBytes, *keys, 0, it->source.get()});
                reportTextureCounters();
                if (profile) LookupOutcomes::Add(*keys != DccKeys::Uncompressed ? LookupOutcomes::SampledHitClearedView : LookupOutcomes::SampledHitView, start);
                return it->texture;
            }
        } else if (source == nullptr && it->surfaceBytes == guestBytes && it->keys == *keys && it->slabs == slabs && stillCommitted(slabs, it->slabs)) {
            // Unwritten pages need no comparison; partially resident textures compare only committed
            // pages. The compare goes through the flush hook: it waits for recorded work over the
            // surface (counted, and named for the [hooksync] line).
            const auto equalsCommitted = [&] {
                if (it->slabs.has_value() ? it->bytes.size() != snapshotBytes(it->slabs, bytes) : it->bytes.empty()) return false;
                if (Recorder::SnapshotWriteOverlaps(address, bytes)) counters.pendingReads.fetch_add(1, std::memory_order_relaxed);
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureCompare);
                return it->slabs.has_value() ? equalsCommittedSlabs(address, *it->slabs, it->bytes) : GuestMemory::EqualsCommitted(address, it->bytes);
            };
            const auto unchanged = [&] {
                if (it->committed != committed) return false;
                return committed.has_value() ? !Recorder::SnapshotWriteOverlaps(address, static_cast<std::size_t>(it->surfaceBytes)) && GuestMemory::UnchangedCommittedSince(*committed, it->generation) : GuestMemory::UnchangedSince(address, static_cast<std::size_t>(it->surfaceBytes), it->generation);
            };
            const auto kept = [&] {
                if (untracked[0] == 0) return false;
                const bool same = it->untracked == untracked && !Recorder::SnapshotWriteOverlaps(address, bytes);
                (same ? counters.untrackedKept : counters.untrackedCompared).fetch_add(1, std::memory_order_relaxed);
                return same;
            };
            if ((*keys != DccKeys::Uncompressed && *keys != DccKeys::ClearSingle) || unchanged() || kept() || equalsCommitted()) {
                it->generation = snapshotGeneration;
                it->committed = std::move(committed);
                it->untracked = untracked;
                touchTexture(cache, it);
                LookupRecord record{it->texture.get(), resource, guestBytes, *keys, generation, nullptr};
                if (generation == 0 && it->committed.has_value()) {
                    record.committedGeneration = snapshotGeneration;
                    record.committed = *it->committed;
                }
                logLookup(record);
                reportTextureCounters();
                if (profile) LookupOutcomes::Add(LookupOutcomes::SampledHitSnapshot, start);
                return it->texture;
            }
        }
        if (it->bytes.size() == snapshotSize) recycled = std::move(it->bytes);
        else parkSnapshot(cache, it->bytes);
        eraseTexture(cache, it);
        ++cache.counts.stale;
    }
    const auto clear = source == nullptr ? SampledClearColor(context, resource, *keys, depthCompare) : std::nullopt;
    static const bool sampledImport = std::getenv("APS5_NO_SAMPLED_IMPORT") == nullptr;
    const bool gpuWritten = sampledImport && source == nullptr && !clear && !depthCompare && !layerRows && !slabs.has_value() && *keys == DccKeys::Uncompressed && Recorder::SnapshotWriteOverlaps(address, bytes);
    const HostImport* gpuSource = gpuWritten ? ExistingHostImportFor(context, address, bytes) : nullptr;
    if (source != nullptr || clear.has_value() || gpuSource != nullptr) parkSnapshot(cache, recycled);
    CachedTexture entry{key, address, std::vector<std::byte>{}, nullptr, *keys, snapshotGeneration};
    entry.committed = std::move(committed);
    entry.surfaceBytes = bytes;
    entry.slabs = std::move(slabs);
    const auto createTexture = [&](auto&&... args) {
        const CacheHold reclaimable(sampledCacheReclaimable);
        return std::make_shared<Texture>(context, std::forward<decltype(args)>(args)...);
    };
    if (source != nullptr) {
        entry.source = source;
        entry.sourceVersion = source->Version();
        if (depthCompare) {
            entry.texture = createTexture(source, resource, components, Texture::DepthCopyTag{});
            entry.texture->RefreshDepthCopy();
        } else if (blockCopy) {
            entry.texture = createTexture(source, resource, components, Texture::BlockCopyTag{});
            entry.texture->RefreshDepthCopy();
        } else if (IsRepackedTextureFormat(resource.format)) {
            entry.texture = createTexture(source, resource, components, Texture::RepackCopyTag{});
            entry.texture->RefreshDepthCopy();
        } else if (srgbExpand) {
            entry.texture = createTexture(source, resource, components, Texture::SrgbExpandTag{});
            entry.texture->RefreshDepthCopy();
        } else {
            entry.texture = createTexture(source, resource, components);
        }
        entry.accounted = source->Cached() ? 0u : heldBytes(*source);
        counters.fromStorage.fetch_add(1, std::memory_order_relaxed);
    } else if (clear) {
        entry.texture = createTexture(*context.detiler, resource, components, std::span<const std::byte>{}, false, layerRows, &*clear);
        entry.accounted = entry.texture->AllocationBytes();
        counters.cleared.fetch_add(1, std::memory_order_relaxed);
    } else if (gpuSource != nullptr) {
        entry.texture = createTexture(*context.detiler, resource, components, std::span<const std::byte>{}, false, false, nullptr, std::nullopt, gpuSource);
        entry.accounted = entry.texture->AllocationBytes();
        counters.imported.fetch_add(1, std::memory_order_relaxed);
    } else {
        // Snapshot before the upload so a write racing with it is caught by the next comparison.
        const bool pending = *keys == DccKeys::Uncompressed && Recorder::SnapshotWriteOverlaps(address, bytes);
        if (pending) counters.pendingReads.fetch_add(1, std::memory_order_relaxed);
        if (entry.slabs.has_value()) reportCommittedSlabs(resource, *entry.slabs, guestBytes);
        constexpr std::size_t SnapshotCompareBytes = 256u << 10u;
        const bool dropped = *keys == DccKeys::Uncompressed && snapshotSize > SnapshotCompareBytes && (entry.generation != 0 || (!pending && untracked[0] != 0));
        std::shared_ptr<Buffer> staging;
        if (dropped) {
            parkSnapshot(cache, recycled);
            expireSpareSnapshots(cache, Recorder::Presents());
            staging = std::make_shared<Buffer>(context, snapshotSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        } else {
            if (recycled.empty()) recycled = spareSnapshot(cache, snapshotSize);
            entry.bytes = recycled.empty() ? std::vector<std::byte>(snapshotSize) : std::move(recycled);
        }
        const auto read = staging != nullptr ? staging->Bytes() : std::span<std::byte>(entry.bytes);
        readSnapshot(resource, *keys, entry.slabs, read);
        if (!pending) entry.untracked = untracked;
        static const bool traceTextures = std::getenv("APS5_TRACE_TEXTURES") != nullptr;
        if (traceTextures) {
            std::size_t nonzero = 0;
            for (std::size_t i = 0; i < read.size(); i += 64) nonzero += read[i] != std::byte{0};
            std::fprintf(stderr, "[texture] 0x%llx %ux%u format %u tile %d: %zu of %zu sampled bytes nonzero\n", static_cast<unsigned long long>(resource.baseAddress), resource.width, resource.height, resource.format, static_cast<int>(resource.tileMode), nonzero, read.size() / 64);
        }
        if (staging != nullptr) entry.texture = createTexture(*context.detiler, resource, components, std::move(staging), depthCompare, layerRows, packedSlabs(entry.slabs));
        else entry.texture = createTexture(*context.detiler, resource, components, entry.bytes, depthCompare, layerRows, nullptr, packedSlabs(entry.slabs));
        entry.accounted = entry.texture->AllocationBytes();
        if (entry.bytes.size() > SnapshotCompareBytes && (entry.generation != 0 || entry.untracked[0] != 0)) parkSnapshot(cache, entry.bytes);
        counters.snapshots.fetch_add(1, std::memory_order_relaxed);
    }
    const auto budget = sampledBudget(context, cache);
    auto texture = entry.texture;
    const auto madeRecord = [&] {
        LookupRecord record{texture.get(), resource, guestBytes, *keys, generation, source.get()};
        if (source == nullptr && generation == 0 && entry.committed.has_value()) {
            record.committedGeneration = entry.generation;
            record.committed = *entry.committed;
        }
        return record;
    };
    if (!admitTexture(cache, key, entry.accounted, budget)) {
        noteReleased(texture, resource.baseAddress);
        logLookup(madeRecord());
        reportCacheCounts(cache, budget);
        reportTextureCounters();
        if (profile) LookupOutcomes::Add(source != nullptr ? LookupOutcomes::SampledMadeView : LookupOutcomes::SampledMadeSnapshot, start);
        return texture;
    }
    cache.bytes += entry.accounted;
    entry.present = Recorder::Presents();
    ++cache.counts.inserted;
    logLookup(madeRecord());
    cache.entries.push_front(std::move(entry));
    cache.index[key] = cache.entries.begin();
    if (source != nullptr) cache.views.emplace(source.get(), cache.entries.begin());
    reportCacheCounts(cache, budget);
    reportTextureCounters();
    if (profile) LookupOutcomes::Add(source != nullptr ? LookupOutcomes::SampledMadeView : LookupOutcomes::SampledMadeSnapshot, start);
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
    std::uint64_t accounted = 0;
};

// As TextureCache: use order with a hash index by key, plus one by image for StorageImageCached.
struct StorageTextureCache {
    AgcDriver::Mutex mutex;
    std::list<CachedStorageTexture> entries;
    std::unordered_map<StorageKey, std::list<CachedStorageTexture>::iterator, StorageKeyHash> index;
    std::unordered_map<const StorageTexture*, std::list<CachedStorageTexture>::iterator> byImage;
    std::atomic<std::uint64_t> bytes{0};
};

}
}
