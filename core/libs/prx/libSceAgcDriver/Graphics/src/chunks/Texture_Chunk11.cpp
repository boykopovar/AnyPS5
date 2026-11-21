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
std::uint64_t StorageTexture::storeDrawnRect(const DrawnArea& area, std::span<const ImportSegment> segments, std::span<const std::pair<std::uint64_t, std::uint64_t>> stores, const std::vector<std::uint32_t>& saveUnits) {
    const auto& mip = mips.front();
    const auto elementBytes = static_cast<std::uint64_t>(BytesPerElement(descriptor.format));
    const auto rowBase = descriptor.baseAddress + mip.tiledOffset;
    const auto width = area.right - area.left;
    const auto height = area.bottom - area.top;
    const auto rowBytes = std::uint64_t{width} * elementBytes;
    const auto hullBegin = stores.front().first;
    const auto hullBytes = static_cast<std::size_t>(stores.back().second - hullBegin);
    std::uint64_t storedBytes = 0;
    for (const auto& [from, to] : stores) storedBytes += to - from;
    auto scratch = std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(rowBytes * height), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    auto timing = Recorder::NoTiming;
    if (recorder != nullptr) {
        recorder->FlushKeyStoresOverlapping(hullBegin, hullBytes);
        recorder->FlushStoresOverlapping(hullBegin, hullBytes);
        commands = recorder->Commands();
        timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageWriteBack);
        recorder->Keep(scratch, scratch->Size());
        if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
        Recorder::CountBarriers(Recorder::CommandClass::StorageWriteBack, 3);
        if (Recorder::BarrierValidate()) {
            const std::pair<VkImage, bool> read{image, false};
            recorder->NoteAccess(Recorder::CommandClass::StorageWriteBack, Recorder::Access{{}, stores, std::span(&read, 1), VK_PIPELINE_STAGE_TRANSFER_BIT});
        }
    } else {
        batch = std::make_unique<CommandBatch>(context);
        commands = batch->Handle();
    }
    VkImageMemoryBarrier toSource{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSource.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toSource.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toSource.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSource.image = image;
    toSource.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
    const VkMemoryBarrier importReady{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &importReady, 0, nullptr, 1, &toSource);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {static_cast<std::int32_t>(area.left), static_cast<std::int32_t>(area.top), 0};
    region.imageExtent = {width, height, 1u};
    context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, scratch->Handle(), 1, &region);
    const auto scratchRead = WholeBufferBarrier(scratch->Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &scratchRead, 0, nullptr);
    const auto scratchOffset = [&](std::uint64_t address) {
        const auto offset = address - rowBase;
        const auto row = offset / mip.pitchBytes;
        return (row - area.top) * rowBytes + offset - row * mip.pitchBytes - area.left * elementBytes;
    };
    const auto copyBuffer = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
    for (const auto& segment : segments) {
        std::vector<VkBufferCopy> copies;
        for (const auto& [from, to] : stores) {
            const auto begin = std::max(from, segment.begin);
            const auto end = std::min(to, segment.end);
            if (begin < end) copies.push_back({scratchOffset(begin), HostImportOffset(*segment.import, begin), end - begin});
        }
        if (!copies.empty()) copyBuffer(commands, scratch->Handle(), segment.import->buffer, static_cast<std::uint32_t>(copies.size()), copies.data());
    }
    if (!saveUnits.empty()) {
        if (original.size() != guestBytes) original.resize(static_cast<std::size_t>(guestBytes));
        const auto saved = savedBytes->Bytes();
        for (const auto unit : saveUnits) {
            const auto offset = static_cast<std::size_t>(layerBegin(unit) - descriptor.baseAddress);
            std::memcpy(saved.data() + offset, original.data() + offset, static_cast<std::size_t>(layerBytes(unit)));
        }
        std::vector<VkBufferCopy> saves;
        for (const auto& [from, to] : stores) saves.push_back({scratchOffset(from), from - descriptor.baseAddress, to - from});
        copyBuffer(commands, scratch->Handle(), savedBytes->Handle(), static_cast<std::uint32_t>(saves.size()), saves.data());
    }
    VkImageMemoryBarrier backToGeneral = toSource;
    backToGeneral.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    backToGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    backToGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    backToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    const VkMemoryBarrier stored{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT};
    context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &stored, 0, nullptr, 1, &backToGeneral);
    if (batch) {
        batch->SubmitAndWait();
        return storedBytes;
    }
    recorder->EndGpuTiming(timing, storedBytes);
    recorder->MarkShaderReadsCovered();
    recorder->NotePendingWrite(hullBegin, hullBytes);
    noteStore(recorder->LastWriteNote(hullBegin, hullBytes));
    return storedBytes;
}

