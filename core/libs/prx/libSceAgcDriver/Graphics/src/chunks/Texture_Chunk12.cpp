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
bool StorageTexture::WaitForFlushing(std::uint64_t address, std::size_t bytes, const StorageTexture* except) {
    auto& pending = Pending();
    if (pending.flushingCount.load(std::memory_order_acquire) == 0 || GuestMemory::GpuMutex().HeldByThisThread()) return false;
    const auto* exempt = Refreshing::Get();
    for (bool waited = false;; waited = true) {
        {
            std::lock_guard lock(pending.mutex);
            if (std::none_of(pending.flushing.begin(), pending.flushing.end(), [&](const StorageTexture* texture) { return texture != except && texture != exempt && texture->overlaps(address, bytes); })) return waited;
        }
        { std::lock_guard gpu(GuestMemory::GpuMutex()); }
        std::this_thread::yield();
    }
}

bool StorageTexture::hashedAtUnwatch(std::uint64_t address, std::size_t bytes) const {
    return (guestBytes > GenerationBaselineBytes || anyBaselineDue.load(std::memory_order_relaxed)) && overlapsLive(address, bytes);
}

void StorageTexture::SetPublishHookForTest(void (*hook)()) {
    publishHookForTest.store(hook, std::memory_order_release);
}

void StorageTexture::SetPendingHookForTest(void (*hook)(std::uint64_t address, std::size_t bytes)) {
    pendingHookForTest.store(hook, std::memory_order_release);
}

bool StorageTexture::UnwatchHookLocksGpu(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    if (MultisampleDccUnwatchLocksGpu(ranges)) return true;
    if (AnyPendingOverlaps(ranges)) return true;
    auto& live = Live();
    std::lock_guard lock(live.mutex);
    for (const auto& range : ranges) {
        if (range.second <= range.first) continue;
        const auto bytes = static_cast<std::size_t>(range.second - range.first);
        for (const auto* texture : live.textures) {
            if (texture->hashedAtUnwatch(range.first, bytes)) return true;
        }
    }
    return false;
}

void StorageTexture::InstallUnwatchHook() {
    static const bool installed = [] {
        GuestMemory::SetUnwatchHook(&StorageTexture::SaveBeforeUnwatch);
        return true;
    }();
    static_cast<void>(installed);
}

void StorageTexture::SaveBeforeUnwatch(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0) return;
    SaveMultisampleDccBeforeUnwatch(address, bytes);
    std::vector<std::shared_ptr<StorageTexture>> overlapping;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        const auto take = [&](StorageTexture* texture) {
            if (!texture->overlaps(address, bytes)) return;
            if (auto alive = texture->weak_from_this().lock()) overlapping.push_back(std::move(alive));
        };
        for (auto* texture : pending.textures) take(texture);
        for (auto* texture : pending.flushing) take(texture);
    }
    std::vector<std::shared_ptr<StorageTexture>> live;
    const bool proven = GuestMemory::UnwatchWritesProven();
    {
        auto& images = Live();
        std::lock_guard lock(images.mutex);
        for (auto* texture : images.textures) {
            if (!texture->hashedAtUnwatch(address, bytes)) continue;
            if (auto alive = texture->weak_from_this().lock()) live.push_back(std::move(alive));
        }
    }
    if (overlapping.empty() && live.empty()) return;
    RequireGpuLockBeforeImports("the unwatch hook");
    std::lock_guard gpu(GuestMemory::GpuMutex());
    for (const auto& texture : live) {
        texture->hashBeforeUnwatch(address, bytes, proven);
        texture->rebaseBeforeUnwatch(address, bytes, proven);
    }
    for (const auto& texture : overlapping) texture->saveBeforeUnwatch(address, bytes);
}

void StorageTexture::forgetKeptAtUnwatch(std::uint32_t unit) {
    const auto begin = layerBegin(unit);
    keptAtUnwatch.erase(keptAtUnwatch.lower_bound(begin), keptAtUnwatch.lower_bound(begin + layerBytes(unit)));
}

