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
bool StorageTexture::skippedResultsInside(std::uint64_t address, std::size_t bytes) const {
    const auto layer = hookSkipLayer;
    if (layer >= trackedLayers || !layerPending[layer] || version != hookSkipVersion || layerGeneration[layer] != hookSkipGeneration) return false;
    const bool whole = storesWhole();
    return whole || (address < layerBegin(layer) + layerBytes(layer) && layerBegin(layer) < address + bytes);
}

StorageTexture::HookSkipCounts StorageTexture::TakeHookSkipCounts() {
    return {flushedAfterSkip.exchange(0, std::memory_order_relaxed), flushedAfterSkipSameSite.exchange(0, std::memory_order_relaxed)};
}

namespace {
struct OwnPendingMovesSlot {};
struct OwnRefreshWorkSlot {};
}

std::uint64_t StorageTexture::OwnPendingMoves() {
    return HostThreadSlot<std::uint64_t, OwnPendingMovesSlot>::Get();
}

std::uint64_t StorageTexture::OwnRefreshWork() {
    return HostThreadSlot<std::uint64_t, OwnRefreshWorkSlot>::Get();
}

void StorageTexture::NoteRefreshWork() {
    HostThreadSlot<std::uint64_t, OwnRefreshWorkSlot>::Set(OwnRefreshWork() + 1);
}

StorageTexture::OrderedSkipCounts StorageTexture::TakeOrderedSkipCounts() {
    return {orderedFlushSkips.exchange(0, std::memory_order_relaxed), orderedRefreshSkips.exchange(0, std::memory_order_relaxed)};
}

void StorageTexture::SetFlushFailureForTest(bool fail) {
    flushFailureForTest.store(fail, std::memory_order_release);
}

int StorageTexture::TakeFlushFailureForTest() {
    return flushFailureFiredForTest.exchange(0, std::memory_order_acq_rel);
}

bool StorageTexture::OrderedFlushArmedForTest() {
    return GpuOrderedFlush();
}

void StorageTexture::BumpPendingSerial() {
    pendingSerial.fetch_add(1, std::memory_order_release);
    HostThreadSlot<std::uint64_t, OwnPendingMovesSlot>::Set(OwnPendingMoves() + 1);
}

bool StorageTexture::ScanPending(std::span<PendingQuery> queries) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    bool identities = true;
    for (auto& query : queries) {
        query.overlaps = false;
        query.found = nullptr;
        if (query.end <= query.begin) continue;
        const auto bytes = static_cast<std::size_t>(query.end - query.begin);
        if (pending.textures.MayOverlap(query.begin, bytes)) {
            for (const auto* texture : pending.textures) {
                if (query.found == nullptr && texture->descriptor.baseAddress == query.begin && texture->guestBytes >= bytes) query.found = texture;
                if (texture != query.except && texture->overlaps(query.begin, bytes)) query.overlaps = true;
            }
        }
        for (const auto* texture : pending.flushing) {
            if (texture != query.except && texture->overlaps(query.begin, bytes)) query.overlaps = true;
        }
        if (query.pending != nullptr && query.found != query.pending) identities = false;
    }
    return identities;
}

std::size_t StorageTexture::DiscardPendingInside(std::uint64_t address, std::size_t bytes) {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    std::size_t discarded = 0;
    const auto end = address + bytes;
    const auto* exempt = Refreshing::Get();
    for (auto it = pending.textures.begin(); it != pending.textures.end();) {
        auto* texture = *it;
        const auto begin = texture->descriptor.baseAddress;
        if (texture != exempt && begin >= address && begin + texture->guestBytes <= end) {
            texture->dirty = false;
            texture->layerPending.assign(texture->trackedLayers, false);
            texture->layerOriginal.assign(texture->trackedLayers, false);
            texture->cancelStoredSaves();
            texture->forgetDrawn();
            it = pending.textures.erase(it);
            ++discarded;
        } else if (texture != exempt && texture->blockUnits && texture->overlaps(address, bytes)) {
            bool dropped = false;
            for (std::uint32_t unit = 0; unit < texture->trackedLayers; ++unit) {
                const auto unitBegin = texture->layerBegin(unit);
                if (!texture->layerPending[unit] || unitBegin < address || unitBegin + texture->layerBytes(unit) > end) continue;
                texture->layerPending[unit] = false;
                texture->layerOriginal[unit] = false;
                texture->cancelStoredSave(unit);
                unitsDropped.fetch_add(1, std::memory_order_relaxed);
                dropped = true;
            }
            if (dropped) ++discarded;
            if (dropped && !texture->anyLayerPending()) {
                texture->dirty = false;
                it = pending.textures.erase(it);
                continue;
            }
            ++it;
        } else {
            ++it;
        }
    }
    if (discarded != 0) BumpPendingSerial();
    return discarded;
}

