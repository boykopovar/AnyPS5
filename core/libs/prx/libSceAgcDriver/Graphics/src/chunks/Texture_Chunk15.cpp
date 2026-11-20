#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlimMutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
// For the declaration of PendingStorageOverlaps, defined below.
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ShaderCacheDirectory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/KeptUnits.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include <algorithm>
#include <filesystem>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>
#include <limits>
#include <exception>
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#if defined(__APPLE__)
#include <unistd.h>
#endif

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
void StorageTexture::WriteBack() {
    storeTrappedOverlaps("explicit");
    const auto previous = std::exchange(flushReason, "explicit");
    writeBack(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    flushReason = previous;
}

std::shared_ptr<StorageTexture> StorageTexture::FindLive(std::uint64_t address, std::uint64_t bytes) {
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    for (auto it = live.textures.rbegin(); it != live.textures.rend(); ++it) {
        auto* texture = *it;
        if (texture->released || !texture->Cached() || texture->descriptor.baseAddress != address || texture->guestBytes != bytes) continue;
        return texture->weak_from_this().lock();
    }
    return nullptr;
}

bool StorageTexture::SameSurfaceShape(const StorageTexture& other) const {
    const auto& mine = descriptor;
    const auto& theirs = other.descriptor;
    return guestBytes == other.guestBytes && mine.width == theirs.width && mine.height == theirs.height && mine.linearPitch == theirs.linearPitch && mine.depthOrLastArray == theirs.depthOrLastArray && mine.baseArray == theirs.baseArray && mine.mipCount == theirs.mipCount && mine.tileMode == theirs.tileMode && mine.dimension == theirs.dimension && mine.format == theirs.format && storageFormat == other.storageFormat && geometry.imageLayers == other.geometry.imageLayers && geometry.imageDepth == other.geometry.imageDepth;
}

bool StorageTexture::ImageCoversGuestBytes() const {
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> whole{{{0, guestBytes}}};
    return uncoveredBytes(whole).empty();
}

const char* StorageTexture::CopyRefusal(const Context& context, const StorageTexture* destination, const StorageTexture& source, std::uint64_t address) {
    if (destination != nullptr && (destination == &source || !destination->SameSurfaceShape(source))) return "shape";
    if (source.descriptor.dccAddress != 0 || (destination != nullptr && destination->descriptor.dccAddress != 0)) return "keys";
    if (source.released || !source.Cached() || (destination != nullptr && (destination->released || !destination->Cached()))) return "released";
    if (HostImportFor(context, address, static_cast<std::size_t>(source.guestBytes)) == nullptr) return "not imported";
    if (Recorder::Active() == nullptr) return "no recorder";
    return nullptr;
}

bool StorageTexture::CopyFrom(StorageTexture& source, const char*& refusal) {
    refusal = CopyRefusal(context, this, source, descriptor.baseAddress);
    if (refusal != nullptr) return false;
    auto* recorder = Recorder::Active();
    // The source image must hold the surface: a stale one uploads first (and other images'
    // results pending over its memory land first, as the transfer's flush would have them).
    source.Refresh();
    // Results pending over the destination range from before the copy are dead inside it and land
    // first around it; other images over the range hold content the copy supersedes (they
    // re-upload at their next use, behind this image's write-back).
    static_cast<void>(DiscardPendingInside(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)));
    FlushPending(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), this, "buffer copy destination", PublishScope::PartialUnits);
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        for (auto* texture : live.textures) {
            if (texture != this && !texture->released && texture->overlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) texture->forgetSavedOver(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
        }
    }
    GuestMemory::MarkWritten(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    const auto commands = recorder->Commands();
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::Copy);
    Recorder::CountBarriers(Recorder::CommandClass::Copy, 2);
    if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
    if (auto other = source.weak_from_this().lock()) keepStorageTexture(*recorder, std::move(other));
    if (Recorder::BarrierValidate()) {
        const std::array<std::pair<VkImage, bool>, 2> images{{{source.image, false}, {image, true}}};
        recorder->NoteAccess(Recorder::CommandClass::Copy, Recorder::Access{{}, {}, images, VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    VkImageMemoryBarrier barriers[2]{};
    for (auto& barrier : barriers) {
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
    }
    barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].image = source.image;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].image = image;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);
    std::vector<VkImageCopy> regions;
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
            if (!geometry.HasLayer(level, layer)) continue;
            VkImageCopy region{};
            region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, geometry.CopyLayer(layer), 1};
            region.srcOffset = {0, 0, geometry.CopyDepth(layer)};
            region.dstSubresource = region.srcSubresource;
            region.dstOffset = region.srcOffset;
            region.extent = {std::max(descriptor.width >> level, 1u), std::max(descriptor.height >> level, 1u), 1u};
            regions.push_back(region);
        }
    }
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source.image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, static_cast<std::uint32_t>(regions.size()), regions.data());
    VkImageMemoryBarrier copied = barriers[1];
    copied.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copied.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &copied);
    recorder->EndGpuTiming(timing, guestBytes);
    MarkWholeWritten();
    return true;
}