void StorageTexture::Flush(const char* reason) {
    for (const auto& source : std::exchange(adoptedSources, {})) source->Flush();
    storeTrappedOverlaps(reason);
    countStorageEviction(reason);
    {
        auto& live = Live();
        std::lock_guard lock(live.mutex);
        released = true;
    }
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        if (!dirty) return;
        dirty = false;
        pending.textures.remove(this);
        BumpPendingSerial();
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    const auto previous = std::exchange(flushReason, reason);
    writeBack(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    flushReason = previous;
}

void StorageTexture::appendStoredLayers(std::uint64_t address, std::size_t bytes, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) const {
    constexpr std::uint64_t block = 65536;
    constexpr std::size_t runs = 8;
    const auto start = ranges.size();
    const bool all = readTrapped.load(std::memory_order_acquire) || (descriptor.dccAddress != 0 && (uploadedKeys != DccKeys::Uncompressed || descriptor.mipCount > 1)) || (blockUnits && !BlockWriteBackEach());
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layerPending[layer]) continue;
        const auto begin = layerBegin(layer);
        const auto end = begin + layerBytes(layer);
        if (!all && !(address < end && begin < address + bytes)) continue;
        const auto first = begin / block * block;
        const auto last = (end + block - 1) / block * block;
        if (ranges.size() > start && ranges.back().first <= first && first <= ranges.back().second) ranges.back().second = std::max(ranges.back().second, last);
        else ranges.emplace_back(first, last);
    }
    if (ranges.size() - start <= runs) return;
    auto hull = ranges[start];
    for (auto index = start + 1; index < ranges.size(); ++index) {
        hull.first = std::min(hull.first, ranges[index].first);
        hull.second = std::max(hull.second, ranges[index].second);
    }
    ranges.resize(start);
    ranges.push_back(hull);
}

bool StorageTexture::storesWhole() const {
    if (descriptor.dccAddress == 0) return false;
    if (uploadedKeys != DccKeys::Uncompressed) return true;
    if (descriptor.mipCount <= 1) return false;
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    return std::any_of(live.textures.begin(), live.textures.end(), [this](const StorageTexture* texture) { return texture != this && texture->overlapsLive(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)); });
}

bool StorageTexture::pendingUnitInside(std::uint64_t address, std::size_t bytes) const {
    static const bool pretest = std::getenv("APS5_NO_FLUSH_PRETEST") == nullptr;
    if (!pretest) return true;
    const bool whole = storesWhole();
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layerPending[layer]) continue;
        if (whole) return true;
        const auto begin = layerBegin(layer);
        if (address < begin + layerBytes(layer) && begin < address + bytes) return true;
    }
    return false;
}

namespace {

#if defined(__APPLE__)
std::atomic<bool> labelTrapsSet{false};

std::uint64_t TrapPage() {
    static const auto page = static_cast<std::uint64_t>(::getpagesize());
    return page;
}

void ResolveLabelTrap(std::uintptr_t begin, std::uintptr_t end) {
    const auto bytes = static_cast<std::size_t>(end - begin);
    GuestMemory::FlushGpuWrites(begin, bytes);
    while (AgcDriver::DriverDetail::DrawPipeline::HeldReads(begin, bytes)) {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        GuestMemory::FlushGpuWrites(begin, bytes);
    }
}

bool LabelTrapsEnabled() {
    static const bool enabled = [] {
        if (std::getenv("APS5_NO_LABEL_READ_TRAP") != nullptr || !GuestWriteWatch::GuestWriteWatchSetReadTrapResolver_nid_postfix(&ResolveLabelTrap)) return false;
        GuestWriteWatch::GuestWriteWatchSetReadTrapBypass_nid_postfix(+[] { return GuestMemory::GpuMutex().HeldByThisThread(); });
        return true;
    }();
    return enabled;
}
#endif

void waitForIdlePublish(const char* reason) {
    if (reason == nullptr || std::strcmp(reason, "idle") != 0 || Recorder::InCompletion()) return;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    if (auto* recorder = Recorder::Active()) {
        Recorder::CountSync(0);
        recorder->Sync();
    }
}

}