StorageTexture::FillCoverage StorageTexture::ClassifyFill(std::uint64_t address, std::size_t bytes) {
    FillCoverage coverage;
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    std::vector<StorageTexture*> overlapping;
    for (auto* texture : live.textures) {
        if (texture->overlapsLive(address, bytes)) overlapping.push_back(texture);
    }
    const auto end = address + bytes;
    StorageTexture* covered = nullptr;
    for (auto* texture : overlapping) {
        if (texture->descriptor.baseAddress == address && texture->guestBytes == bytes) {
            covered = texture;
            coverage.cover = FillCover::Exact;
            coverage.layer = WholeImage;
        }
    }
    static const bool layerMemo = std::getenv("APS5_NO_LAYER_REFUSAL_MEMO") == nullptr;
    for (auto* texture : overlapping) {
        if (covered != nullptr) break;
        const auto& geometry = texture->geometry;
        const auto base = texture->descriptor.baseAddress;
        if (geometry.layers < 2 || geometry.thick || geometry.imageDepth != 1 || geometry.layers != geometry.imageLayers || geometry.layerBytes != bytes || geometry.layerBytes * geometry.layers != texture->guestBytes || address < base) continue;
        if (layerMemo && texture->layerRefusedGeneration == texture->generation) continue;
        const auto offset = address - base;
        if (offset % bytes != 0 || offset / bytes >= geometry.layers) continue;
        covered = texture;
        coverage.cover = FillCover::Layer;
        coverage.layer = static_cast<std::uint32_t>(offset / bytes);
    }
    if (covered != nullptr) {
        coverage.image = covered->weak_from_this().lock();
        if (coverage.image == nullptr) {
            coverage.cover = FillCover::None;
        } else {
            for (const auto* texture : overlapping) {
                if (texture == covered) continue;
                ++coverage.others;
                if (texture->descriptor.baseAddress >= address && texture->descriptor.baseAddress + texture->guestBytes <= end) ++coverage.inside;
            }
        }
    } else if (std::any_of(live.textures.begin(), live.textures.end(), [&](const StorageTexture* texture) { return texture->keysFillMatches(address, bytes, !overlapping.empty()); })) {
        coverage.cover = FillCover::Keys;
    } else if (overlapping.size() > 1) {
        coverage.cover = FillCover::Several;
    } else if (overlapping.size() == 1) {
        auto* single = overlapping.front();
        const auto begin = single->descriptor.baseAddress;
        const auto stop = begin + single->guestBytes;
        if (address >= begin && end <= stop) coverage.cover = FillCover::Inside;
        else if (address <= begin && end >= stop) coverage.cover = FillCover::Around;
        else coverage.cover = FillCover::Straddle;
    }
    static const bool trace = std::getenv("APS5_TRACE_FILL_COVER") != nullptr;
    static int traced = 0;
    const auto cover = coverage.cover;
    if (trace && (cover == FillCover::Inside || cover == FillCover::Around || cover == FillCover::Straddle || cover == FillCover::Several) && traced < 96) {
        ++traced;
        std::fprintf(stderr, "[fill-cover] fill 0x%llx+0x%zx meets %zu image(s):", static_cast<unsigned long long>(address), bytes, overlapping.size());
        for (const auto* texture : overlapping) {
            const auto& d = texture->descriptor;
            std::fprintf(stderr, " [0x%llx+0x%llx %ux%u mips %u layers %u dim %d tile %d format %u dcc 0x%llx%s]", static_cast<unsigned long long>(d.baseAddress), static_cast<unsigned long long>(texture->guestBytes), d.width, d.height, d.mipCount, texture->geometry.imageLayers, static_cast<int>(d.dimension), static_cast<int>(d.tileMode), d.format, static_cast<unsigned long long>(d.dccAddress), texture->dirty ? " dirty" : "");
        }
        std::fprintf(stderr, "\n");
    }
    return coverage;
}