bool StorageTexture::keptAtUnwatchHolds(std::uint64_t from, std::uint32_t unit) const {
    const auto found = keptAtUnwatch.find(from);
    return found != keptAtUnwatch.end() && layerOriginal[unit] && found->second == layerGeneration[unit];
}

bool StorageTexture::keptForCpu(std::uint32_t unit) const {
    const auto begin = layerBegin(unit);
    const auto end = begin + layerBytes(unit);
    for (auto it = keptAtUnwatch.lower_bound(begin); it != keptAtUnwatch.end() && it->first < end; ++it) {
        if (it->second == layerGeneration[unit]) return true;
    }
    return false;
}

void StorageTexture::forgetSavedOver(std::uint64_t address, std::size_t bytes) {
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        if (layerBegin(unit) < address + bytes && address < layerBegin(unit) + layerBytes(unit)) {
            layerOriginal[unit] = false;
            cancelStoredSave(unit);
        }
    }
    releaseUnheldOriginal();
}

void StorageTexture::saveBeforeUnwatch(std::uint64_t address, std::size_t bytes) {
    std::vector<bool> units(trackedLayers, false);
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) units[unit] = layerPending[unit] && !layerOriginal[unit] && layerBegin(unit) < address + bytes && address < layerBegin(unit) + layerBytes(unit);
    const auto runs = unitRuns(units);
    if (runs.empty()) return;
    original.resize(static_cast<std::size_t>(guestBytes));
    comparedRanges = {};
    for (const auto& [begin, end] : runs) GuestMemory::ReadCommittedUnsynced(descriptor.baseAddress + begin, std::span(original).subspan(static_cast<std::size_t>(begin), static_cast<std::size_t>(end - begin)));
    for (const auto& [begin, end] : runs) GuestMemory::CollectWritesUncached(descriptor.baseAddress + begin, static_cast<std::size_t>(end - begin));
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        if (!units[unit]) continue;
        if (layerGeneration[unit] == 0) {
            layerOriginal[unit] = true;
            cancelStoredSave(unit);
            continue;
        }
        if (!keepWrittenSince(unit)) continue;
        layerOriginal[unit] = true;
        cancelStoredSave(unit);
    }
}

std::vector<std::uint32_t> StorageTexture::prepareStoredSave(const std::vector<bool>& layers, const std::vector<bool>& skippedLayer) {
    std::vector<std::uint32_t> units;
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        if (layers[unit] && !skippedLayer[unit]) units.push_back(unit);
    }
    if (!units.empty() && !savedBytes) savedBytes = std::make_shared<Buffer>(context, static_cast<std::size_t>(guestBytes), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    return units;
}

void StorageTexture::commitStoredSave(const std::vector<std::uint32_t>& units) {
    if (units.empty()) return;
    const auto ticket = ++lastSaveTicket;
    for (const auto unit : units) savedTicket[unit] = ticket;
    auto* recorder = recorderFor(context);
    if (recorder == nullptr) {
        applyStoredSave(ticket, units);
        return;
    }
    recorder->OnComplete([texture = weak_from_this(), bytes = savedBytes, ticket, units] {
        if (auto alive = texture.lock()) alive->applyStoredSave(ticket, units);
    });
}

void StorageTexture::applyStoredSave(std::uint64_t ticket, const std::vector<std::uint32_t>& units) {
    if (!savedBytes) return;
    const auto stored = savedBytes->Bytes();
    for (const auto unit : units) {
        if (savedTicket[unit] != ticket) continue;
        if (original.size() != guestBytes) original.resize(static_cast<std::size_t>(guestBytes));
        comparedRanges = {};
        const auto offset = static_cast<std::size_t>(layerBegin(unit) - descriptor.baseAddress);
        std::memcpy(original.data() + offset, stored.data() + offset, static_cast<std::size_t>(layerBytes(unit)));
        layerOriginal[unit] = true;
        savedTicket[unit] = 0;
    }
}

void StorageTexture::cancelStoredSave(std::uint32_t unit) {
    if (unit < savedTicket.size()) savedTicket[unit] = 0;
}

void StorageTexture::cancelStoredSaves() {
    savedTicket.assign(trackedLayers, 0);
}