void StorageTexture::MarkWholeWritten() {
    // As after a clear: the image holds the surface, guest memory follows at the write-back (which
    // keeps the 64 KiB blocks the CPU writes from now on; the collect is uncached for the reason
    // given in FillClear).
    if (GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) {
        layerOriginal.assign(trackedLayers, false);
        cancelStoredSaves();
        releaseUnheldOriginal();
    } else {
        captureGuestBytes(nullptr);
    }
    uploadedKeys = DccKeys::Uncompressed;
    const auto now = GuestMemory::CollectWritesUncached(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) layerGeneration[layer] = collectedLayerGeneration(layer, now, true);
    refreshGeneration();
    forgetBorrowed(0, trackedLayers);
    MarkDirty();
}

void StorageTexture::MarkUnitsWritten(const std::vector<bool>& units) {
    if (!blockUnits || descriptor.dccAddress != 0 || units.size() != trackedLayers || std::all_of(units.begin(), units.end(), [](bool unit) { return unit; })) {
        MarkWholeWritten();
        return;
    }
    const auto runs = unitRuns(units);
    if (runs.empty()) return;
    if (GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) {
        for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
            if (!units[unit]) continue;
            layerOriginal[unit] = false;
            cancelStoredSave(unit);
        }
        releaseUnheldOriginal();
    } else {
        captureGuestBytes(&units);
    }
    uploadedKeys = DccKeys::Uncompressed;
    for (const auto& [begin, end] : runs) {
        const auto first = static_cast<std::uint32_t>(begin / trackedLayerBytes);
        const auto count = static_cast<std::uint32_t>((end - begin + trackedLayerBytes - 1) / trackedLayerBytes);
        const auto collected = GuestMemory::CollectWritesUncached(descriptor.baseAddress + begin, static_cast<std::size_t>(end - begin));
        for (auto unit = first; unit < first + count; ++unit) layerGeneration[unit] = collected;
    }
    refreshGeneration();
    for (const auto& [begin, end] : runs) {
        const auto first = static_cast<std::uint32_t>(begin / trackedLayerBytes);
        const auto count = static_cast<std::uint32_t>((end - begin + trackedLayerBytes - 1) / trackedLayerBytes);
        forgetBorrowed(first, count);
        markLayersPending(first, count);
    }
}