std::size_t StorageTexture::NoteKeysFill(std::uint64_t address, std::size_t bytes, std::uint8_t key) {
    static const bool refillDisabled = std::getenv("APS5_NO_KEYS_REFILL") != nullptr;
    DccKeys keys = DccKeys::Mixed;
    switch (key) {
        case 0x00: keys = DccKeys::Clear0000; break;
        case 0x40: keys = DccKeys::Clear0001; break;
        case 0x80: keys = DccKeys::Clear1110; break;
        case 0xc0: keys = DccKeys::Clear1111; break;
        case 0x20: keys = DccKeys::ClearRegister; break;
        case 0xff: keys = DccKeys::Uncompressed; break;
        default: return 0;
    }
    static const bool traceKeys = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    const bool overlapped = std::any_of(live.textures.begin(), live.textures.end(), [&](const StorageTexture* texture) { return texture->overlapsLive(address, bytes); });
    std::size_t covered = 0;
    for (auto* texture : live.textures) {
        if (!texture->keysFillMatches(address, bytes, overlapped)) continue;
        texture->filledKeys = keys;
        ++covered;
        if (traceKeys && IsDccClear(keys)) std::fprintf(stderr, "[dcc-keys] %s key fill over 0x%llx+0x%llx (uploaded %s, dirty %d)\n", DccKeysName(keys), static_cast<unsigned long long>(texture->descriptor.baseAddress), static_cast<unsigned long long>(texture->guestBytes), DccKeysName(texture->uploadedKeys), texture->dirty ? 1 : 0);
        if (refillDisabled || !IsDccClear(keys) || texture->uploadedKeys != keys) continue;
        texture->uploadedKeys = DccKeys::Uncompressed;
    }
    return covered;
}

std::size_t StorageTexture::ClearByKeysFill(std::uint64_t address, std::size_t bytes, std::uint8_t key) {
    static const bool enabled = [] { const char* text = std::getenv("APS5_KEYS_FILL_CLEAR"); return text != nullptr && std::strcmp(text, "1") == 0; }();
    if (!enabled) return 0;
    DccKeys keys = DccKeys::Mixed;
    switch (key) {
        case 0x00: keys = DccKeys::Clear0000; break;
        case 0x40: keys = DccKeys::Clear0001; break;
        case 0x80: keys = DccKeys::Clear1110; break;
        case 0xc0: keys = DccKeys::Clear1111; break;
        case 0x20: keys = DccKeys::ClearRegister; break;
        default: return 0;
    }
    std::vector<std::shared_ptr<StorageTexture>> covered;
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        const bool overlapped = std::any_of(live.textures.begin(), live.textures.end(), [&](const StorageTexture* texture) { return texture->overlapsLive(address, bytes); });
        for (auto* texture : live.textures) {
            if (!texture->keysFillMatches(address, bytes, overlapped)) continue;
            if (auto shared = texture->weak_from_this().lock()) covered.push_back(std::move(shared));
        }
    }
    std::size_t cleared = 0;
    for (const auto& texture : covered) {
        if (texture->clearByKeysFill(keys, key)) ++cleared;
    }
    return cleared;
}

