#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/PrimedSharedMutex.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/FirstSampleCopy.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuardSpans.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/DepthToStorage_spv.h"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <shared_mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
bool DepthSurfaceHolds(const Context& context, const GuestTextureResource& resource, const Texture* texture) {
    if ((resource.dimension != TextureDimension::k2D && resource.dimension != TextureDimension::k2DMsaa) || resource.baseArray != 0) return false;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    const auto address = resource.baseAddress;
    settlePendingHtileClear(context, address);
    std::lock_guard lock(surfacesMutex());
    auto* found = planeSurface(context.device, address);
    if (found == nullptr || !found->Holds(texture)) return false;
    if (!depthCopySourceActive) found->CheckCurrent();
    found->CheckResource(resource);
    return true;
}

void RequireNoNativeDepthFeedback_nid_no_patch(const Context& context, const DepthTarget& target, std::span<const std::shared_ptr<Texture>> sampled) {
    if (target.samples == 1u || sampled.empty()) return;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        if (surface->context.device != context.device || !holdsTarget(surface->target, target)) continue;
        Require(std::none_of(sampled.begin(), sampled.end(), [&](const auto& texture) { return texture != nullptr && surface->Holds(texture.get()); }), "sampling a native multisampled depth target the draw attaches is not implemented");
    }
}

VkImageAspectFlags HtileFillClears(std::uint32_t pattern, bool stencilInHtile) {
    VkImageAspectFlags cleared = (pattern & 0xfu) == 0 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0u;
    if (stencilInHtile && ((pattern >> 8u) & 0x3u) == 0) cleared |= VK_IMAGE_ASPECT_STENCIL_BIT;
    return cleared;
}

std::uint64_t HtileMetadataBytes(VkExtent2D extent) {
    const bool gcn = ShaderRecompiler::GcnGuestShaders();
    const std::uint32_t blockWidth = gcn ? 512u : 1024u;
    const auto blocks = static_cast<std::uint64_t>((extent.width + blockWidth - 1u) / blockWidth) * ((extent.height + 511u) / 512u);
    return blocks * (gcn ? 16384u : 32768u);
}

bool HtileFillCovers(std::uint64_t htile, VkExtent2D extent, std::uint64_t address, std::size_t bytes) {
    if (htile == 0 || htile < address || htile >= address + bytes) return false;
    return address + bytes - htile >= HtileMetadataBytes(extent);
}

bool PlaneFillCovers(std::uint64_t plane, std::uint64_t planeBytes, std::uint64_t address, std::size_t bytes) {
    return plane != 0 && plane >= address && plane + planeBytes <= address + bytes;
}

std::optional<std::uint8_t> StencilFillValue(std::uint32_t pattern) {
    const auto byte = static_cast<std::uint8_t>(pattern & 0xffu);
    if (pattern != byte * 0x01010101u) return std::nullopt;
    return byte;
}

std::optional<float> DepthFillValue(std::uint32_t pattern, bool d16) {
    if (d16) {
        if ((pattern >> 16u) != (pattern & 0xffffu)) return std::nullopt;
        return static_cast<float>(pattern & 0xffffu) / 65535.0f;
    }
    const auto value = std::bit_cast<float>(pattern);
    if (!(value >= 0.0f && value <= 1.0f)) return std::nullopt;
    return value;
}

namespace {

std::optional<float> planeFillDepth(const DepthTarget& target, std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    if (target.samples != 1u || HtileFillCovers(target.htileAddress, target.extent, address, bytes)) return std::nullopt;
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    if (!PlaneFillCovers(target.address, DepthSliceBytes(target.extent, d16 ? 2u : 4u), address, bytes)) return std::nullopt;
    return DepthFillValue(pattern, d16);
}

}

void NoteDepthMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    forgetCopiesInside(address, bytes);
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    forgetHtileReads(address, bytes);
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire)) continue;
        const auto& target = surface->target;
        const bool touched = std::any_of(surface->sampling.initial.begin(), surface->sampling.initial.end(), [&](const auto& range) { return address < range.address + range.bytes && range.address < address + bytes; });
        if (touched || surface->Overlaps(address, bytes) || (target.htileAddress != 0u && address < target.htileAddress + HtileMetadataBytes(target.extent) && target.htileAddress < address + bytes)) surface->expanded = false;
        if (surface->samplingActive && target.stencilAddress != 0u && !target.htileStencil && target.htileDepthOnly && surface->sampling.initial.size() > 2u) {
            const auto& plane = surface->sampling.initial[2];
            if (address < plane.address + plane.bytes && plane.address < address + bytes) {
                const auto value = StencilFillValue(pattern);
                if (!PlaneFillCovers(plane.address, plane.bytes, address, bytes)) surface->sampling.Refuse("native multisample stencil plane partial fill transition is not implemented");
                else if (!value.has_value()) surface->sampling.Refuse("native multisample stencil plane fill of more than one byte value is not implemented");
                else if (!GuestMemory::Accessible(reinterpret_cast<const void*>(plane.address), plane.bytes) || !GuestMemory::Watched(plane.address, plane.bytes)) surface->sampling.Refuse("native multisample stencil plane fill over unwatched memory is not implemented");
                else {
                    const auto* recorder = RecorderFor(surface->context);
                    const auto refill = surface->stencilClearGeneration != 0u && surface->stencilClearValue == value && surface->stencilFillNote == 0u && recorder != nullptr ? fillNoteAfter(*recorder, plane, surface->stencilFillFloor, *value * 0x01010101u) : 0u;
                    const auto priorFloor = surface->stencilFillFloor;
                    if (refill != 0u) surface->stencilFillNote = refill;
                    else {
                        surface->stencilClearGeneration = GuestMemory::CollectWritesUncached(plane.address, plane.bytes);
                        surface->stencilClearValue = *value;
                        surface->stencilFillNote = recorder != nullptr ? newestFillNote(*recorder, plane, priorFloor, *value * 0x01010101u) : 0u;
                        surface->stencilFillFloor = recorder != nullptr ? recorder->NewestWriteNote(plane.address, plane.bytes) : 0u;
                        surface->stencilStored = GuestMemory::StoredAllSince_nid_no_patch(plane.address, plane.bytes, plane.generation);
                    }
                }
            }
        }
        const bool overlaps = target.samples != 1u && target.htileAddress != 0u && address < target.htileAddress + HtileMetadataBytes(target.extent) && target.htileAddress < address + bytes;
        if (overlaps && surface->samplingActive && HtileFillCovers(target.htileAddress, target.extent, address, bytes)) {
            const auto& metadata = surface->sampling.initial[1];
            if (!GuestMemory::Accessible(reinterpret_cast<const void*>(metadata.address), metadata.bytes) || !GuestMemory::Watched(metadata.address, metadata.bytes)) surface->sampling.Refuse("native multisample depth metadata fill over unwatched memory is not implemented");
            else {
                const auto* recorder = RecorderFor(surface->context);
                const auto refill = surface->clearGeneration != 0u && surface->clearPattern == pattern && surface->clearFillNote == 0u && recorder != nullptr ? fillNoteAfter(*recorder, metadata, surface->clearFillFloor, pattern) : 0u;
                const auto priorFloor = surface->clearFillFloor;
                if (surface->clearPending || (surface->clearGeneration != 0u && !surface->clearPattern.has_value())) {
                    surface->clearPending = false;
                    surface->clearCompletionQueued = false;
                    surface->clearGeneration = 0u;
                    surface->clearAspects = 0u;
                    surface->clearFillNote = 0u;
                    surface->clearFillFloor = 0u;
                    surface->clearStored = false;
                }
                if (refill != 0u) surface->clearFillNote = refill;
                else {
                    surface->clearGeneration = GuestMemory::CollectWritesUncached(metadata.address, metadata.bytes);
                    surface->clearAspects = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
                    surface->clearPattern = pattern;
                    surface->clearFillNote = recorder != nullptr ? newestFillNote(*recorder, metadata, priorFloor, pattern) : 0u;
                    surface->clearFillFloor = recorder != nullptr ? recorder->NewestWriteNote(metadata.address, metadata.bytes) : 0u;
                    surface->clearStored = GuestMemory::StoredAllSince_nid_no_patch(metadata.address, metadata.bytes, metadata.generation);
                }
            }
            continue;
        }
        if (overlaps) surface->sampling.Refuse("native multisample depth metadata fill transition is not implemented");
        if (HtileFillCovers(target.htileAddress, target.extent, address, bytes)) {
            const VkImageAspectFlags written = VK_IMAGE_ASPECT_DEPTH_BIT | (target.htileStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
            surface->ForgetClear(written);
            surface->NoteHtileClear(HtileFillClears(pattern, target.htileStencil));
            continue;
        }
        if (target.samples != 1) continue;
        if (const auto depth = planeFillDepth(target, address, bytes, pattern)) surface->NotePlaneFill(VK_IMAGE_ASPECT_DEPTH_BIT, *depth, 0);
        if (PlaneFillCovers(target.stencilAddress, DepthSliceBytes(target.extent, 1u), address, bytes)) {
            if (const auto stencil = StencilFillValue(pattern)) surface->NotePlaneFill(VK_IMAGE_ASPECT_STENCIL_BIT, 0.0f, *stencil);
        }
    }
}