// See AdjacentGenerationEnabled above.
std::vector<StorageTexture::Adjacent> StorageTexture::adjacentPendingUnchanged(std::uint64_t firstBlock, std::uint64_t lastBlock) const {
    std::vector<Adjacent> adjacent;
    if (!AdjacentGenerationEnabled() || guestBytes == 0) return adjacent;
    constexpr std::uint64_t block = 65536;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        const auto consider = [&](StorageTexture* texture) {
            if (texture == this || texture->guestBytes == 0 || texture->overlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) return;
            const auto begin = texture->descriptor.baseAddress;
            const auto neighbourFirst = begin / block;
            const auto neighbourLast = (begin + texture->guestBytes - 1) / block;
            if (neighbourFirst > lastBlock || neighbourLast < firstBlock) return;
            // Surfaces that do not overlap share at most one block: the neighbour's tracked layer
            // holding it is what the stamps touch.
            const auto shared = std::max(neighbourFirst, firstBlock);
            const auto layer = static_cast<std::uint32_t>((std::max(shared * block, begin) - begin) / texture->trackedLayerBytes);
            if (auto alive = texture->weak_from_this().lock()) adjacent.push_back({std::move(alive), layer, texture->layerGeneration[layer]});
        };
        for (auto* texture : pending.textures) consider(texture);
        for (auto* texture : pending.flushing) consider(texture);
    }
    // Only a layer whose memory has not changed since its generation can be advanced: the collect
    // stamps the CPU's writes so far, and a later one lands at a newer generation either way.
    std::erase_if(adjacent, [](const Adjacent& entry) {
        const auto& texture = entry.texture;
        const auto begin = texture->layerBegin(entry.layer);
        const auto bytes = static_cast<std::size_t>(texture->layerBytes(entry.layer));
        GuestMemory::CollectWritesUncached(begin, bytes);
        return !GuestMemory::UnchangedSince(begin, bytes, entry.generation);
    });
    return adjacent;
}

void StorageTexture::advanceAdjacent(const std::vector<Adjacent>& adjacent, std::uint64_t now, std::uint64_t firstBlock, std::uint64_t lastBlock) {
    static const bool trace = std::getenv("APS5_TRACE_FLUSH") != nullptr;
    constexpr std::uint64_t block = 65536;
    for (const auto& [texture, layer, seen] : adjacent) {
        // A layer stored or refreshed meanwhile set its own generation (an old one on purpose when
        // it kept blocks for the CPU): only the value seen at the check is advanced.
        if (texture->layerGeneration[layer] != seen || now <= seen) continue;
        // A CPU write to the neighbour landing since the check may have been stamped by another
        // thread's walk of its range at a value in (seen, now]; the advance would hide it. Its blocks
        // outside this write-back's span carry no stamp of ours, so they are re-checked against the
        // seen generation (a stamp scan, no walk). A write inside the shared block itself in that
        // window is indistinguishable from this write-back's stamp, as it is for the block rule.
        const auto begin = texture->layerBegin(layer);
        const auto end = begin + texture->layerBytes(layer);
        const auto beforeEnd = std::min(end, firstBlock * block);
        const auto afterBegin = std::max(begin, (lastBlock + 1) * block);
        if (begin < beforeEnd && !GuestMemory::UnchangedSince(begin, static_cast<std::size_t>(beforeEnd - begin), seen)) continue;
        if (afterBegin < end && !GuestMemory::UnchangedSince(afterBegin, static_cast<std::size_t>(end - afterBegin), seen)) continue;
        if (trace) std::fprintf(stderr, "[flush] adjacent pending image 0x%llx+0x%llx layer %u advanced past a write-back's stamps (generation %llu -> %llu)\n", static_cast<unsigned long long>(texture->descriptor.baseAddress), static_cast<unsigned long long>(texture->guestBytes), layer, static_cast<unsigned long long>(seen), static_cast<unsigned long long>(now));
        texture->layerGeneration[layer] = texture->advanceUnchanged(layer, now);
        texture->refreshGeneration();
    }
}

void StorageTexture::blockGenerations(std::vector<std::uint64_t>& generations) const {
    constexpr std::uint64_t block = 65536;
    const auto spanBegin = descriptor.baseAddress & ~(block - 1);
    generations.assign(static_cast<std::size_t>((descriptor.baseAddress + guestBytes - spanBegin + block - 1) / block), 0);
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        const auto end = layerBegin(layer) + layerBytes(layer);
        for (auto at = layerBegin(layer) & ~(block - 1); at < end; at += block) generations[static_cast<std::size_t>((at - spanBegin) / block)] = layerGeneration[layer];
    }
}

std::uint64_t StorageTexture::unitBlockGeneration(std::uint32_t unit, std::uint64_t at) const {
    const auto unitGeneration = layerGeneration[unit];
    if (unitGeneration == 0 || !layerPending[unit] || completionKept.empty()) return unitGeneration;
    return std::max(unitGeneration, completionKept[static_cast<std::size_t>(at / 65536 - descriptor.baseAddress / 65536)]);
}