bool StorageTexture::clearByKeysFill(DccKeys keys, std::uint8_t key) {
    static const bool traceKeys = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    VkClearColorValue clearValue{};
    auto* recorder = Recorder::Active();
    const char* refusal = !ClearColorFor(storageFormat, keys, clearValue) ? (keys == DccKeys::ClearRegister ? "clear-register code" : "no clear value in the storage format") : recorder == nullptr ? "no recorder" : nullptr;
    if (refusal != nullptr) {
        static std::atomic<bool> reported{false};
        if (!reported.exchange(true)) std::fprintf(stderr, "[dcc-keys] key fill 0x%02x (%s) over surface 0x%llx+0x%llx (guest format %u, vk format %d, keys 0x%llx) not applied at once: %s\n", key, DccKeysName(keys), static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), descriptor.format, static_cast<int>(storageFormat), static_cast<unsigned long long>(descriptor.dccAddress), refusal);
        return false;
    }
    std::size_t droppedUnits = 0;
    const bool wasDirty = dirty;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        droppedUnits = layerPending.Count();
        layerPending.assign(trackedLayers, false);
        if (dirty) {
            dirty = false;
            pending.textures.remove(this);
            BumpPendingSerial();
        }
    }
    const auto commands = recorder->Commands();
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::DccClear);
    if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
    Recorder::CountBarriers(Recorder::CommandClass::DccClear, 2);
    if (Recorder::BarrierValidate()) {
        const std::pair<VkImage, bool> clearedImage{image, true};
        recorder->NoteAccess(Recorder::CommandClass::DccClear, Recorder::Access{{}, {}, std::span(&clearedImage, 1), VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    VkImageMemoryBarrier toClear{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toClear.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.image = image;
    toClear.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toClear);
    clearImage(commands, clearValue, toClear.subresourceRange);
    VkImageMemoryBarrier toGeneral = toClear;
    toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    recorder->EndGpuTiming(timing, guestBytes);
    layerOriginal.assign(trackedLayers, false);
    cancelStoredSaves();
    releaseUnheldOriginal();
    uploadedKeys = keys;
    filledKeys = DccKeys::Uncompressed;
    keyProof = {};
    forgetBorrowed(0, trackedLayers);
    forgetUnwatchHash(0, trackedLayers);
    const auto now = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) layerGeneration[layer] = collectedLayerGeneration(layer, now);
    refreshGeneration();
    ++version;
    if (traceKeys) std::fprintf(stderr, "[dcc-keys] key fill %s clears 0x%llx+0x%llx at once (keys 0x%llx, %zu pending units dropped, dirty %d)\n", DccKeysName(keys), static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), static_cast<unsigned long long>(descriptor.dccAddress), droppedUnits, wasDirty ? 1 : 0);
    return true;
}

