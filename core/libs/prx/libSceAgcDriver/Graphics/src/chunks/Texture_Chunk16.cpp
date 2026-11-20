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
namespace {
void StorageTexture::AdoptHandOver(CreationHandOver handOver) {
    if (!handOver.handed) return;
    if (!blockUnits || trackedLayerBytes != 65536 || uploadedKeys != handOver.keys || anyLayerPending()) {
        FlushPending(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), this, "storage image creation", PublishScope::None);
        if (handOver.detached != nullptr) handOver.detached->Flush();
        if (uploadedKeys != handOver.keys) Refresh();
        return;
    }
    const auto begin = descriptor.baseAddress;
    const auto end = descriptor.baseAddress + guestBytes;
    if (handedUnits.size() != trackedLayers) handedUnits.assign(trackedLayers, false);
    for (const auto& source : handOver.sources) {
        for (std::uint32_t unit = 0; unit < source->trackedLayers; ++unit) {
            const auto from = source->layerBegin(unit);
            if (source->layerPending[unit] && from >= begin && from < end) handedUnits[static_cast<std::size_t>((from - begin) / trackedLayerBytes)] = true;
        }
        const auto same = std::find_if(handedOver.begin(), handedOver.end(), [&](const auto& entry) { return entry.first.lock() == source; });
        if (same != handedOver.end()) same->second = source->writes;
        else handedOver.emplace_back(source, source->writes);
    }
    if (handOver.detached != nullptr) {
        auto inherited = std::exchange(handOver.detached->adoptedSources, {});
        adoptedSources.push_back(std::move(handOver.detached));
        for (auto& source : inherited) adoptedSources.push_back(std::move(source));
    }
    creationHandOvers.fetch_add(1, std::memory_order_relaxed);
}

std::shared_ptr<StorageTexture> StorageTexture::pendingAlias() const {
    static const bool disabled = std::getenv("APS5_NO_ALIAS_BORROW") != nullptr;
    if (disabled || !blockUnits) return nullptr;
    std::vector<std::shared_ptr<StorageTexture>> candidates;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        for (auto* texture : pending.textures) {
            if (texture == this || texture->descriptor.baseAddress != descriptor.baseAddress || texture->guestBytes != guestBytes || !texture->blockUnits) continue;
            if (auto alive = texture->weak_from_this().lock()) candidates.push_back(std::move(alive));
        }
    }
    for (const auto& candidate : candidates) {
        const auto& mine = descriptor;
        const auto& theirs = candidate->descriptor;
        // The same texels under another format of the same size: the device copy moves them bit
        // for bit, as the guest bytes would.
        const auto oneSlice = [](const GuestTextureResource& resource) { return resource.dimension == TextureDimension::k2D || (resource.dimension == TextureDimension::k2DArray && resource.depthOrLastArray == 0); };
        const bool sameDimension = mine.dimension == theirs.dimension || (oneSlice(mine) && oneSlice(theirs));
        const bool sameShape = mine.width == theirs.width && mine.height == theirs.height && mine.depthOrLastArray == theirs.depthOrLastArray && mine.baseArray == theirs.baseArray && mine.mipCount == theirs.mipCount && mine.tileMode == theirs.tileMode && sameDimension && BytesPerElement(mine.format) == BytesPerElement(theirs.format) && geometry.imageLayers == candidate->geometry.imageLayers && geometry.imageDepth == candidate->geometry.imageDepth && trackedLayers == candidate->trackedLayers;
        if (sameShape && candidate->Cached() && candidate->uploadedKeys == DccKeys::Uncompressed) return candidate;
    }
    return nullptr;
}

void StorageTexture::forgetBorrowed(std::uint32_t first, std::uint32_t count) {
    if (borrowedUnits.empty()) return;
    if (first < borrowedUnits.size()) borrowedUnits.SetRange(first, std::min<std::size_t>(count, borrowedUnits.size() - first), false);
    if (!borrowedUnits.Any()) {
        borrowedUnits.clear();
        borrowedFrom.reset();
    }
}

std::uint64_t StorageTexture::borrowUnits(StorageTexture& source, const std::vector<bool>& units) {
    const auto runs = unitRuns(units);
    const auto windows = sliceWindows(runs);
    Require(!windows.empty(), "storage image borrows no unit");
    std::vector<VkImageCopy> regions;
    for (const auto& window : windows) {
        for (const auto& region : window.regions) regions.push_back({region.imageSubresource, region.imageOffset, region.imageSubresource, region.imageOffset, region.imageExtent});
    }
    std::uint64_t bytes = 0;
    for (const auto& [begin, end] : runs) bytes += end - begin;
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    auto timing = Recorder::NoTiming;
    if (recorder != nullptr) {
        commands = recorder->Commands();
        timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageUpload);
        if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
        if (auto other = source.weak_from_this().lock()) keepStorageTexture(*recorder, std::move(other));
        Recorder::CountBarriers(Recorder::CommandClass::StorageUpload, 2);
        if (Recorder::BarrierValidate()) {
            const std::array<std::pair<VkImage, bool>, 2> images{{{source.image, false}, {image, true}}};
            recorder->NoteAccess(Recorder::CommandClass::StorageUpload, Recorder::Access{{}, {}, images, VK_PIPELINE_STAGE_TRANSFER_BIT});
        }
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
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
    context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source.image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, static_cast<std::uint32_t>(regions.size()), regions.data());
    VkImageMemoryBarrier copied = barriers[1];
    copied.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copied.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &copied);
    if (batch) batch->SubmitAndWait();
    else recorder->EndGpuTiming(timing, bytes);
    countStorageUpload(3, bytes);
    if (borrowedFrom.lock() != source.weak_from_this().lock() || borrowedUnits.size() != trackedLayers) {
        borrowedUnits.assign(trackedLayers, false);
        borrowedFrom = source.weak_from_this();
    }
    borrowedVersion = source.version;
    source.lent = true;
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        if (units[unit]) borrowedUnits[unit] = true;
    }
    ++version;
    return bytes;
}

