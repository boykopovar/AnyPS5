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
std::vector<VkBufferImageCopy> StorageTexture::CopyRegions(const std::vector<bool>* layers) const {
    std::vector<VkBufferImageCopy> regions;
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        if (layers != nullptr && !(*layers)[layer]) continue;
        for (std::uint32_t level = 0; level < descriptor.mipCount; ++level) {
            if (!geometry.HasLayer(level, layer)) continue;
            const auto& mip = mips[level];
            VkBufferImageCopy region{};
            region.bufferOffset = layer * sliceLinearBytes + mip.linearOffset;
            region.bufferRowLength = mip.pitchBytes / BytesPerElement(descriptor.format) * BlockWidth(descriptor.format);
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, geometry.CopyLayer(layer), 1};
            region.imageOffset = {0, 0, geometry.CopyDepth(layer)};
            region.imageExtent = {std::max(descriptor.width >> level, 1u), std::max(descriptor.height >> level, 1u), 1u};
            regions.push_back(region);
        }
    }
    return regions;
}

bool StorageTexture::overlaps(std::uint64_t address, std::size_t bytes) const {
    return address < descriptor.baseAddress + guestBytes && descriptor.baseAddress < address + bytes;
}

bool StorageTexture::overlapsLive(std::uint64_t address, std::size_t bytes) const {
    return !released && guestBytes != 0 && overlaps(address, bytes);
}

bool StorageTexture::keysFillMatches(std::uint64_t address, std::size_t bytes, bool overlapped) const {
    constexpr std::uint64_t keyBytes = 256;
    if (released || descriptor.dccAddress != address || guestBytes / keyBytes == 0 || bytes < guestBytes / keyBytes) return false;
    return !overlapped || bytes <= DccKeyCount(descriptor, guestBytes);
}

bool StorageTexture::keysReplacedByClear() const {
    const auto keyCount = DccKeyBytes(guestBytes);
    if (keyCount == 0 || keyCount != DccKeyCount(descriptor, guestBytes) || keyProof.generation == 0 || keyProof.keys != uploadedKeys || !GuestMemory::GpuMutex().HeldByThisThread()) return false;
    if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(descriptor.dccAddress, keyCount)) return false;
    if (!GuestMemory::CpuWroteSince(descriptor.dccAddress, keyCount, keyProof.generation)) return false;
    const auto keys = ReadDccKeys(descriptor.dccAddress, guestBytes);
    return IsDccClear(keys) && keys != uploadedKeys;
}

void StorageTexture::MarkDirty() {
    if (descriptor.dccAddress != 0 && (IsDccClear(uploadedKeys) || uploadedKeys == DccKeys::ClearSingle) && !IsDccClear(filledKeys)) {
        traceKeyStore("first write", descriptor, guestBytes);
        if (!keysReplacedByClear()) MarkDccUncompressed(context, descriptor.dccAddress, guestBytes, DccKeyCount(descriptor, guestBytes));
        uploadedKeys = DccKeys::Uncompressed;
    }
    markLayersPending(0, trackedLayers);
}

void StorageTexture::MarkSliceDirty(std::uint32_t slice, std::uint32_t count) {
    if (geometry.thick || geometry.imageLayers <= 1 || count == 0 || slice >= geometry.imageLayers || count > geometry.imageLayers - slice || geometry.layerBytes == 0 || trackedLayerBytes == 0 || descriptor.dccAddress != 0) {
        MarkDirty();
        return;
    }
    const auto begin = geometry.GuestLayerOffset(slice);
    const auto first = static_cast<std::uint32_t>(begin / trackedLayerBytes);
    const auto last = static_cast<std::uint32_t>(std::min<std::uint64_t>((begin + geometry.layerBytes * count + trackedLayerBytes - 1) / trackedLayerBytes, trackedLayers));
    if (first >= last) {
        MarkDirty();
        return;
    }
    markLayersPending(first, last - first);
}

bool StorageTexture::anyLayerPending() const {
    return layerPending.Any();
}

std::uint64_t StorageTexture::advanceUnchanged(std::uint32_t unit, std::uint64_t to) {
    if (unit < unwatchHash.size() && unwatchHash[unit].generation != 0 && unwatchHash[unit].generation == layerGeneration[unit]) unwatchHash[unit].generation = to;
    return to;
}