std::size_t DiscardFilledDepthPlanes(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    if (bytes == 0 || !overlapsStalePlane(address, address + bytes)) return 0;
    std::lock_guard lock(surfacesMutex());
    std::size_t discarded = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> remaining;
    for (const auto& surface : surfaces()) {
        if (surface->stencilStale) remaining.push_back(staleStencilRange(*surface));
        if (!surface->AnyStale()) continue;
        const auto& target = surface->target;
        const auto plane = stalePlaneRange(*surface);
        const auto depth = surface->active->load(std::memory_order_acquire) ? planeFillDepth(target, address, bytes, pattern) : std::nullopt;
        if (!surface->syncing && depth.has_value() && PlaneFillCovers(plane.first, plane.second - plane.first, address, bytes)) {
            surface->ClearStale();
            surface->NotePlaneFill(VK_IMAGE_ASPECT_DEPTH_BIT, *depth, 0);
            ++discarded;
        }
        if (surface->AnyStale()) remaining.push_back(plane);
    }
    if (discarded == 0) return 0;
    auto& planes = stalePlanes();
    std::lock_guard staleLock(planes.mutex);
    planes.ranges = std::move(remaining);
    publishStaleHull(planes);
    return discarded;
}

void NoteDepthRenderControl(const DepthRenderControl& draw) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (!surface->active->load(std::memory_order_acquire) || surface->target.samples == 1u || surface->target.samples != draw.samples || surface->target.address != draw.address) continue;
        surface->renderControl = draw.control;
        const bool depthExpand = (draw.control & 0x40u) != 0u;
        const bool stencilExpand = (draw.control & 0x20u) != 0u;
        if (depthExpand && (stencilExpand || surface->target.stencilAddress == 0u)) surface->expanded = true;
        else if ((draw.control & 3u) != 0u) surface->expanded = false;
    }
}

bool DepthRenderControlDrains(const DepthRenderControl& draw) {
    if (draw.samples == 1u) return false;
    if ((draw.control & 0x60u) != 0u) return true;
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && surface->target.samples == draw.samples && surface->target.address == draw.address; });
}

bool DepthSurfaceAt(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource) {
    std::lock_guard lock(surfacesMutex());
    const auto* found = planeSurface(context.device, resource.baseAddress);
    if (found == nullptr) return false;
    const auto& target = found->target;
    if (resource.width != target.extent.width || resource.height != target.extent.height) return false;
    const bool stencil = target.stencilAddress != 0 && target.stencilAddress == resource.baseAddress;
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
    const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
    return ResolveTextureFormat(resource.format) == expected || depthBits;
}