namespace {

std::atomic<bool>& ClearHandOverOn() {
    static std::atomic<bool> on{std::getenv("APS5_NO_CLEAR_HAND_OVER") == nullptr};
    return on;
}

}

void StorageTexture::SetClearHandOver(bool enabled) {
    ClearHandOverOn().store(enabled, std::memory_order_relaxed);
}

std::uint64_t StorageTexture::ClearHandOvers() {
    return clearHandOvers.load(std::memory_order_relaxed);
}

std::uint64_t StorageTexture::HandedOverSuperseded() {
    return handedOverSuperseded.load(std::memory_order_relaxed);
}

std::optional<DccKeys> StorageTexture::clearHandOver() {
    if (!ClearHandOverOn().load(std::memory_order_relaxed) || !blockUnits || descriptor.dccAddress == 0 || anyLayerPending()) return std::nullopt;
    if (!GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) return std::nullopt;
    const auto others = overlappingPending(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    if (others.empty()) return std::nullopt;
    const auto begin = descriptor.baseAddress;
    const auto end = descriptor.baseAddress + guestBytes;
    const auto keyBytes = DccKeyBytes(guestBytes);
    for (const auto& other : others) {
        if (!other->blockUnits || other->storesWhole()) return std::nullopt;
        if (other->descriptor.dccAddress != 0 && other->descriptor.dccAddress < descriptor.dccAddress + keyBytes && descriptor.dccAddress < other->descriptor.dccAddress + DccKeyBytes(other->guestBytes)) return std::nullopt;
        for (std::uint32_t unit = 0; unit < other->trackedLayers; ++unit) {
            if (!other->layerPending[unit]) continue;
            const auto from = other->layerBegin(unit);
            const auto to = from + other->layerBytes(unit);
            if (to <= begin || from >= end) continue;
            if (from < begin || to > end || other->layerBytes(unit) != trackedLayerBytes) return std::nullopt;
        }
    }
    const auto keys = ProvedKeys();
    VkClearColorValue clear{};
    if (!IsDccClear(keys) || !ClearColorFor(storageFormat, keys, clear)) return std::nullopt;
    if (handedUnits.size() != trackedLayers) handedUnits.assign(trackedLayers, false);
    for (const auto& other : others) {
        for (std::uint32_t unit = 0; unit < other->trackedLayers; ++unit) {
            const auto from = other->layerBegin(unit);
            if (other->layerPending[unit] && from >= begin && from < end) handedUnits[static_cast<std::size_t>((from - begin) / trackedLayerBytes)] = true;
        }
        const auto same = std::find_if(handedOver.begin(), handedOver.end(), [&](const auto& entry) { return entry.first.lock() == other; });
        if (same != handedOver.end()) same->second = other->writes;
        else handedOver.emplace_back(other, other->writes);
    }
    clearHandOvers.fetch_add(1, std::memory_order_relaxed);
    return keys;
}

bool StorageTexture::AnyLayerPending() const {
    return anyLayerPending();
}

void StorageTexture::supersedeHandedOver(std::uint32_t first, std::uint32_t count) {
    const auto begin = layerBegin(first);
    const auto end = first + count >= trackedLayers ? descriptor.baseAddress + guestBytes : layerBegin(first + count);
    for (auto entry = handedOver.begin(); entry != handedOver.end();) {
        const auto source = entry->first.lock();
        if (source == nullptr || source->writes != entry->second) {
            entry = handedOver.erase(entry);
            continue;
        }
        bool superseded = false;
        for (std::uint32_t unit = 0; unit < source->trackedLayers; ++unit) {
            const auto from = source->layerBegin(unit);
            if (!source->layerPending[unit] || from < begin || from + source->layerBytes(unit) > end) continue;
            source->layerPending[unit] = false;
            handedOverSuperseded.fetch_add(1, std::memory_order_relaxed);
            superseded = true;
        }
        if (superseded) source->reconcilePending();
        ++entry;
    }
    if (first == 0 && count >= trackedLayers) handedOver.clear();
    if (std::any_of(handedUnits.begin() + first, handedUnits.begin() + std::min<std::size_t>(first + count, handedUnits.size()), [](bool handed) { return handed; })) {
        const auto current = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
        std::vector<std::uint64_t> generations;
        blockGenerations(generations);
        std::vector<std::uint8_t> stamped(generations.size(), 0);
        std::vector<std::uint8_t> cpu(generations.size(), 0);
        const bool tracked = GuestMemory::ChangedBlocks(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), generations, stamped, cpu);
        bool moved = false;
        for (std::uint32_t unit = first; unit < first + count && unit < handedUnits.size(); ++unit) {
            if (!handedUnits[unit]) continue;
            handedUnits[unit] = false;
            if (!tracked || unit >= cpu.size() || cpu[unit] != 0) continue;
            layerGeneration[unit] = advanceUnchanged(unit, current);
            moved = true;
        }
        if (moved) refreshGeneration();
    }
    if (std::none_of(handedUnits.begin(), handedUnits.end(), [](bool handed) { return handed; })) handedUnits.clear();
    pruneAdoptedSources();
}