void StorageTexture::refreshGeneration() {
    generation = *std::min_element(layerGeneration.begin(), layerGeneration.end());
}

std::uint64_t StorageTexture::GenerationFor(std::uint64_t address, std::uint64_t bytes) const {
    if (layerBehind.empty() && generation != 0) return generation;
    std::optional<std::uint64_t> oldest;
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        const auto begin = layerBegin(layer);
        if (address >= begin + layerBytes(layer) || begin >= address + bytes) continue;
        if (!layerBehind.empty() && layerBehind[layer]) return 0;
        oldest = std::min(oldest.value_or(layerGeneration[layer]), layerGeneration[layer]);
    }
    return oldest.value_or(generation);
}

std::uint64_t StorageTexture::collectedLayerGeneration(std::uint32_t layer, std::uint64_t whole, bool uncached) const {
    if (whole != 0 || trackedLayers <= 1) return whole;
    const auto begin = layerBegin(layer);
    const auto bytes = static_cast<std::size_t>(layerBytes(layer));
    return uncached ? GuestMemory::CollectWritesUncached(begin, bytes) : GuestMemory::CollectWrites(begin, bytes);
}

void StorageTexture::noteLayersBehind(std::uint32_t firstLayer, std::uint32_t lastLayer, const std::vector<bool>& outside) {
    if (layerBehind.size() != trackedLayers) layerBehind.assign(trackedLayers, false);
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (layer >= firstLayer && layer < lastLayer) layerBehind[layer] = false;
        else if (layer < outside.size() && outside[layer]) layerBehind[layer] = true;
    }
    const auto layersEnd = layerBegin(lastLayer - 1) + layerBytes(lastLayer - 1);
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 2> ranges{{{descriptor.baseAddress, layerBegin(firstLayer)}, {layersEnd, descriptor.baseAddress + guestBytes}}};
    for (const auto& [from, to] : ranges) {
        if (to <= from) continue;
        for (const auto& other : overlappingPending(from, static_cast<std::size_t>(to - from))) {
            const bool whole = other->storesWhole();
            for (std::uint32_t unit = 0; unit < other->trackedLayers; ++unit) {
                if (!whole && !other->layerPending[unit]) continue;
                const auto begin = std::max(from, other->layerBegin(unit));
                const auto end = std::min(to, other->layerBegin(unit) + other->layerBytes(unit));
                if (end <= begin) continue;
                const auto first = static_cast<std::uint32_t>((begin - descriptor.baseAddress) / trackedLayerBytes);
                const auto last = static_cast<std::uint32_t>(std::min<std::uint64_t>((end - descriptor.baseAddress + trackedLayerBytes - 1) / trackedLayerBytes, trackedLayers));
                for (auto layer = first; layer < last; ++layer) layerBehind[layer] = true;
            }
        }
    }
    if (std::none_of(layerBehind.begin(), layerBehind.end(), [](bool behind) { return behind; })) layerBehind.clear();
}

namespace {

struct WriteRecords {
    std::mutex mutex;
    std::unordered_map<std::uint64_t, StorageTexture::WriteRecord> byAddress;
    std::uint64_t serial = 0;
};

WriteRecords& LayoutWrites() {
    static WriteRecords records;
    return records;
}

}

std::optional<StorageTexture::WriteRecord> StorageTexture::LastWrite(std::uint64_t address) {
    auto& records = LayoutWrites();
    std::lock_guard lock(records.mutex);
    const auto found = records.byAddress.find(address);
    if (found == records.byAddress.end()) return std::nullopt;
    return found->second;
}

namespace {
std::atomic<std::uint64_t> flipSerial{0};
std::atomic<std::uint64_t> occasionalLabelStores{0};
}