namespace {

bool& GpuOrderedFlush() {
    thread_local bool flag = false;
    return flag;
}

bool& GpuOrderedSkipped() {
    thread_local bool flag = false;
    return flag;
}

}

bool StorageTexture::FlushPending(std::uint64_t address, std::size_t bytes, const StorageTexture* except, const char* reason, PublishScope scope, bool* published, bool waited, VkDevice device) {
    if (!FlushDepthPlanes(address, bytes).empty()) waitForIdlePublish(reason);
    if (published != nullptr) *published = false;
    const auto nothingToStore = [&] {
        auto& pending = Pending();
        if (pending.flushingCount.load(std::memory_order_acquire) != 0) return false;
        if (!pending.textures.SurelyClear(address, bytes)) {
            std::lock_guard lock(pending.mutex);
            const auto* exempt = Refreshing::Get();
            if (pending.textures.MayOverlap(address, bytes) && std::any_of(pending.textures.begin(), pending.textures.end(), [&](const StorageTexture* texture) { return texture != except && texture != exempt && texture->overlaps(address, bytes) && texture->pendingUnitInside(address, bytes); })) return false;
            if (pending.flushingCount.load(std::memory_order_acquire) != 0) return false;
        }
        return scope == PublishScope::None || !AnyShadowedOverlaps(address, bytes);
    };
    if (nothingToStore()) return false;
    if (AgcDriver::DriverDetail::DrawPipeline::HeldReads(address, bytes)) {
        queuedReadHeld.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    std::unique_lock<GuestMemory::GpuMutexType> drawnHold;
    if (auto& drawnPending = Pending(); drawnPending.drawnCount.load(std::memory_order_acquire) != 0 && !GuestMemory::GpuMutex().HeldByThisThread()) {
        bool overlap = false;
        {
            std::lock_guard lock(drawnPending.mutex);
            overlap = std::any_of(drawnPending.drawn.begin(), drawnPending.drawn.end(), [&](const PendingWrites::Drawn& entry) { return entry.texture->dirty && entry.texture->overlaps(address, bytes); });
        }
        if (overlap) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
            drawnHold = std::unique_lock(GuestMemory::GpuMutex());
        }
    }
    std::vector<std::shared_ptr<StorageTexture>> flush;
    std::vector<std::uint64_t> ownNotes;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        const auto* exempt = Refreshing::Get();
        for (auto it = pending.textures.MayOverlap(address, bytes) ? pending.textures.begin() : pending.textures.end(); it != pending.textures.end();) {
            auto* texture = *it;
            if (texture != except && texture != exempt && (device == VK_NULL_HANDLE || texture->context.device == device) && texture->overlaps(address, bytes)) {
                if (!texture->pendingUnitInside(address, bytes)) {
                    pretestSkipped.fetch_add(1, std::memory_order_relaxed);
                    ++it;
                    continue;
                }
                texture->dirty = false;
                if (const auto skips = texture->hookSkips.exchange(0, std::memory_order_relaxed); skips != 0 && texture->skippedResultsInside(address, bytes)) {
                    flushedAfterSkip.fetch_add(1, std::memory_order_relaxed);
                    if (PublishReasonFor(reason) == PublishReason::Hook && texture->hookSkipSite.load(std::memory_order_relaxed) == static_cast<std::uint8_t>(GuestMemory::CurrentReadSite())) flushedAfterSkipSameSite.fetch_add(1, std::memory_order_relaxed);
                }
                if (auto alive = texture->weak_from_this().lock()) flush.push_back(std::move(alive));
                it = pending.textures.erase(it);
            } else {
                ++it;
            }
        }
#if defined(__APPLE__)
        if (!flush.empty() && labelTrapsSet.load(std::memory_order_relaxed)) {
            const auto selected = flush.size();
            for (auto it = pending.textures.begin(); it != pending.textures.end();) {
                auto* texture = *it;
                const auto covered = [&](const std::shared_ptr<StorageTexture>& stored) { return stored->overlaps(texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes)); };
                if (texture == except || texture == exempt || !texture->readTrapped.load(std::memory_order_acquire) || std::none_of(flush.begin(), flush.begin() + static_cast<std::ptrdiff_t>(selected), covered)) {
                    ++it;
                    continue;
                }
                texture->dirty = false;
                if (auto alive = texture->weak_from_this().lock()) flush.push_back(std::move(alive));
                it = pending.textures.erase(it);
            }
        }