namespace {

std::vector<LayeredSlice> layeredSlices(const Context& context, const LayeredDepthStorage& storage) {
    const auto& base = storage.base;
    std::vector<LayeredSlice> slices;
    const auto& list = surfaces();
    for (std::uint32_t layer = storage.firstLayer; layer < storage.layers; ++layer) {
        const auto address = base.address + static_cast<std::uint64_t>(layer) * storage.layerPitchBytes;
        const auto slice = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) { return surface->context.device == context.device && !surface->retired && surface->target.address == address && surface->target.extent.width == base.extent.width && surface->target.extent.height == base.extent.height && surface->target.format == base.format && surface->target.samples == 1; });
        if (slice != list.rend()) slices.push_back({slice->get(), layer});
    }
    return slices;
}

std::shared_ptr<StorageTexture> layeredDepthStorage(const Context& context, DepthSurface& surface, const GuestTextureResource& resource, std::uint32_t mip) {
    const auto& target = surface.target;
    const auto layers = resource.depthOrLastArray + 1u;
    const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto bytesPerTexel = d16 ? 2u : 4u;
    surface.RequireStorable(resource);
    Require(!DescribeSurface(resource).levelMajor, "depth planes of a PS4 layered surface with several mip levels (stored level by level) are not implemented");
    auto& entry = layeredStorages()[{context.device, resource.baseAddress, resource.width, resource.height, layers, resource.format, static_cast<int>(resource.tileMode), static_cast<int>(resource.dimension), resource.baseArray}];
    if (entry == nullptr) entry = std::make_unique<LayeredDepthStorage>(context, resource, target, bytesPerTexel, resource.baseArray, layers);
    else static_cast<void>(entry->storage->Refresh());
    const auto slices = layeredSlices(context, *entry);
    for (const auto& slice : slices) slice.surface->ApplyFastClear();
    entry->Copy(slices, true);
    return entry->storage;
}

}

std::shared_ptr<StorageTexture> DepthSurfaceStorage(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, std::uint32_t mip) {
    if (mip != 0 || resource.baseLevel != 0) return nullptr;
    std::lock_guard lock(surfacesMutex());
    const auto& list = surfaces();
    const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) {
        return surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && !surface->retired && (surface->target.address == resource.baseAddress || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == resource.baseAddress));
    });
    if (found == list.rend()) return nullptr;
    auto& surface = **found;
    const bool stencil = surface.target.stencilAddress != 0 && surface.target.stencilAddress == resource.baseAddress;
    if (stencil || resource.dimension != TextureDimension::k2DArray || (resource.baseArray == 0 && resource.depthOrLastArray == 0)) return surface.Storage(words, resource, mip);
    if (!surface.StorageViewMatches(words, resource)) return nullptr;
    return layeredDepthStorage(context, surface, resource, mip);
}

DepthCopySourceScope::DepthCopySourceScope(bool active) : previous(depthCopySourceActive) {
    depthCopySourceActive = active;
}

DepthCopySourceScope::~DepthCopySourceScope() {
    depthCopySourceActive = previous;
}

std::array<std::uint32_t, 8> DepthCopySourceWords(const DepthTarget& target, bool stencil) {
    const bool multisampled = target.samples != 1u;
    const auto base = ((stencil ? target.stencilAddress : target.address) | target.pipeBankXor) >> 8u;
    const auto metadata = multisampled && !stencil ? target.htileAddress >> 8u : 0u;
    const auto count = static_cast<std::uint32_t>(std::countr_zero(target.samples));
    const auto width = target.extent.width - 1u, height = target.extent.height - 1u;
    const auto type = multisampled ? 14u : 9u;
    const auto word6 = (multisampled && !stencil && target.iterate256 ? 1u << 10u : 0u) | (multisampled && !stencil && target.htilePipeAligned ? 1u << 19u : 0u) | (metadata != 0 ? 1u << 21u : 0u) | (static_cast<std::uint32_t>(metadata & 255u) << 24u);
    return {static_cast<std::uint32_t>(base), static_cast<std::uint32_t>((base >> 32u) & 255u) | ((stencil ? 5u : 22u) << 20u) | ((width & 3u) << 30u), (width >> 2u) | (height << 14u), (type << 28u) | (target.depthSwizzle << 20u) | (count << 16u) | 0x204u, 0u, count << 4u, word6, static_cast<std::uint32_t>(metadata >> 8u)};
}