void StorageTexture::NoteFlip() {
    flipSerial.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t StorageTexture::OccasionalLabelStores() {
    return occasionalLabelStores.load(std::memory_order_relaxed);
}

void StorageTexture::markLayersPending(std::uint32_t first, std::uint32_t count) {
    if (count != 0) GuestMemory::ForgetKnownZero(layerBegin(first), static_cast<std::size_t>(layerBegin(first + count - 1) + layerBytes(first + count - 1) - layerBegin(first)));
    {
        auto& records = LayoutWrites();
        std::lock_guard lock(records.mutex);
        if (records.byAddress.size() >= 65536u) records.byAddress.clear();
        records.byAddress[descriptor.baseAddress] = {descriptor.width, descriptor.height, descriptor.tileMode, descriptor.dimension, descriptor.format, ++records.serial};
    }
    GuestMemory::NoteGpuPending(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    CaptureTrace::Log("image-write image=%llx first=%u count=%u generation=%llu", static_cast<unsigned long long>(descriptor.baseAddress), first, count, static_cast<unsigned long long>(generation));
    ++writes;
    static const bool eager = std::getenv("APS5_EAGER_WRITEBACK") != nullptr || std::getenv("APS5_NO_TEXTURE_CACHE") != nullptr;
    if (const auto flip = flipSerial.load(std::memory_order_relaxed); writes == 1u || flip != writtenFlip) {
        writtenOccasionally = writes == 1u || flip - writtenFlip > OccasionalWriteFlips;
        writtenFlip = flip;
    }
    std::optional<MapImportGate::PublishHold> publication;
    if (!layerPending.Any()) publication.emplace(descriptor.baseAddress, guestBytes);
    const bool newUnits = !layerPending.AllIn(first, count);
    if (newUnits) {
        std::vector<bool> unsaved;
        bool anyUnsaved = false;
        const bool trims = CommitTrims();
        if (trims) {
            for (std::uint32_t layer = first; layer < first + count && !anyUnsaved; ++layer) anyUnsaved = !layerPending[layer] && !layerOriginal[layer] && (layerGeneration[layer] == 0 || savedTicket[layer] == 0 || keptForCpu(layer));
        }
        if (!trims || anyUnsaved) {
            unsaved.assign(trackedLayers, false);
            anyUnsaved = false;
            for (std::uint32_t layer = first; layer < first + count; ++layer) {
                if (layerPending[layer]) continue;
                if (layerOriginal[layer]) {
                    pendingMiss[layer] = BaselineMiss::SavedBytesDropped;
                    continue;
                }
                auto miss = BaselineMiss::LeftWatchWhilePending;
                if (layerGeneration[layer] != 0 && savedTicket[layer] != 0) miss = BaselineMiss::SaveInFlight;
                else if (layerGeneration[layer] != 0 && !GuestMemory::Watched(layerBegin(layer), static_cast<std::size_t>(layerBytes(layer)))) {
                    awaitRebase(layer);
                    miss = baselineMiss(layer);
                }
                unsaved[layer] = keptForCpu(layer) || miss == BaselineMiss::LeftWatchWhilePending || miss == BaselineMiss::None;
                pendingMiss[layer] = miss == BaselineMiss::None ? BaselineMiss::SavedBytesDropped : miss;
                anyUnsaved = anyUnsaved || unsaved[layer];
            }
        } else {
            for (std::uint32_t layer = first; layer < first + count; ++layer) {
                if (!layerPending[layer]) pendingMiss[layer] = layerOriginal[layer] ? BaselineMiss::SavedBytesDropped : BaselineMiss::SaveInFlight;
            }
        }
        if (anyUnsaved && !GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) {
            captureGuestBytes(&unsaved, true);
            for (std::uint32_t layer = first; layer < first + count; ++layer) {
                if (unsaved[layer]) pendingMiss[layer] = BaselineMiss::SavedBytesDropped;
            }
        }
        const auto collectFrom = layerBegin(first);
        const auto collectBytes = static_cast<std::size_t>(layerBegin(first + count - 1) + layerBytes(first + count - 1) - layerBegin(first));
        if (const auto hook = pendingHookForTest.load(std::memory_order_acquire)) hook(collectFrom, collectBytes);
        if (RuntimeSwitches::Set(RuntimeSwitch::NoMemoPendingCollect)) GuestMemory::CollectWritesUncached(collectFrom, collectBytes);
        else GuestMemory::CollectWrites(collectFrom, collectBytes);
        if (!completionKept.empty()) {
            for (std::uint32_t layer = first; layer < first + count; ++layer) {
                if (layerPending[layer]) continue;
                const auto end = layerBegin(layer) + layerBytes(layer);
                for (auto at = layerBegin(layer) & ~std::uint64_t{65535}; at < end; at += 65536) completionKept[static_cast<std::size_t>(at / 65536 - descriptor.baseAddress / 65536)] = 0;
            }
        }
        layerPending.SetRange(first, count, true);
    }
    const auto pendingSince = GuestMemory::TrackerGeneration();
    StampUnkeptUnits(keptAtUnwatch, {descriptor.baseAddress, trackedLayerBytes, guestBytes}, layerGeneration, layerPendingSince, first, count, pendingSince);
    if (auto source = borrowedUnits.size() == trackedLayers && borrowedUnits.AnyIn(first, count) ? borrowedFrom.lock() : nullptr; source != nullptr) {
        bool superseded = false;
        for (std::uint32_t unit = first; unit < first + count; ++unit) {
            if (!borrowedUnits[unit]) continue;
            borrowedUnits[unit] = false;
            if (source->version != borrowedVersion || !source->layerPending[unit]) continue;
            source->layerPending[unit] = false;
            unitsSuperseded.fetch_add(1, std::memory_order_relaxed);
            superseded = true;
        }
        if (superseded) source->reconcilePending();
    }
    if (!handedOver.empty() || !handedUnits.empty()) supersedeHandedOver(first, count);
    if (eager) {
        publication.reset();
        WriteBack();
        return;
    }
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    if (!drawingTarget) {
        pendingFromOthers = true;
        if (drawnListed) drawnWhole = true;
    }
    if (Recorder::Active() == nullptr) {
        static const bool hooked = [] {
            GuestMemory::SetFlushHook(&FlushHook);
            return true;
        }();
        static_cast<void>(hooked);
    }
    ++version;
    if (const auto format = ResolveTextureFormat(descriptor.format); format == VK_FORMAT_R8_UINT || format == VK_FORMAT_R8_UNORM) eightBitWrites().fetch_add(1, std::memory_order_release);
    const bool wasLent = std::exchange(lent, false);
    if (dirty) {
        if (wasLent || newUnits) BumpPendingSerial();
        return;
    }
    dirty = true;
    pending.textures.push_back(this);
    BumpPendingSerial();
}

void StorageTexture::reconcilePending() {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    const bool any = anyLayerPending();
    if (!any) forgetDrawn();
    if (any == dirty) return;
    dirty = any;
    if (any) pending.textures.push_back(this);
    else pending.textures.remove(this);
    BumpPendingSerial();
}

bool StorageTexture::Dirty() const {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    return dirty;
}

constexpr std::size_t MaxDrawnRects = 16;

bool StorageTexture::MarkDrawn(std::uint32_t slice, std::uint32_t count, VkRect2D scissor) {
    struct Drawing {
        bool& flag;
        bool previous;
        ~Drawing() { flag = previous; }
    } drawing{drawingTarget, std::exchange(drawingTarget, true)};
    MarkSliceDirty(slice, count);
    const auto clip = [](std::int64_t value, std::uint32_t limit) { return static_cast<std::uint32_t>(std::clamp<std::int64_t>(value, 0, limit)); };
    const auto left = clip(scissor.offset.x, descriptor.width);
    const auto top = clip(scissor.offset.y, descriptor.height);
    const auto right = clip(std::int64_t{scissor.offset.x} + scissor.extent.width, descriptor.width);
    const auto bottom = clip(std::int64_t{scissor.offset.y} + scissor.extent.height, descriptor.height);
    const bool rectangle = proxyImage == VK_NULL_HANDLE && descriptor.tileMode == TextureTileMode::kLinear && !blockUnits && trackedLayers == 1 && descriptor.mipCount == 1 && arrayLayers == 1 && geometry.imageLayers == 1 && geometry.imageDepth == 1 && !geometry.thick && descriptor.dccAddress == 0;
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    if (!anyLayerPending()) return true;
    if (!drawnListed) {
        drawnListed = true;
        drawnWhole = false;
        drawnRects.clear();
        pending.drawn.push_back({this, guestBytes, std::chrono::steady_clock::now()});
        pending.drawnCount.store(pending.drawn.size(), std::memory_order_release);
    }
    if (!rectangle || pendingFromOthers) drawnWhole = true;
    if (!drawnWhole && right > left && bottom > top) addDrawnRect({left, top, right, bottom});
    return !drawnWhole && drawnRects.size() <= MaxDrawnRects;
}

void StorageTexture::addDrawnRect(DrawnRect rect) {
    const auto inside = [](const DrawnRect& outer, const DrawnRect& inner) { return outer.left <= inner.left && outer.top <= inner.top && inner.right <= outer.right && inner.bottom <= outer.bottom; };
    if (std::any_of(drawnRects.begin(), drawnRects.end(), [&](const DrawnRect& kept) { return inside(kept, rect); })) return;
    for (auto kept = drawnRects.begin(); kept != drawnRects.end();) {
        const bool columns = kept->left == rect.left && kept->right == rect.right && kept->top <= rect.bottom && rect.top <= kept->bottom;
        const bool rows = kept->top == rect.top && kept->bottom == rect.bottom && kept->left <= rect.right && rect.left <= kept->right;
        if (!columns && !rows && !inside(rect, *kept)) {
            ++kept;
            continue;
        }
        rect = {std::min(kept->left, rect.left), std::min(kept->top, rect.top), std::max(kept->right, rect.right), std::max(kept->bottom, rect.bottom)};
        drawnRects.erase(kept);
        kept = drawnRects.begin();
    }
    drawnRects.push_back(rect);
}

void StorageTexture::forgetDrawn() {
    pendingFromOthers = false;
    if (!drawnListed) return;
    drawnListed = false;
    drawnWhole = false;
    drawnRects.clear();
    auto& pending = Pending();
    std::erase_if(pending.drawn, [this](const PendingWrites::Drawn& entry) { return entry.texture == this; });
    pending.drawnCount.store(pending.drawn.size(), std::memory_order_release);
}

void StorageTexture::noteStore(std::uint64_t note) {
    if (note == 0) return;
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    storeNotes.push_back(note);
    if (storeNotes.size() > 8) storeNotes.erase(storeNotes.begin());
}

StorageTexture::DrawnArea StorageTexture::drawnArea() const {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    if (!drawnListed) return {};
    DrawnArea area{true, !drawnWhole && !pendingFromOthers};
    if (!area.rect || drawnRects.empty()) return area;
    area.left = area.top = std::numeric_limits<std::uint32_t>::max();
    for (const auto& rect : drawnRects) {
        area.left = std::min(area.left, rect.left);
        area.top = std::min(area.top, rect.top);
        area.right = std::max(area.right, rect.right);
        area.bottom = std::max(area.bottom, rect.bottom);
    }
    area.rects = drawnRects;
    return area;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> StorageTexture::drawnSpans(const DrawnArea& area, std::span<const std::pair<std::uint64_t, std::uint64_t>> keep) const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> spans;
    if (area.rects.empty()) return spans;
    const auto& mip = mips.front();
    const auto elementBytes = static_cast<std::uint64_t>(BytesPerElement(descriptor.format));
    const auto rowBase = descriptor.baseAddress + mip.tiledOffset;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> columns;
    std::size_t next = 0;
    for (auto row = area.top; row < area.bottom; ++row) {
        columns.clear();
        for (const auto& rect : area.rects) {
            if (rect.top <= row && row < rect.bottom) columns.emplace_back(rect.left, rect.right);
        }
        std::sort(columns.begin(), columns.end());
        const auto rowStart = rowBase + std::uint64_t{row} * mip.pitchBytes;
        for (std::size_t first = 0; first < columns.size();) {
            auto right = columns[first].second;
            auto last = first + 1;
            for (; last < columns.size() && columns[last].first <= right; ++last) right = std::max(right, columns[last].second);
            const auto rowBegin = rowStart + columns[first].first * elementBytes;
            const auto rowEnd = rowStart + right * elementBytes;
            first = last;
            while (next < keep.size() && keep[next].second <= rowBegin) ++next;
            for (auto index = next; index < keep.size() && keep[index].first < rowEnd; ++index) {
                const auto from = std::max(rowBegin, keep[index].first);
                const auto to = std::min(rowEnd, keep[index].second);
                if (from < to) spans.emplace_back(from, to);
            }
        }
    }
    return spans;
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