void StorageTexture::releaseReadTraps(bool sharingPages) {
#if defined(__APPLE__)
    if (!labelTrapsSet.load(std::memory_order_relaxed)) return;
    const auto release = [](const StorageTexture* texture) {
        if (texture->readTrapped.exchange(false, std::memory_order_acq_rel)) GuestWriteWatch::GuestWriteWatchReleaseReads_nid_postfix(static_cast<std::uintptr_t>(texture->descriptor.baseAddress), static_cast<std::size_t>(texture->guestBytes));
    };
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    release(this);
    if (!sharingPages) return;
    const auto page = TrapPage();
    const auto first = descriptor.baseAddress & ~(page - 1);
    const auto bytes = static_cast<std::size_t>(((descriptor.baseAddress + guestBytes + page - 1) & ~(page - 1)) - first);
    for (const auto* texture : pending.textures) {
        if (texture != this && texture->overlaps(first, bytes)) release(texture);
    }
    for (const auto* texture : pending.flushing) {
        if (texture != this && texture->overlaps(first, bytes)) release(texture);
    }
#else
    static_cast<void>(sharingPages);
#endif
}

void StorageTexture::storeTrappedOverlaps(const char* reason) {
#if defined(__APPLE__)
    if (!labelTrapsSet.load(std::memory_order_relaxed)) return;
    std::vector<std::pair<std::uint64_t, std::size_t>> ranges;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        if (!dirty) return;
        for (const auto* texture : pending.textures) {
            if (texture != this && texture->readTrapped.load(std::memory_order_acquire) && texture->overlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) ranges.emplace_back(texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes));
        }
    }
    for (const auto& [address, bytes] : ranges) static_cast<void>(FlushPending(address, bytes, nullptr, reason));
#else
    static_cast<void>(reason);
#endif
}

void StorageTexture::writeBack(std::uint64_t address, std::size_t bytes) {
    if (readTrapped.load(std::memory_order_acquire)) {
        address = descriptor.baseAddress;
        bytes = static_cast<std::size_t>(guestBytes);
    }
    // Block units are stored per access; an image whose units keep being asked for in pieces (a
    // consumer touching it through many small ranges, each piece re-arming the pending memos of
    // the next dispatch) stores every pending unit once more than `pieces` partial stores fall
    // within `window` presents, and keeps doing so for another window. Off by default: the title's
    // aliased images are stored in pieces by design (every piece is another image's hand-over) and
    // widening them stored whole images again (movie stage: 39 vs 35 ms of GPU per present, 188 vs
    // 214 presents per 10 s); APS5_BLOCK_WRITEBACK_WIDEN=1 enables the widening, and
    // APS5_BLOCK_WRITEBACK_EACH=1 stores the touched units only, every time.
    const bool each = BlockWriteBackEach();
    static const std::uint32_t pieces = [] { const char* text = std::getenv("APS5_BLOCK_WRITEBACK_PIECES"); return text != nullptr ? static_cast<std::uint32_t>(std::strtoul(text, nullptr, 10)) : 4u; }();
    static const std::uint64_t window = [] { const char* text = std::getenv("APS5_BLOCK_WRITEBACK_FRAMES"); return text != nullptr ? std::strtoull(text, nullptr, 10) : 8ull; }();
    std::vector<bool> layers(trackedLayers, false);
    std::size_t selected = 0, pendingCount = 0;
    // Under a clear code the texels outside the stored units would read as the clear while the
    // keys, marked uncompressed for the whole surface, say texels: such an image stores whole.
    const bool whole = storesWhole();
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layerPending[layer]) continue;
        ++pendingCount;
        layers[layer] = whole || (address < layerBegin(layer) + layerBytes(layer) && layerBegin(layer) < address + bytes);
        if (layers[layer]) ++selected;
    }
    if (selected == 0) {
        reconcilePending();
        return;
    }
    if (blockUnits && !each && selected < pendingCount) {
        const auto present = Recorder::Presents();
        if (present - partialWindowStart >= window) {
            partialWindowStart = present;
            partialStores = 0;
        }
        if (present < storeWholeUntil || ++partialStores > pieces) {
            storeWholeUntil = present + window;
            layers = layerPending.Bools();
            coalescedWriteBacks.fetch_add(1, std::memory_order_relaxed);
        }
    }
    writeBackLayers(layers);
}

bool StorageTexture::guestBytesSettled() const {
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> range{{{descriptor.baseAddress, descriptor.baseAddress + guestBytes}}};
    if (AnyShadowedOverlaps(range) || AnyPendingOverlaps(range)) return false;
    const auto* recorder = Recorder::Active();
    return recorder == nullptr || !recorder->PendingWriteOverlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
}