bool StorageTexture::FillClear(std::span<const std::uint32_t, 4> pattern, std::uint32_t layer, const char*& refusal, bool allowRegisterClear) {
    static const bool trace = std::getenv("APS5_TRACE_FILL_COVER") != nullptr;
    struct TraceLayerRefusal {
        const StorageTexture& texture;
        std::span<const std::uint32_t, 4> pattern;
        std::uint32_t layer;
        const char*& refusal;
        bool cleared = false;
        ~TraceLayerRefusal() {
            static std::atomic<int> traced{0};
            if (cleared || !trace || layer == WholeImage || refusal == nullptr || traced.fetch_add(1) >= 64) return;
            const auto& d = texture.descriptor;
            std::fprintf(stderr, "[fill-cover] layer %u of 0x%llx+0x%llx (%ux%u mips %u layers %u, %llu bytes per layer, format %u tile %d dcc 0x%llx%s) refused: %s; pattern %08x %08x %08x %08x; generation %llu\n", layer, static_cast<unsigned long long>(d.baseAddress), static_cast<unsigned long long>(texture.guestBytes), d.width, d.height, d.mipCount, texture.geometry.imageLayers, static_cast<unsigned long long>(texture.geometry.layerBytes), d.format, static_cast<int>(d.tileMode), static_cast<unsigned long long>(d.dccAddress), texture.dirty ? ", dirty" : "", refusal, pattern[0], pattern[1], pattern[2], pattern[3], static_cast<unsigned long long>(texture.generation));
        }
    } traceRefusal{*this, pattern, layer, refusal};
    VkClearColorValue clearValue{};
    const auto elementBytes = BytesPerElement(descriptor.format);
    if (!FillClearColor(storageFormat, elementBytes, pattern, clearValue) && !(ClearKeepsDenormals(context, storageFormat) && FillClearColor(storageFormat, elementBytes, pattern, clearValue, true))) {
        refusal = "pattern";
        return false;
    }
    if (layer != WholeImage && (layer >= geometry.imageLayers || geometry.thick || geometry.imageDepth != 1)) {
        refusal = "layer";
        return false;
    }
    if (descriptor.dccAddress != 0) {
        if (!allowRegisterClear && Recorder::SnapshotWriteOverlaps(descriptor.dccAddress, static_cast<std::size_t>(guestBytes / 256))) {
            refusal = "keys pending";
            return false;
        }
        if (TextureClearKeys(descriptor, guestBytes) != DccKeys::Uncompressed) {
            refusal = "keys";
            return false;
        }
    }
    auto* recorder = Recorder::Active();
    if (recorder == nullptr) {
        refusal = "no recorder";
        return false;
    }
    const auto begin = layer == WholeImage ? descriptor.baseAddress : descriptor.baseAddress + geometry.GuestLayerOffset(layer);
    const auto bytes = static_cast<std::size_t>(layer == WholeImage ? guestBytes : geometry.layerBytes);
    if (layer != WholeImage && trackedLayers == 1) {
        GuestMemory::CollectWritesUncached(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
        const auto before = static_cast<std::size_t>(begin - descriptor.baseAddress);
        const auto after = static_cast<std::size_t>(descriptor.baseAddress + guestBytes - (begin + bytes));
        if ((before != 0 && !GuestMemory::UnchangedSince(descriptor.baseAddress, before, generation)) || (after != 0 && !GuestMemory::UnchangedSince(begin + bytes, after, generation))) {
            refusal = "surface changed";
            layerRefusedGeneration = generation;
            return false;
        }
    }
    if (layer != WholeImage && blockUnits && ((begin - descriptor.baseAddress) % trackedLayerBytes != 0 || bytes % trackedLayerBytes != 0)) {
        refusal = "layer alignment";
        return false;
    }
    bool aliasedKeys = false;
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        for (auto* texture : live.textures) {
            if (texture == this || texture->released || !texture->overlaps(begin, bytes)) continue;
            texture->forgetSavedOver(begin, bytes);
            if (texture->descriptor.dccAddress != 0) aliasedKeys = true;
        }
    }
    static std::atomic<int> traced{0};
    if (trace && (descriptor.dccAddress != 0 || aliasedKeys) && traced.fetch_add(1) < 16) std::fprintf(stderr, "[fill-cover] clear of surface 0x%llx+0x%llx (format %u, keys 0x%llx%s): pattern %08x %08x %08x %08x\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), descriptor.format, static_cast<unsigned long long>(descriptor.dccAddress), aliasedKeys ? ", another image over the memory has keys" : "", pattern[0], pattern[1], pattern[2], pattern[3]);
    PublishShadow(begin, bytes, PublishScope::PartialUnits, PublishReason::FillClear);
    GuestMemory::MarkWritten(begin, bytes);
    const auto commands = recorder->Commands();
    const auto timing = recorder->BeginGpuTiming(Recorder::CommandClass::FillClear);
    Recorder::CountBarriers(Recorder::CommandClass::FillClear, 2);
    if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
    if (Recorder::BarrierValidate()) {
        const std::pair<VkImage, bool> cleared{image, true};
        recorder->NoteAccess(Recorder::CommandClass::FillClear, Recorder::Access{{}, {}, std::span(&cleared, 1), VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    VkImageMemoryBarrier toClear{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toClear.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toClear.oldLayout = layer == WholeImage ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.image = image;
    toClear.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, layer == WholeImage ? 0u : layer, layer == WholeImage ? geometry.imageLayers : 1u};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toClear);
    clearImage(commands, clearValue, toClear.subresourceRange);
    VkImageMemoryBarrier toGeneral = toClear;
    toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    recorder->EndGpuTiming(timing, bytes);
    traceRefusal.cleared = true;
    const bool watched = GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    if (watched) {
        layerOriginal.assign(trackedLayers, false);
        cancelStoredSaves();
        releaseUnheldOriginal();
    }
    uploadedKeys = DccKeys::Uncompressed;
    if (layer != WholeImage && trackedLayers > 1) {
        const auto first = blockUnits ? static_cast<std::uint32_t>((begin - descriptor.baseAddress) / trackedLayerBytes) : layer;
        const auto count = blockUnits ? static_cast<std::uint32_t>(bytes / trackedLayerBytes) : 1u;
        if (!watched) {
            std::vector<bool> filled(trackedLayers, false);
            std::fill_n(filled.begin() + first, count, true);
            captureGuestBytes(&filled);
        }
        const auto now = GuestMemory::CollectWritesUncached(begin, bytes);
        for (auto unit = first; unit < first + count; ++unit) layerGeneration[unit] = now;
        refreshGeneration();
        forgetBorrowed(first, count);
        markLayersPending(first, count);
        return true;
    }
    if (!watched) captureGuestBytes(nullptr);
    const auto now = GuestMemory::CollectWritesUncached(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) layerGeneration[layer] = collectedLayerGeneration(layer, now, true);
    refreshGeneration();
    forgetBorrowed(0, trackedLayers);
    MarkDirty();
    return true;
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