void StorageTexture::releaseUnheldOriginal() {
    if (original.capacity() == 0 || std::any_of(layerOriginal.begin(), layerOriginal.end(), [](bool held) { return held; })) return;
    std::vector<std::byte>().swap(original);
    comparedRanges = {};
}

void StorageTexture::awaitStoredSave(const std::vector<bool>* layers) {
    std::uint64_t first = descriptor.baseAddress + guestBytes;
    std::uint64_t last = descriptor.baseAddress;
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        if (savedTicket[unit] == 0 || (layers != nullptr && !(*layers)[unit])) continue;
        first = std::min(first, layerBegin(unit));
        last = std::max(last, layerBegin(unit) + layerBytes(unit));
    }
    if (last <= first) return;
    auto* recorder = recorderFor(context);
    if (recorder == nullptr) recorder = context.recorder;
    Require(recorder != nullptr, "a write-back's saved guest bytes are pending without a recorder");
    recorder->SyncThrough(first, static_cast<std::size_t>(last - first));
    const auto outstanding = [&] {
        for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
            if (savedTicket[unit] != 0 && (layers == nullptr || (*layers)[unit])) return true;
        }
        return false;
    };
    if (outstanding()) recorder->Sync();
}

bool StorageTexture::PublishShadowsOnly(std::uint64_t address, std::size_t bytes, PublishScope scope, const char* reason, bool waited) {
    if (scope == PublishScope::None || !AnyShadowedOverlaps(address, bytes)) return false;
    std::lock_guard gpu(GuestMemory::GpuMutex());
    if (PublishShadow(address, bytes, scope, PublishReasonFor(reason), waited) == 0) return false;
    waitForIdlePublish(reason);
    return true;
}

namespace {

struct StorageDumpSelector {
    std::uint64_t address = 0;
    std::uint32_t width = 0, height = 0, layers = 0, format = 0;
    bool anyFormat = true;
};

const StorageDumpSelector& StorageDumpSelection() {
    static const StorageDumpSelector selector = [] {
        StorageDumpSelector parsed;
        const char* text = std::getenv("APS5_DUMP_STORAGE");
        if (text == nullptr) return parsed;
        if (const int fields = std::sscanf(text, "%ux%ux%u:%u", &parsed.width, &parsed.height, &parsed.layers, &parsed.format); fields >= 3) {
            parsed.anyFormat = fields < 4;
            return parsed;
        }
        parsed.address = std::strtoull(text, nullptr, 16);
        return parsed;
    }();
    return selector;
}

bool StorageDumpSelects(const GuestTextureResource& descriptor) {
    const auto& selector = StorageDumpSelection();
    if (selector.address != 0) return descriptor.baseAddress == selector.address;
    return selector.width != 0 && descriptor.width == selector.width && descriptor.height == selector.height && descriptor.depthOrLastArray + 1 == selector.layers && (selector.anyFormat || descriptor.format == selector.format);
}

bool StorageDumpReady() {
    const char* gate = std::getenv("APS5_DUMP_STORAGE_GATE");
    std::error_code error;
    return gate == nullptr || std::filesystem::exists(gate, error);
}

}