bool StorageTexture::unchangedSinceBaseline(std::uint64_t from, std::uint64_t to) const {
    const auto& baseline = originalHolds(from, static_cast<std::size_t>(to - from)) ? original : generationBaseline;
    if (baseline.size() != guestBytes) return false;
    const auto offset = static_cast<std::size_t>(from - descriptor.baseAddress);
    return GuestMemory::EqualsCommittedUnsynced(from, std::span<const std::byte>(baseline).subspan(offset, static_cast<std::size_t>(to - from)));
}

bool StorageTexture::cpuLeftUnchanged(std::uint32_t layer, std::uint64_t from, std::uint64_t to, bool edge) const {
    if (!unchangedSinceBaseline(from, to)) return false;
    if (edge || layerPendingSince[layer] == 0) return true;
    const std::array<std::uint64_t, 1> since{layerPendingSince[layer]};
    std::array<std::uint8_t, 1> changed{};
    std::array<std::uint8_t, 1> cpu{};
    GuestMemory::ChangedBlocks(from, static_cast<std::size_t>(to - from), since, changed, cpu);
    return cpu[0] == 0;
}

void StorageTexture::writeBackLayers(const std::vector<bool>& layers) {
    CaptureTrace::Log("writeback image=%llx generation=%llu reason=%s units=%zu", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(generation), flushReason, static_cast<std::size_t>(std::count(layers.begin(), layers.end(), true)));
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    PhaseTimer timer;
    struct Account {
        bool enabled;
        PhaseTimer& timer;
        ~Account() { if (enabled) Profile().storageWriteBack += timer.lap(); }
    } account{profile, timer};
    // Whatever the outcome, the registration follows the pending flags (a throw leaves the
    // layers pending).
    struct Reconcile {
        StorageTexture& texture;
        ~Reconcile() { texture.reconcilePending(); }
    } reconcile{*this};
    const auto elementBytes = BytesPerElement(descriptor.format);
    // 64 KiB blocks the CPU wrote since the layer was last in sync keep the CPU's bytes: the game
    // may have reused the memory for something else entirely (see layerGeneration). A generation of
    // zero means no tracking, and the layer is stored whole.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> keep;
    std::vector<std::pair<std::uint64_t, std::uint32_t>> keptBlocks;
    std::vector<bool> skippedLayer(trackedLayers, false);
    bool skippedAny = false;
    std::size_t skipped = 0;
    std::uint64_t firstStored = descriptor.baseAddress + guestBytes;
    std::uint64_t lastStored = descriptor.baseAddress;
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layers[layer]) continue;
        firstStored = std::min(firstStored, layerBegin(layer));
        lastStored = std::max(lastStored, layerBegin(layer) + layerBytes(layer));
    }
    if (lastStored <= firstStored) return;
    struct ReleaseReadTrap {
        StorageTexture& texture;
        ~ReleaseReadTrap() { texture.releaseReadTraps(false); }
    } releaseReadTrap{*this};
    constexpr std::uint64_t block = 65536;
    const auto spanBegin = firstStored & ~(block - 1);
    const auto spanBlocks = static_cast<std::size_t>((lastStored - spanBegin + block - 1) / block);
    // The keep-blocks decision must see every CPU write to the stored span up to now, so it
    // bypasses the per-packet collect memo (a Refresh collect earlier in the same packet would
    // satisfy it); the unselected layers keep their own generations and are collected by their
    // own refreshes. One tracker lock then reads the span's stamps against the layers' generations.
    GuestMemory::CollectWritesUncached(firstStored, static_cast<std::size_t>(lastStored - firstStored));
    std::vector<std::uint64_t> generations(spanBlocks, 0);
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layers[layer]) continue;
        const auto end = layerBegin(layer) + layerBytes(layer);
        for (auto at = layerBegin(layer) & ~(block - 1); at < end; at += block) generations[static_cast<std::size_t>((at - spanBegin) / block)] = unitBlockGeneration(layer, at);
    }
    const auto drawn = drawnArea();
    std::vector<std::uint8_t> changedBlocks(spanBlocks);
    std::vector<std::uint8_t> cpuBlocks(drawn.drawn ? spanBlocks : 0);
    const bool tracked = GuestMemory::ChangedBlocks(firstStored, static_cast<std::size_t>(lastStored - firstStored), generations, changedBlocks, cpuBlocks);
    if (tracked) confirmRewatched(spanBegin, changedBlocks, cpuBlocks, nullptr);
    std::vector<bool> compared(trackedLayers, false);
    std::vector<bool> unitTracked;
    if (!tracked) {
        awaitStoredSave(&layers);
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (!layers[layer] || layerOriginal[layer]) continue;
            const auto begin = layerBegin(layer);
            const auto bytes = static_cast<std::size_t>(layerBytes(layer));
            const auto offset = static_cast<std::size_t>((begin - spanBegin) / block);
            const auto count = static_cast<std::size_t>((begin + bytes - spanBegin + block - 1) / block) - offset;
            auto cpu = cpuBlocks.empty() ? std::span<std::uint8_t>{} : std::span(cpuBlocks).subspan(offset, count);
            GuestMemory::CollectWritesUncached(begin, bytes);
            if (unitTracked.empty()) unitTracked.assign(trackedLayers, false);
            unitTracked[layer] = GuestMemory::ChangedBlocks(begin, bytes, std::span(generations).subspan(offset, count), std::span(changedBlocks).subspan(offset, count), cpu);
            if (unitTracked[layer]) confirmRewatched(begin & ~std::uint64_t{0xffff}, std::span(changedBlocks).subspan(offset, count), cpu, nullptr);
        }
        std::vector<bool> saved(trackedLayers, false);
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) saved[layer] = layers[layer] && layerOriginal[layer];
        for (const auto& [begin, end] : unitRuns(saved)) {
            const auto from = descriptor.baseAddress + begin;
            const auto last = descriptor.baseAddress + end - 1;
            if (!compareUntracked(from, static_cast<std::size_t>(end - begin), std::span(changedBlocks).subspan(static_cast<std::size_t>((from - spanBegin) / block), static_cast<std::size_t>(last / block - from / block + 1)))) continue;
            for (auto layer = static_cast<std::uint32_t>(begin / trackedLayerBytes); layer < trackedLayers && layerBegin(layer) <= last; ++layer) compared[layer] = true;
        }
    }
    const auto forgetStored = [&] {
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (!layers[layer]) continue;
            layerOriginal[layer] = false;
            cancelStoredSave(layer);
        }
        releaseUnheldOriginal();
    };
    bool unsavedOutsideWatch = false;
    std::string unsavedCause;
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layers[layer]) continue;
        const auto begin = layerBegin(layer);
        const auto end = begin + layerBytes(layer);
        const bool layerTracked = unitTracked.empty() ? tracked : unitTracked[layer];
        const bool unknown = layerTracked ? layerGeneration[layer] == 0 : !compared[layer];
        if (unknown && !drawn.rect) {
            if (!layerTracked && !unsavedOutsideWatch) unsavedCause = std::string("storing storage image results pending over memory outside the write watch without the guest bytes saved before them is not implemented (unit ") + std::to_string(layer) + " of " + std::to_string(trackedLayers) + ": " + BaselineMissText(pendingMiss[layer]) + ")";
            unsavedOutsideWatch = unsavedOutsideWatch || !layerTracked;
            keep.emplace_back(begin, end);
            continue;
        }
        for (auto at = begin & ~(block - 1); at < end; at += block) {
            const auto from = std::max(at, begin);
            const auto to = std::min(at + block, end);
            if (from >= to) continue;
            const bool edge = from != at || to != at + block;
            const auto index = static_cast<std::size_t>((at - spanBegin) / block);
            const bool kept = keptAtUnwatchHolds(from, layer);
            const bool written = kept || unknown || (changedBlocks[index] == GuestMemory::BlockWritten && (compared[layer] || !edge || GuestMemory::StoredOver(from, static_cast<std::size_t>(to - from), unitBlockGeneration(layer, at))) && !cpuLeftUnchanged(layer, from, to, edge));
            if (written && (!drawn.rect || (layerTracked && !kept && !unknown && cpuBlocks[index] == 0))) {
                skippedAny = true;
                skippedLayer[layer] = true;
                keptBlocks.emplace_back(from, layer);
                ++skipped;
                continue;
            }
            if (written && drawn.rect) skippedLayer[layer] = true;
            if (!keep.empty() && keep.back().second == from) keep.back().second = to;
            else keep.emplace_back(from, to);
        }
    }
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (layers[layer]) forgetKeptAtUnwatch(layer);
    }
    for (const auto& [from, layer] : keptBlocks) keptAtUnwatch.emplace(from, layerGeneration[layer]);
    // Debug aid: APS5_TRACE_FLUSH also names the blocks a write-back leaves to the CPU.
    static const bool traceKept = std::getenv("APS5_TRACE_FLUSH") != nullptr;
    if (traceKept && skippedAny) {
        const auto first = keep.empty() ? descriptor.baseAddress + guestBytes : keep.front().first;
        std::fprintf(stderr, "[flush] image 0x%llx+0x%llx keeps %zu CPU-written 64 KiB blocks (first stored byte at +0x%llx, %zu ranges stored, generation %llu)\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), skipped, static_cast<unsigned long long>(first - descriptor.baseAddress), keep.size(), static_cast<unsigned long long>(generation));
    }
    std::vector<std::pair<std::uint64_t, std::uint64_t>> spans;
    if (drawn.rect) spans = drawnSpans(drawn, keep);
    const auto& stores = drawn.rect ? spans : keep;
    const auto stampBegin = drawn.rect && !stores.empty() ? stores.front().first : firstStored;
    const auto stampEnd = drawn.rect && !stores.empty() ? stores.back().second : lastStored;
    // Taken before this write-back stamps anything (see advanceAdjacent), with the 64 KiB block span
    // its stamps cover.
    const auto firstBlock = stampBegin / block;
    const auto lastBlock = (stampEnd - 1) / block;
    const auto adjacent = adjacentPendingUnchanged(firstBlock, lastBlock);
    // After the store: the stored layers are in sync at a fresh generation unless blocks were kept
    // for the CPU (then the layer is stale there, keeps its old generation and re-uploads at its
    // next use); their results are no longer pending either way. The collect covers the stored
    // span, which holds every stored layer's pages. Pieces retiled into the import's unit shadow
    // (`shadowed`, with its import) are fresh at the generation taken after this write-back's
    // own stamps, so every later import writer stamps newer.
    std::vector<ShadowedRange> shadowed;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> imported;
    std::vector<ImportSegment> segments;
    const auto markShadowed = [&](std::uint64_t generation) {
        for (const auto& segment : segments) {
            std::vector<ShadowedRange> ranges;
            for (const auto& range : shadowed) {
                if (range.begin < segment.begin || range.begin >= segment.end) continue;
                Require(range.end <= segment.end, "a shadowed storage write-back range crosses the end of its import segment");
                ranges.push_back(range);
            }
            if (!ranges.empty()) MarkShadowed(*segment.import, ranges, generation);
        }
    };
    const auto settle = [&](bool memoizedCollect) {
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (layers[layer]) layerPending[layer] = false;
        }
        bool advances = false;
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) advances = advances || (layers[layer] && !skippedLayer[layer]);
        if (!advances && adjacent.empty() && shadowed.empty()) return;
        const auto now = memoizedCollect ? GuestMemory::CollectWrites(firstStored, static_cast<std::size_t>(lastStored - firstStored)) : GuestMemory::CollectWritesUncached(firstStored, static_cast<std::size_t>(lastStored - firstStored));
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (layers[layer] && !skippedLayer[layer]) {
                layerGeneration[layer] = collectedLayerGeneration(layer, now, !memoizedCollect);
                forgetUnwatchHash(layer, 1);
                if (memoizedCollect && generationBaseline.size() == guestBytes) {
                    baselineDue[layer] = true;
                    rebaseTicket[layer] = 0;
                    anyBaselineDue.store(true, std::memory_order_relaxed);
                }
            }
        }
        refreshGeneration();
        advanceAdjacent(adjacent, now, firstBlock, lastBlock);
        if (!shadowed.empty()) markShadowed(now);
    };
    if (stores.empty()) {
        // Every selected block was written by the CPU since the layer's generation: nothing to
        // store, the keys stay as they are, and the image is stale there (re-uploaded at its next
        // use, never from a matching `original`).
        emptyWriteBacks.fetch_add(1, std::memory_order_relaxed);
        forgetStored();
        settle(true);
        return;
    }
    if (const auto* import = HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) segments.push_back({import, descriptor.baseAddress, descriptor.baseAddress + guestBytes});
    else segments = importSegments();
    if (flushReason != nullptr && std::strcmp(flushReason, "idle") == 0 && GuestMemory::Watched(firstStored, static_cast<std::size_t>(lastStored - firstStored))) segments.clear();
    if (!segments.empty()) {
        Require(!unsavedOutsideWatch, unsavedCause);
        // Outside the write watch the stored bytes are what the next store compares with.
        const auto saveUnits = GuestMemory::Watched(firstStored, static_cast<std::size_t>(lastStored - firstStored)) ? std::vector<std::uint32_t>{} : prepareStoredSave(layers, skippedLayer);
        if (blockUnits) {
            if (guestBytes <= SmallStoreBytes) PublishShadow(firstStored, static_cast<std::size_t>(lastStored - firstStored), PublishScope::PartialUnits, PublishReason::Upload);
            // The kept blocks alone pass through the retiler (windows of their slices).
            const auto storedBytes = writeBackWindows(segments, keep, firstStored, lastStored, shadowed, imported, saveUnits.empty() ? nullptr : savedBytes.get());
            countStorageWriteBack(storedBytes, true);
            if (profile) Profile().storageGpu += timer.lap();
            forgetStored();
            traceKeyStore("block write-back", descriptor, guestBytes);
            if (!IsDccClear(filledKeys) && uploadedKeys != DccKeys::Uncompressed && !keysReplacedByClear()) MarkDccUncompressed(context, descriptor.dccAddress, guestBytes, DccKeyCount(descriptor, guestBytes));
            uploadedKeys = DccKeys::Uncompressed;
            for (const auto& [from, to] : keep) GuestMemory::MarkWritten(from, static_cast<std::size_t>(to - from));
            settle(true);
            commitStoredSave(saveUnits);
            ++Profile().storageDirectWriteBacks;
            return;
        }
        if (drawn.rect) {
            PublishShadow(stampBegin, static_cast<std::size_t>(stampEnd - stampBegin), PublishScope::PartialUnits, PublishReason::Upload);
            const auto storedBytes = storeDrawnRect(drawn, segments, stores, saveUnits);
            countStorageWriteBack(storedBytes, true, true);
            if (profile) Profile().storageGpu += timer.lap();
            forgetStored();
            uploadedKeys = DccKeys::Uncompressed;
            std::vector<std::pair<std::uint64_t, std::uint64_t>> stamps;
            for (const auto& [from, to] : stores) {
                if (!stamps.empty() && from / block <= (stamps.back().second - 1) / block + 1) stamps.back().second = to;
                else stamps.emplace_back(from, to);
            }
            for (const auto& [from, to] : stamps) GuestMemory::MarkWritten(from, static_cast<std::size_t>(to - from));
            settle(true);
            commitStoredSave(saveUnits);
            ++Profile().storageDirectWriteBacks;
            return;
        }
        // The whole-layer copies below overwrite the interior units of the span; the edge units
        // keep a neighbour's shadowed results in the rest of their bytes, which reach the import
        // first (this write-back's stamps then make those units stale).
        PublishShadow(firstStored, static_cast<std::size_t>(lastStored - firstStored), PublishScope::PartialUnits, PublishReason::Upload);
        // The surface lives in host-imported memory: the retiler writes into device scratch and the
        // untouched blocks are copied into the imported bytes in place, recorded behind the work that
        // produced the image; nothing crosses to the CPU.
        auto linear = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(sliceLinearBytes * arrayLayers), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        std::vector<CopiedBytes> copied;
        for (const auto& [from, to] : keep) copied.push_back({from - descriptor.baseAddress, to - descriptor.baseAddress, from - descriptor.baseAddress});
        auto padding = paddingSeeds(segments, copied);
        std::vector<std::pair<std::uint64_t, std::uint64_t>> importStores(keep.begin(), keep.end());
        const bool linearRows = descriptor.tileMode == TextureTileMode::kLinear && !geometry.thick && std::none_of(mips.begin(), mips.end(), [](const TileMipLayout& mip) { return mip.tail; });
        const HostImport* directImport = nullptr;
        if (linearRows && !padding.copies.empty() && padding.slabs.empty() && saveUnits.empty()) {
            std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
            for (const auto& range : copied) runs.emplace_back(range.begin, range.end);
            importStores.clear();
            for (const auto& [begin, end] : coveredBytes(runs)) importStores.emplace_back(descriptor.baseAddress + begin, descriptor.baseAddress + end);
            padding = {};
            static const bool importRetile = std::getenv("APS5_NO_LINEAR_IMPORT_RETILE") == nullptr;
            if (importRetile && !geometry.levelMajor && elementBytes % 4 == 0 && segments.size() == 1 && segments.front().begin <= firstStored && lastStored <= segments.front().end) {
                std::vector<std::pair<std::uint64_t, std::uint64_t>> wholeLayers;
                for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
                    if (!layers[layer]) continue;
                    const auto begin = layerBegin(layer);
                    const auto end = begin + layerBytes(layer);
                    if (!wholeLayers.empty() && wholeLayers.back().second == begin) wholeLayers.back().second = end;
                    else wholeLayers.emplace_back(begin, end);
                }
                if (wholeLayers == keep && HostImportDeviceLocal(context, *segments.front().import)) directImport = segments.front().import;
            }
        }
        const auto tiledScratch = directImport == nullptr ? std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(guestBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT) : nullptr;
        detiler.BeginBatch();
        auto* recorder = recorderFor(context);
        std::unique_ptr<CommandBatch> batch;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        auto timing = Recorder::NoTiming;
        std::uint64_t storedBytes = 0;
        for (const auto& [from, to] : keep) storedBytes += to - from;
        if (recorder != nullptr) {
            recorder->FlushKeyStoresOverlapping(firstStored, static_cast<std::size_t>(lastStored - firstStored));
            recorder->FlushStoresOverlapping(firstStored, static_cast<std::size_t>(lastStored - firstStored));
            commands = recorder->Commands();
            timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageWriteBack);
            if (linear) recorder->Keep(linear, linear->Size());
            if (tiledScratch) recorder->Keep(tiledScratch, tiledScratch->Size());
            for (const auto& slab : padding.slabs) recorder->Keep(slab);
            // The image itself must outlive the recorded retile: the cache may evict it right after.
            if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
            Recorder::CountBarriers(Recorder::CommandClass::StorageWriteBack, (padding.copies.empty() ? 4 : 5) - (directImport != nullptr ? 1 : 0));
            if (Recorder::BarrierValidate()) {
                const std::pair<VkImage, bool> read{image, false};
                recorder->NoteAccess(Recorder::CommandClass::StorageWriteBack, Recorder::Access{padding.importReads, importStores, std::span(&read, 1), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT});
            }
        } else {
            batch = std::make_unique<CommandBatch>(context);
            commands = batch->Handle();
        }
        // The tracked layers are the array layers when there are several; one tracked layer means
        // the whole surface.
        const auto* storedLayers = trackedLayers == arrayLayers && trackedLayers > 1 ? &layers : nullptr;
        VkImageMemoryBarrier toSource{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toSource.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toSource.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSource.image = image;
        toSource.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
        if (!padding.copies.empty()) {
            // The current bytes of the copied ranges no element holds go under the retile (every
            // earlier writer of the import, host stores included, first; the import-ready barrier
            // below orders the scratch writes).
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            for (const auto& [source, copies] : padding.copies) {
                context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, source, tiledScratch->Handle(), static_cast<std::uint32_t>(copies.size()), copies.data());
                paddingSeedCopies.fetch_add(copies.size(), std::memory_order_relaxed);
            }
        }
        const auto regions = CopyRegions(storedLayers);
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear->Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
        const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        const VkMemoryBarrier importReady{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &importReady, 1, &linearRead, 0, nullptr);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            if (storedLayers != nullptr && !(*storedLayers)[layer]) continue;
            for (std::uint32_t level = 0; level < mips.size(); ++level) {
                if (!geometry.HasLayer(level, layer)) continue;
                const auto& mip = mips[level];
                if (directImport != nullptr) detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear->Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, directImport->buffer, HostImportOffset(*directImport, descriptor.baseAddress + geometry.GuestOffset(layer, mip)), mip, true, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
                else detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear->Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, tiledScratch->Handle(), geometry.GuestOffset(layer, mip), mip, true, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
            }
        }
        if (directImport != nullptr) {
            linearImportRetiles.fetch_add(1, std::memory_order_relaxed);
        } else {
            const auto scratchDone = WholeBufferBarrier(tiledScratch->Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &scratchDone, 0, nullptr);
            for (const auto& segment : segments) {
                std::vector<VkBufferCopy> copies;
                for (const auto& [from, to] : importStores) {
                    const auto begin = std::max(from, segment.begin);
                    const auto end = std::min(to, segment.end);
                    if (begin < end) copies.push_back({begin - descriptor.baseAddress, HostImportOffset(*segment.import, begin), end - begin});
                }
                if (!copies.empty()) context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, tiledScratch->Handle(), segment.import->buffer, static_cast<std::uint32_t>(copies.size()), copies.data());
            }
            if (!saveUnits.empty()) {
                std::vector<VkBufferCopy> saves;
                for (const auto& [from, to] : keep) saves.push_back({from - descriptor.baseAddress, from - descriptor.baseAddress, to - from});
                context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, tiledScratch->Handle(), savedBytes->Handle(), static_cast<std::uint32_t>(saves.size()), saves.data());
            }
        }
        VkImageMemoryBarrier backToGeneral = toSource;
        backToGeneral.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        backToGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        backToGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        backToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        const VkMemoryBarrier stored{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &stored, 0, nullptr, 1, &backToGeneral);
        if (batch) {
            batch->SubmitAndWait();
        } else {
            recorder->EndGpuTiming(timing, storedBytes);
            recorder->MarkShaderReadsCovered();
            const std::array stamped{std::pair{firstStored, lastStored}};
            recorder->NoteStampedWrites(stamped);
            if (drawn.drawn) noteStore(recorder->LastWriteNote(firstStored, static_cast<std::size_t>(lastStored - firstStored)));
        }
        countStorageWriteBack(storedBytes, true, drawn.drawn);
        if (profile) Profile().storageGpu += timer.lap();
        // The guest bytes now differ from `original`; other caches of the range see the write.
        forgetStored();
        // The keys go the same way as the texels: a fill recorded behind the retile when the
        // metadata is host-imported (no CPU wait for the title's key-writing kernels), else a CPU
        // store (APS5_CPU_DCC_KEYS=1 keeps the CPU store; see DccMetadata.hpp).
        traceKeyStore("layer write-back", descriptor, guestBytes);
        if (!IsDccClear(filledKeys) && uploadedKeys != DccKeys::Uncompressed && !keysReplacedByClear()) MarkDccUncompressed(context, descriptor.dccAddress, guestBytes, DccKeyCount(descriptor, guestBytes));
        uploadedKeys = DccKeys::Uncompressed;
        for (const auto& [from, to] : keep) GuestMemory::MarkWritten(from, static_cast<std::size_t>(to - from));
        // The walk covers this surface's pages only: an adjacent image's later CPU write is stamped
        // newer than this value when its own range is collected. No CPU wrote the pages here
        // (MarkWritten made the only stamps), so the memoized collect is exact.
        settle(true);
        commitStoredSave(saveUnits);
        ++Profile().storageDirectWriteBacks;
        return;
    }
    releaseReadTraps(true);
    // A surface whose memory is no longer registered (freed, or re-registered under another
    // allocation: no readable registered range contains it) has nothing to receive its results;
    // the round trip through the CPU below would store stale texels over whatever the memory
    // holds now. Its selected units are dropped instead. APS5_NO_UNREGISTERED_DROP=1 stores.
    static const bool unregisteredDrop = std::getenv("APS5_NO_UNREGISTERED_DROP") == nullptr;
    bool overhang = false;
    if (unregisteredDrop && !RegisteredReadableCovers(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) {
        const auto allocationEnd = RegisteredReadableEnd(descriptor.baseAddress);
        const auto& level = mips.front();
        const auto rowBytes = std::uint64_t{level.height} * level.pitchBytes;
        const auto lastRowTail = mips.size() == 1 && arrayLayers == 1 && !level.tail && level.tiledSize > rowBytes ? level.tiledSize - rowBytes : 0;
        const auto end = descriptor.baseAddress + guestBytes;
        if (allocationEnd > descriptor.baseAddress && end - allocationEnd <= lastRowTail) {
            auto covered = allocationEnd;
            while (covered < end) {
                const auto next = RegisteredReadableEnd(covered);
                if (next <= covered) break;
                covered = next;
            }
            overhang = covered >= end;
        }
    }
    const bool registered = std::all_of(stores.begin(), stores.end(), [](const auto& range) { return RegisteredReadableCovers(range.first, static_cast<std::size_t>(range.second - range.first)); });
    if (unregisteredDrop && !overhang && !registered) {
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 4) std::fprintf(stderr, "[gpu] storage image 0x%llx+0x%llx: memory no longer registered; %zu pending ranges dropped\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), stores.size());
        unregisteredDropped.fetch_add(1, std::memory_order_relaxed);
        forgetStored();
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (layers[layer]) layerPending[layer] = false;
        }
        return;
    }
    original.resize(static_cast<std::size_t>(guestBytes));
    // The store below compares against the guest bytes, read now when `original` does not hold
    // them (a GPU clear, a dropped or kept unit). Reading them says nothing about the image: a
    // stale unit (dropped, or kept for the CPU) still differs from them, so `original` vouches for
    // the content again only where it did before, or once every unit is stored.
    Require(!unsavedOutsideWatch, unsavedCause);
    std::vector<bool> unsaved = layerOriginal;
    unsaved.flip();
    for (const auto& [begin, end] : unitRuns(unsaved)) {
        if (drawn.rect) {
            GuestMemory::ReadCommitted(descriptor.baseAddress + begin, std::span(original).subspan(static_cast<std::size_t>(begin), static_cast<std::size_t>(end - begin)));
            continue;
        }
        for (const auto& [from, to] : keep) {
            const auto first = std::max(descriptor.baseAddress + begin, from);
            const auto last = std::min(descriptor.baseAddress + end, to);
            if (first < last) GuestMemory::ReadCommitted(first, std::span(original).subspan(static_cast<std::size_t>(first - descriptor.baseAddress), static_cast<std::size_t>(last - first)));
        }
    }
    DeviceBuffer linear(context, static_cast<std::size_t>(sliceLinearBytes * arrayLayers), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    DeviceBuffer tiled(context, original.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Buffer host(context, original.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (profile) Profile().storageAlloc += timer.lap();
    // Start from the uploaded bytes so padding and untouched texels keep their guest values.
    std::memcpy(host.Bytes().data(), original.data(), original.size());
    if (profile) Profile().storageHostCopy += timer.lap();
    detiler.BeginBatch();
    CommandBatch batch(context);
    const auto commands = batch.Handle();
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    CopyBuffer(context, commands, host.Handle(), 0, tiled.Handle(), 0, original.size());
    VkImageMemoryBarrier toSource{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSource.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSource.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.image = image;
    toSource.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSource);
    const auto regions = CopyRegions();
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, linear.Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
    const VkBufferMemoryBarrier toShader[] = {WholeBufferBarrier(linear.Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT), WholeBufferBarrier(tiled.Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 2, toShader, 0, nullptr);
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        for (std::uint32_t level = 0; level < mips.size(); ++level) {
            if (!geometry.HasLayer(level, layer)) continue;
            const auto& mip = mips[level];
            detiler.Dispatch(commands, descriptor.tileMode, elementBytes, linear.Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, tiled.Handle(), geometry.GuestOffset(layer, mip), mip, true, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
        }
    }
    const bool dumpSelected = StorageDumpSelects(descriptor) && StorageDumpReady();
    static int dumps = 0;
    std::unique_ptr<Buffer> dump;
    if (dumpSelected && dumps < 8) {
        dump = std::make_unique<Buffer>(context, static_cast<std::size_t>(mips[0].linearSize * arrayLayers), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            if (geometry.HasLayer(0, layer)) CopyBuffer(context, commands, linear.Handle(), geometry.LinearLayerOffset(layer) + mips[0].linearOffset, dump->Handle(), layer * mips[0].linearSize, mips[0].linearSize);
        }
    }
    const auto toCopy = WholeBufferBarrier(tiled.Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkImageMemoryBarrier backToGeneral = toSource;
    backToGeneral.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    backToGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    backToGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    backToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &toCopy, 1, &backToGeneral);
    CopyBuffer(context, commands, tiled.Handle(), 0, host.Handle(), 0, original.size());
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    APS5_LOG_CHARS_OUT_DEBUG("StorageTexture writeback submit");
    batch.SubmitAndWait();
    APS5_LOG_CHARS_OUT_DEBUG("StorageTexture writeback done");
    std::uint64_t storedBytes = 0;
    for (const auto& [from, to] : stores) storedBytes += to - from;
    countStorageWriteBack(drawn.rect ? storedBytes : guestBytes, false, drawn.drawn);
    if (profile) Profile().storageGpu += timer.lap();
    if (dump) {
        char name[64];
        std::snprintf(name, sizeof(name), "storage_%llx_%d.raw", static_cast<unsigned long long>(descriptor.baseAddress), dumps++);
        if (std::FILE* file = std::fopen(name, "wb")) {
            const std::uint32_t header[3] = {mips[0].pitchBytes / static_cast<std::uint32_t>(elementBytes), mips[0].height, static_cast<std::uint32_t>(storageFormat)};
            std::fwrite(header, sizeof(header), 1, file);
            std::fwrite(dump->Bytes().data(), 1, dump->Bytes().size(), file);
            std::fclose(file);
        }
    }
    // Store only the kept ranges' blocks that changed, so concurrent CPU writes to untouched texels
    // survive; `original` follows for the stored layers (the others' guest bytes are unchanged).
    const auto current = host.Bytes();
    for (const auto& [from, to] : stores) {
        const auto offset = static_cast<std::size_t>(from - descriptor.baseAddress);
        const auto length = static_cast<std::size_t>(to - from);
        GuestMemory::WriteChangedCommitted(from, current.subspan(offset, length), std::span<const std::byte>(original).subspan(offset, length));
        std::memcpy(original.data() + offset, current.data() + offset, length);
    }
    // The texels now hold the whole image, so later reads must see them rather than a fast clear
    // (the keys may be host-imported although the texels were not: then a recorded fill, else a CPU store).
    traceKeyStore("cpu write-back", descriptor, guestBytes);
    if (!IsDccClear(filledKeys) && uploadedKeys != DccKeys::Uncompressed && !keysReplacedByClear()) MarkDccUncompressed(context, descriptor.dccAddress, guestBytes, DccKeyCount(descriptor, guestBytes));
    uploadedKeys = DccKeys::Uncompressed;
    // The store above is the only write to these pages, so `original` is current at a fresh
    // generation, unless blocks were kept for the CPU: then the image is stale there.
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layers[layer]) continue;
        layerOriginal[layer] = !skippedLayer[layer];
        cancelStoredSave(layer);
    }
    // As in the GPU-direct path, but the store's memcpy dirtied this surface's pages in the write
    // watch: a memoized collect would leave them for the next walk to stamp newer than this value
    // (the shared edge block included, undoing the advance), so the walk is made here and
    // consumes them.
    settle(false);
    if (profile) Profile().storageStore += timer.lap();
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
}