bool DepthSurfaceStoreBack(const Context& context, const StorageTexture& storage) {
    std::lock_guard lock(surfacesMutex());
    for (auto& [key, entry] : layeredStorages()) {
        if (std::get<0>(key) != context.device || entry->storage.get() != &storage) continue;
        const auto slices = layeredSlices(context, *entry);
        entry->Copy(slices, false);
        for (const auto& slice : slices) {
            slice.surface->written = ++depthWrites;
            slice.surface->NoteHostWrite();
        }
        return true;
    }
    for (const auto& surface : surfaces()) {
        if (surface->active->load(std::memory_order_acquire) && surface->context.device == context.device && surface->StoreBack(storage)) return true;
    }
    for (auto surface = surfaces().rbegin(); surface != surfaces().rend(); ++surface) {
        if ((*surface)->context.device == context.device && (*surface)->StoreBackCovering(storage)) return true;
    }
    return false;
}

bool DepthSurfaceAt(std::uint64_t address) {
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && !surface->retired && (surface->target.address == address || surface->target.stencilAddress == address); });
}

bool DepthSurfaceOverlaps(VkDevice device, std::uint64_t address, std::uint64_t bytes) {
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->active->load(std::memory_order_acquire) && !surface->retired && surface->context.device == device && surface->Overlaps(address, bytes); });
}

void RetireDepthSurfaces(VkDevice device, std::uint64_t address, std::uint64_t bytes) {
    std::lock_guard gpu(GuestMemory::GpuMutex());
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device != device || surface->retired || !surface->Overlaps(address, bytes)) continue;
        const auto [stencilBegin, stencilEnd] = staleStencilRange(*surface);
        if (!(address <= stencilBegin && stencilEnd <= address + bytes)) storeStaleStencil(*surface);
        retire(*surface);
    }
}

namespace {

bool sixteenBitDepth(VkFormat format) {
    return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D16_UNORM_S8_UINT;
}

GuestTextureResource depthPlaneView(std::uint64_t address, VkExtent2D extent, VkFormat format) {
    GuestTextureResource plane{};
    plane.baseAddress = address;
    plane.width = extent.width;
    plane.height = extent.height;
    plane.mipCount = 1;
    plane.tileMode = TextureTileMode::kZ64KBX;
    plane.dimension = TextureDimension::k2D;
    plane.format = FindGuestColorTargetFormat(format, format == VK_FORMAT_R8_UINT ? 1u : format == VK_FORMAT_R16_UNORM ? 2u : 4u).value();
    plane.dstSelX = 4;
    plane.dstSelY = 5;
    plane.dstSelZ = 6;
    plane.dstSelW = 7;
    return plane;
}

void recordPlaneClear(const Context& context, const std::shared_ptr<StorageTexture>& plane, float depth) {
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "depth plane clears need an active recorder");
    const auto commands = recorder->Commands();
    recorder->Keep(plane);
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::FillClear);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkClearColorValue clear{};
    clear.float32[0] = depth;
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, plane->Image(), VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::FillClear, 2);
    recorder->EndGpuTiming(timing, plane->GuestBytes());
}

void recordPlaneCopy(const Context& context, const std::shared_ptr<StorageTexture>& source, const std::shared_ptr<StorageTexture>& destination, VkExtent2D extent) {
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "depth plane copies need an active recorder");
    const auto commands = recorder->Commands();
    recorder->Keep(source);
    recorder->Keep(destination);
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstSubresource = region.srcSubresource;
    region.extent = {extent.width, extent.height, 1};
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source->Image(), VK_IMAGE_LAYOUT_GENERAL, destination->Image(), VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
    recorder->EndGpuTiming(timing, destination->GuestBytes());
}

}