void StorageTexture::FlushDumpSelected(const char* reason) {
    const auto& selector = StorageDumpSelection();
    if (selector.address == 0 && selector.width == 0) return;
    if (!StorageDumpReady()) return;
    if (std::getenv("APS5_DUMP_STORAGE_GATE") != nullptr) {
        std::lock_guard gpu(GuestMemory::GpuMutex());
        static std::vector<std::uint64_t> captured;
        std::vector<std::shared_ptr<StorageTexture>> selected;
        {
            auto& live = Live();
            std::lock_guard lock(live.mutex);
            for (auto* texture : live.textures) {
                if (selected.size() + captured.size() >= 8) break;
                if (!StorageDumpSelects(texture->descriptor) || texture->sliceLinearBytes * texture->arrayLayers > (8u << 20u)) continue;
                if (std::find(captured.begin(), captured.end(), texture->descriptor.baseAddress) != captured.end()) continue;
                if (auto alive = texture->weak_from_this().lock()) selected.push_back(std::move(alive));
            }
        }
        if (!selected.empty()) {
            if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->Recording()) recorder->Submit();
            for (const auto& texture : selected) {
                const auto& context = texture->context;
                Check(context.Function<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(context.device), "private image readback wait");
                Buffer host(context, static_cast<std::size_t>(texture->sliceLinearBytes * texture->arrayLayers), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
                CommandBatch batch(context);
                const auto commands = batch.Handle();
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = texture->image;
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, texture->descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
                context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                const auto regions = texture->CopyRegions();
                context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, host.Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                const auto hostBarrier = WholeBufferBarrier(host.Handle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
                context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &hostBarrier, 1, &barrier);
                batch.SubmitAndWait();
                char name[80];
                std::snprintf(name, sizeof(name), "storage_direct_%llx_%zu.raw", static_cast<unsigned long long>(texture->descriptor.baseAddress), captured.size());
                std::FILE* file = std::fopen(name, "wb");
                Require(file != nullptr, "cannot open private image readback file");
                const auto& mip = texture->mips[0];
                const std::uint32_t header[3] = {mip.pitchBytes / static_cast<std::uint32_t>(BytesPerElement(texture->descriptor.format)), mip.height, static_cast<std::uint32_t>(texture->storageFormat)};
                bool written = std::fwrite(header, sizeof(header), 1, file) == 1;
                for (std::uint32_t layer = 0; layer < texture->arrayLayers; ++layer) {
                    if (!texture->geometry.HasLayer(0, layer)) continue;
                    written = std::fwrite(host.Bytes().data() + texture->geometry.LinearLayerOffset(layer) + mip.linearOffset, 1, static_cast<std::size_t>(mip.linearSize), file) == mip.linearSize && written;
                }
                const bool closed = std::fclose(file) == 0;
                Require(written && closed, "cannot complete private image readback file");
                captured.push_back(texture->descriptor.baseAddress);
                std::fprintf(stderr, "[storage-dump] direct=%s bytes=%llu\n", name, static_cast<unsigned long long>(mip.linearSize * texture->arrayLayers));
            }
        }
        return;
    }
    std::vector<std::pair<std::uint64_t, std::size_t>> ranges;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        for (const auto* texture : pending.textures) {
            if (StorageDumpSelects(texture->descriptor)) ranges.emplace_back(texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes));
        }
    }
    for (const auto& [address, bytes] : ranges) static_cast<void>(FlushPending(address, bytes, nullptr, reason));
}

void StorageTexture::FlushAllPending(const char* reason, VkDevice device) {
    std::optional<PublishOnCpuScope> onCpu;
    if (reason != nullptr && std::strcmp(reason, "idle") == 0) onCpu.emplace();
    static_cast<void>(FlushPending(0, std::numeric_limits<std::size_t>::max(), nullptr, reason, PublishScope::Whole, nullptr, false, device));
}

bool StorageTexture::FlushAllPendingUntil(const char* reason, const std::function<bool()>& stop, VkDevice device) {
    std::optional<PublishOnCpuScope> onCpu;
    if (reason != nullptr && std::strcmp(reason, "idle") == 0) onCpu.emplace();
    std::vector<std::pair<std::uint64_t, std::size_t>> ranges;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        for (const auto* texture : pending.textures) {
            if (device == VK_NULL_HANDLE || texture->context.device == device) ranges.emplace_back(texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes));
        }
    }
    constexpr std::size_t piece = std::size_t{4} << 20u;
    for (const auto& [address, bytes] : ranges) {
        for (std::size_t offset = 0; offset < bytes; offset += piece) {
            if (stop()) return false;
            static_cast<void>(FlushPending(address + offset, std::min(piece, bytes - offset), nullptr, reason, PublishScope::Whole, nullptr, false, device));
        }
    }
    return true;
}