void StorageTexture::pruneAdoptedSources() {
    std::erase_if(adoptedSources, [](const std::shared_ptr<StorageTexture>& source) { return !source->Dirty(); });
}

void StorageTexture::Detach() {
    storeTrappedOverlaps("cache eviction");
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    released = true;
}

StorageTexture::CreationHandOver::~CreationHandOver() {
    if (detached != nullptr) detached->Flush();
}

std::uint64_t StorageTexture::CreationHandOvers() {
    return creationHandOvers.load(std::memory_order_relaxed);
}

StorageTexture::CreationHandOver StorageTexture::PrepareCreation(const Context& context, const GuestTextureResource& descriptor, std::uint64_t guestBytes, std::shared_ptr<StorageTexture> detached, bool& flushed) {
    CreationHandOver handOver;
    const auto storeFirst = [&] {
        const bool dirty = detached != nullptr && detached->Dirty();
        if (detached != nullptr) detached->Flush();
        flushed = FlushPending(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), nullptr, "storage image creation", PublishScope::None) || dirty;
        return CreationHandOver();
    };
    flushed = false;
    if (!ClearHandOverOn().load(std::memory_order_relaxed) || descriptor.dccAddress == 0 || IsBlockCompressed(descriptor.format)) return storeFirst();
    const auto geometry = DescribeSurface(descriptor);
    if (geometry.guestBytes != guestBytes || !StorageBlockUnits(descriptor, geometry)) return storeFirst();
    const auto keyBytes = DccKeyBytes(guestBytes);
    CompletePendingCpuWrites(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    CompletePendingCpuWrites(context, descriptor.dccAddress, keyBytes);
    if (!GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) return storeFirst();
    auto others = overlappingPending(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    if (others.empty()) return storeFirst();
    const auto begin = descriptor.baseAddress;
    const auto end = descriptor.baseAddress + guestBytes;
    for (const auto& other : others) {
        if (!other->blockUnits || other->storesWhole()) return storeFirst();
        if (other->descriptor.dccAddress != 0 && other->descriptor.dccAddress < descriptor.dccAddress + keyBytes && descriptor.dccAddress < other->descriptor.dccAddress + DccKeyBytes(other->guestBytes)) return storeFirst();
        for (std::uint32_t unit = 0; unit < other->trackedLayers; ++unit) {
            if (!other->layerPending[unit]) continue;
            const auto from = other->layerBegin(unit);
            const auto to = from + other->layerBytes(unit);
            if (to <= begin || from >= end) continue;
            if (from < begin || to > end || other->layerBytes(unit) != 65536) return storeFirst();
        }
    }
    DccKeyProof proof;
    const auto keys = ProvedClearKeys(descriptor, guestBytes, proof);
    VkClearColorValue clear{};
    if (!IsDccClear(keys) || !ClearColorFor(StorageFormatFor(context, ResolveTextureFormat(descriptor.format)), keys, clear)) return storeFirst();
    handOver.keys = keys;
    handOver.sources = std::move(others);
    handOver.detached = std::move(detached);
    handOver.handed = true;
    return handOver;
}

}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