void RunDepthExpandPass(const Context& context, const DepthExpandPass& pass) {
    const auto& source = pass.source;
    const auto failure = [&](std::uint64_t destination, const char* what) {
        char text[288];
        std::snprintf(text, sizeof(text), "AGC graphics: an out-of-place DB expand from 0x%llx (%ux%u) to 0x%llx %s is not implemented", static_cast<unsigned long long>(source.address), source.extent.width, source.extent.height, static_cast<unsigned long long>(destination), what);
        return std::runtime_error(text);
    };
    {
        std::lock_guard lock(surfacesMutex());
        for (const auto address : {pass.destination, pass.stencilDestination}) {
            if (address != 0 && planeSurface(context.device, address) != nullptr) throw failure(address, "into a live depth surface");
        }
    }
    const bool d16 = sixteenBitDepth(source.format);
    expandsRun.fetch_add(1, std::memory_order_relaxed);
    bool copied = false;
    for (const bool stencil : {false, true}) {
        const auto address = stencil ? pass.stencilDestination : pass.destination;
        if (address == 0) continue;
        const auto planeFormat = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const auto destination = CachedStorageSurface(context, depthPlaneView(address, source.extent, planeFormat));
        if (destination == nullptr) throw failure(address, "without a storage image of the write base");
        const ExpandKey key{context.device, address};
        enum class Read { Surface, Clear, GuestBytes } read = Read::Surface;
        ExpandWrite wrote;
        {
            std::lock_guard lock(surfacesMutex());
            const auto owner = depthOwners().find({context.device, source.address});
            if (owner != depthOwners().end()) {
                auto* surface = owner->second.surface;
                const auto& target = surface->target;
                if (sixteenBitDepth(target.format) != d16 || surface->hostFormat != target.format || target.samples != 1 || target.extent.width != source.extent.width || target.extent.height != source.extent.height) throw failure(address, "from a depth surface of another extent, format or sample count");
                if (stencil && target.stencilAddress != source.stencilAddress) throw failure(address, "of a stencil plane its depth surface does not hold");
                surface->ApplyFastClear();
                const auto last = expandWrites().find(key);
                if (last != expandWrites().end() && last->second.source == surface && last->second.written == surface->written && last->second.destination.lock() == destination && last->second.version == destination->Version()) continue;
                if (stencil) surface->CopyStencilTo(destination);
                else surface->CopyDepthTo(destination);
                wrote.source = surface;
                wrote.written = surface->written;
            } else {
                read = !stencil && findClearedHtile(source) != clearedHtiles().end() ? Read::Clear : Read::GuestBytes;
            }
        }
        if (read == Read::Clear) recordPlaneClear(context, destination, source.clearDepth);
        if (read == Read::GuestBytes) {
            const auto plane = CachedStorageSurface(context, depthPlaneView(stencil ? source.stencilAddress : source.address, source.extent, planeFormat));
            if (plane == nullptr) throw failure(address, "without a storage image of the read base");
            recordPlaneCopy(context, plane, destination, source.extent);
        }
        destination->MarkDirty();
        wrote.destination = destination;
        wrote.version = destination->Version();
        {
            std::lock_guard lock(surfacesMutex());
            expandWrites()[key] = std::move(wrote);
            unstoredExpands()[key] = static_cast<std::size_t>(destination->GuestBytes());
        }
        copied = true;
        static bool reported[2][3] = {};
        if (!std::exchange(reported[stencil ? 1 : 0][static_cast<int>(read)], true)) {
            const auto from = static_cast<unsigned long long>(stencil ? source.stencilAddress : source.address);
            const auto written = static_cast<unsigned long long>(address);
            if (read == Read::Surface) std::fprintf(stderr, "[gpu] DB expand: the %s plane of 0x%llx (%ux%u) is written at 0x%llx\n", stencil ? "stencil" : "depth", from, source.extent.width, source.extent.height, written);
            else std::fprintf(stderr, "[gpu] DB expand: 0x%llx (%ux%u) has no depth surface; %s written at 0x%llx\n", from, source.extent.width, source.extent.height, read == Read::Clear ? "DB_DEPTH_CLEAR (an HTILE clear is pending) is" : "its guest bytes are", written);
        }
    }
    if (!copied) expandsSkipped.fetch_add(1, std::memory_order_relaxed);
}

}
}
}
}