std::size_t StorageTexture::StoreSmallPending(const char* reason) {
    std::vector<std::shared_ptr<StorageTexture>> small;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        for (auto* texture : pending.textures) {
            if (texture->guestBytes > SmallStoreBytes || texture->readTrapped.load(std::memory_order_acquire)) continue;
            if (auto alive = texture->weak_from_this().lock()) small.push_back(std::move(alive));
        }
    }
    std::size_t stored = 0;
    for (const auto& texture : small) {
        const auto address = texture->descriptor.baseAddress;
        const auto bytes = static_cast<std::size_t>(texture->guestBytes);
        if (!HostImportCovers(texture->context, address, bytes)) continue;
        if (FlushPending(address, bytes, nullptr, reason)) ++stored;
    }
    return stored;
}

void StorageTexture::StoreReadbackBeforeLabel(const char* reason, VkDevice device) {
    static const bool disabled = std::getenv("APS5_NO_LABEL_READBACK_STORE") != nullptr;
    if (disabled) return;
    static const bool gpuOrdered = std::getenv("APS5_NO_FLUSH_GPU_ORDERED") == nullptr;
    struct OrderedScope {
        bool previous;
        explicit OrderedScope(bool on) : previous(GpuOrderedFlush()) { GpuOrderedFlush() = previous || on; }
        ~OrderedScope() { GpuOrderedFlush() = previous; }
    } ordered{gpuOrdered};
    std::vector<std::pair<std::uint64_t, std::size_t>> ranges;
    std::vector<std::pair<std::uint64_t, std::size_t>> occasional;
    {
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
#if defined(__APPLE__)
        const bool trap = LabelTrapsEnabled();
        if (trap) labelTrapsSet.store(true, std::memory_order_relaxed);
        const auto page = TrapPage();
        const auto sharesPage = [&](const StorageTexture* texture) {
            const auto first = texture->descriptor.baseAddress & ~(page - 1);
            const auto bytes = static_cast<std::size_t>(((texture->descriptor.baseAddress + texture->guestBytes + page - 1) & ~(page - 1)) - first);
            const auto shares = [&](const StorageTexture* other) { return other != texture && other->overlaps(first, bytes); };
            const auto sharesStored = [&](const StorageTexture* other) { return shares(other) && (other->descriptor.tileMode == TextureTileMode::kLinear || other->guestBytes <= SmallStoreBytes); };
            return std::any_of(pending.textures.begin(), pending.textures.end(), sharesStored) || std::any_of(pending.flushing.begin(), pending.flushing.end(), shares);
        };
#endif
        for (const auto* texture : pending.textures) {
            if (texture->descriptor.tileMode != TextureTileMode::kLinear && texture->guestBytes > SmallStoreBytes) continue;
#if defined(__APPLE__)
            if (trap && !sharesPage(texture) && (texture->readTrapped.load(std::memory_order_acquire) || !DirectMemoryAliased_nid_postfix(static_cast<std::uintptr_t>(texture->descriptor.baseAddress), static_cast<std::size_t>(texture->guestBytes))) && GuestWriteWatch::GuestWriteWatchTrapReads_nid_postfix(static_cast<std::uintptr_t>(texture->descriptor.baseAddress), static_cast<std::size_t>(texture->guestBytes))) {
                texture->readTrapped.store(true, std::memory_order_release);
                continue;
            }
#endif
            ranges.emplace_back(texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes));
        }
        for (auto* texture : pending.textures) {
            if (texture->descriptor.tileMode == TextureTileMode::kLinear || texture->guestBytes <= SmallStoreBytes || texture->guestBytes > OccasionalStoreBytes) continue;
            if (device != VK_NULL_HANDLE && texture->context.device != device) continue;
            if (!texture->writtenOccasionally.exchange(false, std::memory_order_relaxed)) continue;
            occasionalLabelStores.fetch_add(1, std::memory_order_relaxed);
            occasional.emplace_back(texture->descriptor.baseAddress, static_cast<std::size_t>(texture->guestBytes));
        }
    }
    for (const auto& [address, bytes] : ranges) static_cast<void>(FlushPending(address, bytes, nullptr, reason));
    for (const auto& [address, bytes] : occasional) static_cast<void>(FlushPending(address, bytes, nullptr, reason, PublishScope::Whole, nullptr, false, device));
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