#endif
        for (const auto& texture : flush) pending.flushing.push_back(texture.get());
        pending.flushingCount.store(pending.flushing.size(), std::memory_order_release);
        if (!flush.empty()) BumpPendingSerial();
        for (const auto& texture : flush) {
            if (texture->drawnListed) ownNotes.insert(ownNotes.end(), texture->storeNotes.begin(), texture->storeNotes.end());
            ownNotes.insert(ownNotes.end(), texture->flushNotes.begin(), texture->flushNotes.end());
        }
    }
    if (flush.empty()) {
        if (drawnHold.owns_lock()) drawnHold.unlock();
        const bool stored = WaitForFlushing(address, bytes, except);
        const bool units = PublishShadowsOnly(address, bytes, scope, reason, waited || stored);
        if (published != nullptr) *published = units;
        return stored;
    }
    struct Unregister {
        const std::vector<std::shared_ptr<StorageTexture>>& flush;
        ~Unregister() {
            auto& pending = Pending();
            std::lock_guard lock(pending.mutex);
            for (const auto& texture : flush) std::erase(pending.flushing, texture.get());
            pending.flushingCount.store(pending.flushing.size(), std::memory_order_release);
            BumpPendingSerial();
        }
    } unregister{flush};
    static const bool trace = std::getenv("APS5_TRACE_FLUSH") != nullptr;
    if (trace) {
        for (const auto& texture : flush) std::fprintf(stderr, "[flush] image 0x%llx+0x%llx for %s 0x%llx+0x%zx\n", static_cast<unsigned long long>(texture->descriptor.baseAddress), static_cast<unsigned long long>(texture->guestBytes), reason, static_cast<unsigned long long>(address), bytes);
    }
    std::lock_guard gpu(GuestMemory::GpuMutex());
    if (!ownNotes.empty() && (flush.size() != 1 || std::any_of(flush.begin(), flush.end(), [](const std::shared_ptr<StorageTexture>& texture) { return ExistingHostImportFor(texture->context, texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes)) == nullptr; }))) ownNotes.clear();
    auto* const orderedRecorder = Recorder::Active();
    const auto publishesNothing = [&](const Recorder& active) {
        if (scope == PublishScope::None) return true;
        if (!LiveShadowedRanges(address, bytes, scope).empty()) return false;
        constexpr std::uint64_t block = 65536;
        constexpr auto top = std::numeric_limits<std::uint64_t>::max();
        const auto first = address / block * block;
        const auto end = bytes > top - address ? top : address + bytes;
        const auto last = end > top - block ? end : (end + block - 1) / block * block;
        return active.PendingLeaseSerial(first, static_cast<std::size_t>(last - first)) == 0;
    };
    const bool orderedSkip = GpuOrderedFlush() && orderedRecorder != nullptr && !Recorder::InCompletion() && !flush.empty() && std::all_of(flush.begin(), flush.end(), [orderedRecorder](const std::shared_ptr<StorageTexture>& texture) { return texture->context.device == orderedRecorder->Device() && HostImportFor(texture->context, texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes)) != nullptr; }) && publishesNothing(*orderedRecorder);
    if (orderedSkip) {
        GpuOrderedSkipped() = true;
        orderedFlushSkips.fetch_add(1, std::memory_order_relaxed);
    }
    if (flushFailureForTest.exchange(false, std::memory_order_acq_rel)) {
        for (const auto& texture : flush) texture->reconcilePending();
        flushFailureFiredForTest.store(GpuOrderedFlush() ? 2 : 1, std::memory_order_release);
        throw std::runtime_error("StorageTexture::FlushPending: injected failure for a test");
    }
    if (auto* recorder = Recorder::Active(); !orderedSkip && recorder != nullptr && !Recorder::InCompletion()) {
        try {
            static const bool wholeRange = std::getenv("APS5_FLUSH_WAIT_WHOLE_RANGE") != nullptr;
            const bool publishes = !wholeRange && scope != PublishScope::None;
            std::vector<std::pair<std::uint64_t, std::uint64_t>> stored;
            const auto waitRanges = [&] {
                stored.clear();
                if (wholeRange) {
                    stored.emplace_back(address, address + bytes);
                    return;
                }
                for (const auto& texture : flush) texture->appendStoredLayers(address, bytes, stored);
                if (publishes) {
                    for (const auto& range : LiveShadowedRanges(address, bytes, scope)) stored.push_back(range);
                }
            };
            const auto pendingWriter = [&](const Recorder& active) {
                constexpr std::uint64_t block = 65536;
                std::uint64_t writer = 0;
                if (publishes) {
                    constexpr auto top = std::numeric_limits<std::uint64_t>::max();
                    const auto first = address / block * block;
                    const auto end = bytes > top - address ? top : address + bytes;
                    const auto last = end > top - block ? end : (end + block - 1) / block * block;
                    writer = active.PendingLeaseSerial(first, static_cast<std::size_t>(last - first));
                }
                for (const auto& [begin, end] : stored) {
                    if (writer == active.OpenSerial()) break;
                    writer = std::max(writer, active.PendingWriterSerial(begin, static_cast<std::size_t>(end - begin), ownNotes));
                }
                return writer;
            };
            const auto openWrites = [&stored](const Recorder& active) {
                return std::any_of(stored.begin(), stored.end(), [&active](const auto& range) { return active.OpenWriteOverlaps(range.first, static_cast<std::size_t>(range.second - range.first)); });
            };
            for (waitRanges(); pendingWriter(*recorder) != 0; waitRanges()) {
                recorder->Reap();
                waitRanges();
                const auto writer = pendingWriter(*recorder);
                if (writer == recorder->OpenSerial()) {
                    Recorder::CountSync(openWrites(*recorder) ? 1 : 3);
                    recorder->Sync();
                    break;
                }
                if (writer == 0) break;
                recorder->FinishUpTo(writer);
            }
        } catch (...) {
            for (const auto& texture : flush) texture->reconcilePending();
            throw;
        }
    }
    const auto* noting = Recorder::Active();
    const auto firstNote = noting != nullptr ? noting->WriteNoteCount() : 0;
    std::exception_ptr failure;
    const auto previousReason = std::exchange(flushReason, reason);
    for (const auto& texture : flush) {
        try {
            texture->writeBack(address, bytes);
        } catch (...) {
            if (!failure) failure = std::current_exception();
        }
    }
    flushReason = previousReason;
    if (failure) std::rethrow_exception(failure);
    if (scope != PublishScope::None) {
        const auto units = PublishShadow(address, bytes, scope, PublishReasonFor(reason), waited);
        if (units != 0 || !flush.empty()) waitForIdlePublish(reason);
        if (published != nullptr) *published = units != 0;
    } else if (!flush.empty()) {
        waitForIdlePublish(reason);
    }
    if (flush.size() == 1 && noting != nullptr && noting == Recorder::Active()) {
        if (const auto notes = noting->StampedNotesSince(firstNote); !notes.empty()) {
            auto& pending = Pending();
            std::lock_guard lock(pending.mutex);
            auto& own = flush.front()->flushNotes;
            own.insert(own.end(), notes.begin(), notes.end());
            if (own.size() > 32) own.erase(own.begin(), own.end() - 32);
        }
    }
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
