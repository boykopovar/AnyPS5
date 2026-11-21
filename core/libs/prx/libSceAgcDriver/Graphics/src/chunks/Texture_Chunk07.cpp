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
bool StorageTexture::compareUntracked(std::uint64_t address, std::size_t bytes, std::span<std::uint8_t> changed, bool memoize) const {
    if (!originalHolds(address, bytes)) return false;
    struct Exempt {
        const StorageTexture* previous;
        ~Exempt() { Refreshing::Set(previous); }
    } exempt{Refreshing::Get()};
    Refreshing::Set(this);
    Require(address >= descriptor.baseAddress && bytes <= guestBytes && address - descriptor.baseAddress <= guestBytes - bytes, "texture comparison exceeds its snapshot");
    Require(original.size() == guestBytes, "texture comparison has no complete snapshot");
    GuestMemory::FlushGpuWrites(address, bytes);
    if (!originalHolds(address, bytes)) return false;
    constexpr std::uint64_t blockBytes = 65536;
    const auto end = address + bytes;
    const auto first = address / blockBytes;
    Require(changed.size() == (end - 1) / blockBytes - first + 1, "texture comparison block count differs");
    const auto* recorder = Recorder::Active();
    const auto epoch = GuestMemory::CollectEpoch();
    const auto hint = memoize && epoch != 0 ? GuestMemory::UntrackedHint(address, bytes) : 0;
    const bool forgetting = (GuestMemory::ForgetSerial() & 1u) != 0;
    const std::array<std::uint64_t, 5> stamp{hint != 0, hint != 0 ? hint : epoch, GuestMemory::NewestStamp(address, bytes), GuestMemory::ForgetStamp(address, bytes), recorder != nullptr ? recorder->NewestWriteNote(address, bytes) : 0};
    const bool cacheable = memoize && epoch != 0 && !forgetting;
    if (cacheable && std::any_of(comparedRanges.begin(), comparedRanges.end(), [&](const ComparedRange& entry) { return entry.address == address && entry.bytes == bytes && entry.stamp == stamp; })) {
        std::fill(changed.begin(), changed.end(), GuestMemory::BlockUnchanged);
        return true;
    }
    untrackedCompares.fetch_add(1, std::memory_order_relaxed);
    const auto saved = std::span(original).subspan(static_cast<std::size_t>(address - descriptor.baseAddress), bytes);
    if (GuestMemory::EqualsCommittedUnsynced(address, saved)) {
        std::fill(changed.begin(), changed.end(), GuestMemory::BlockUnchanged);
        if (cacheable) {
            auto found = std::find_if(comparedRanges.begin(), comparedRanges.end(), [&](const ComparedRange& entry) { return entry.address == address && entry.bytes == bytes; });
            if (found == comparedRanges.end()) found = comparedRanges.begin() + nextComparedRange++ % comparedRanges.size();
        }
        return true;
    }
    comparedRanges = {};
    for (std::size_t index = 0; index < changed.size(); ++index) {
        const auto begin = std::max(address, (first + index) * blockBytes);
        const auto stop = std::min(end, (first + index + 1) * blockBytes);
        const auto saved = std::span(original).subspan(static_cast<std::size_t>(begin - descriptor.baseAddress), static_cast<std::size_t>(stop - begin));
        changed[index] = GuestMemory::EqualsCommittedUnsynced(begin, saved) ? GuestMemory::BlockUnchanged : GuestMemory::BlockWritten;
    }
    return true;
}

bool SingleKeyColorFor(const GuestTextureResource& resource, std::uint64_t guestBytes, std::uint64_t collected, DccSingleProof& proof, VkFormat format, DccKeys keys, VkClearColorValue& clear) {
    if (keys != DccKeys::ClearSingle || !ProvedSingleUniform(resource, guestBytes, collected, proof)) return false;
    const auto elementBytes = BytesPerElement(resource.format);
    if (elementBytes == 0 || elementBytes > sizeof(std::uint32_t) * 4 || sizeof(std::uint32_t) * 4 % elementBytes != 0) return false;
    std::uint32_t pattern[4]{};
    auto* bytes = reinterpret_cast<std::byte*>(pattern);
    if (!ReadSingleElement(resource, std::span(bytes, elementBytes))) return false;
    for (std::size_t at = elementBytes; at < sizeof(pattern); at += elementBytes) std::memcpy(bytes + at, bytes, elementBytes);
    return FillClearColor(format, elementBytes, pattern, clear);
}

std::vector<bool> StorageTexture::cpuUploadLayers(const std::vector<bool>& units) const {
    if (descriptor.dimension != TextureDimension::k2DArray || descriptor.mipCount != 1 || descriptor.dccAddress != 0 || IsBlockCompressed(descriptor.format) || geometry.thick || arrayLayers <= 1 || geometry.imageLayers != arrayLayers || trackedLayerBytes == 0 || units.size() != trackedLayers) return {};
    std::vector<bool> selected(arrayLayers, false);
    for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
        const auto begin = geometry.GuestLayerOffset(layer);
        const auto end = begin + geometry.layerBytes;
        if (end <= begin || end > guestBytes || begin % trackedLayerBytes != 0 || (end % trackedLayerBytes != 0 && end != guestBytes)) return {};
        const auto first = static_cast<std::uint32_t>(begin / trackedLayerBytes);
        const auto last = static_cast<std::uint32_t>((end + trackedLayerBytes - 1) / trackedLayerBytes);
        if (first >= last || last > trackedLayers) return {};
        selected[layer] = units[first];
        for (auto unit = first + 1; unit < last; ++unit) {
            if (units[unit] != selected[layer]) return {};
        }
    }
    if (std::none_of(selected.begin(), selected.end(), [](bool value) { return value; }) || std::all_of(selected.begin(), selected.end(), [](bool value) { return value; })) return {};
    return selected;
}

void StorageTexture::upload(const std::vector<bool>* layers) {
    const bool profile = LookupOutcomes::Profiled();
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    CompletePendingCpuWrites(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    if (descriptor.dccAddress != 0) CompletePendingCpuWrites(context, descriptor.dccAddress, DccKeyBytes(guestBytes));
    CaptureTrace::Log("upload image=%llx bytes=%llu generation=%llu reason=%s partial=%d", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), static_cast<unsigned long long>(generation), uploadReason, layers != nullptr);
    const auto elementBytes = BytesPerElement(descriptor.format);
    const auto linearBytes = sliceLinearBytes * arrayLayers;
    if (layers != nullptr && ((!blockUnits && trackedLayers != arrayLayers) || std::all_of(layers->begin(), layers->end(), [](bool selected) { return selected; }))) layers = nullptr;
    GuestMemory::MaterializeKnownZero(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    auto now = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    const auto stampLayers = [&](bool selectedOnly) {
        const bool whole = !selectedOnly || layers == nullptr;
        if (guestBytes > GenerationBaselineBytes || !guestBytesSettled()) generationBaseline.clear();
        else if (whole) {
            generationBaseline.resize(static_cast<std::size_t>(guestBytes));
            GuestMemory::ReadCommitted(descriptor.baseAddress, generationBaseline);
        }
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (!whole && !(*layers)[layer]) continue;
            baselineDue[layer] = false;
            rebaseTicket[layer] = 0;
            forgetKeptAtUnwatch(layer);
            layerGeneration[layer] = collectedLayerGeneration(layer, now);
            if (whole || generationBaseline.size() != guestBytes) continue;
            const auto offset = static_cast<std::size_t>(layerBegin(layer) - descriptor.baseAddress);
            GuestMemory::ReadCommitted(layerBegin(layer), std::span<std::byte>(generationBaseline).subspan(offset, static_cast<std::size_t>(layerBytes(layer))));
        }
        refreshGeneration();
    };
    uploadedKeys = ProvedClearKeys(descriptor, guestBytes, keyProof);
    filledKeys = DccKeys::Uncompressed;
    VkClearColorValue clearValue{};
    const bool fastClear = IsDccClear(uploadedKeys) || uploadedKeys == DccKeys::ClearSingle;
    if (fastClear && (ClearColorFor(storageFormat, uploadedKeys, clearValue) || SingleKeyColorFor(descriptor, guestBytes, now, singleProof, storageFormat, uploadedKeys, clearValue))) {
        static_cast<void>(HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)));
        if (GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) {
            layerOriginal.assign(trackedLayers, false);
            cancelStoredSaves();
            releaseUnheldOriginal();
        } else if (!guestBytesStillHeld()) {
            captureGuestBytes(nullptr);
        }
        forgetBorrowed(0, trackedLayers);
        stampLayers(false);
        static const bool recordClear = std::getenv("APS5_NO_RECORDED_CLEAR") == nullptr;
        auto* recorder = recordClear ? Recorder::Active() : nullptr;
        std::unique_ptr<CommandBatch> batch;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        auto timing = Recorder::NoTiming;
        if (recorder != nullptr) {
            commands = recorder->Commands();
            timing = recorder->BeginGpuTiming(Recorder::CommandClass::DccClear);
            if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
            ++Profile().storageRecordedClears;
            Recorder::CountBarriers(Recorder::CommandClass::DccClear, 2);
            if (Recorder::BarrierValidate()) {
                const std::pair<VkImage, bool> cleared{image, true};
                recorder->NoteAccess(Recorder::CommandClass::DccClear, Recorder::Access{{}, {}, std::span(&cleared, 1), VK_PIPELINE_STAGE_TRANSFER_BIT});
            }
        } else {
            batch = std::make_unique<CommandBatch>(context);
            commands = batch->Handle();
            ++Profile().storageWaitedClears;
        }
        VkImageMemoryBarrier toClear{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toClear.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
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
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        if (batch) batch->SubmitAndWait();
        else recorder->EndGpuTiming(timing, guestBytes);
        countStorageUpload(0, guestBytes);
        ++version;
        if (profile) LookupOutcomes::Add(LookupOutcomes::UploadClear, start);
        return;
    }
    std::vector<ImportSegment> segments;
    if (uploadedKeys == DccKeys::Uncompressed) {
        if (const auto* whole = HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) segments.push_back({whole, descriptor.baseAddress, descriptor.baseAddress + guestBytes});
        else if (blockUnits) segments = importSegments();
    }
    if (!segments.empty()) {
        const auto* import = segments.front().import;
        now = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
        if (GuestMemory::Watched(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) {
            layerOriginal.assign(trackedLayers, false);
            cancelStoredSaves();
            releaseUnheldOriginal();
        } else {
            captureGuestBytes(layers);
        }
        stampLayers(true);
        if (blockUnits && (segments.size() > 1 || layers != nullptr || AnyShadowedOverlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)))) {
            std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
            if (layers != nullptr) {
                for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
                    if ((*layers)[unit]) forgetBorrowed(unit, 1);
                }
                runs = unitRuns(*layers);
            } else {
                forgetBorrowed(0, trackedLayers);
                runs.emplace_back(0, guestBytes);
            }
            const auto uploadedBytes = uploadWindows(segments, runs, layers == nullptr && version == 0);
            countStorageUpload(1, uploadedBytes);
            if (layers != nullptr) {
                partialUploads.fetch_add(1, std::memory_order_relaxed);
                partialUploadBytes.fetch_add(uploadedBytes, std::memory_order_relaxed);
            }
            ++Profile().storageDirectUploads;
            ++version;
            if (profile) LookupOutcomes::Add(LookupOutcomes::UploadDirect, start);
            return;
        }
        PublishShadow(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), PublishScope::Whole, PublishReason::Upload);
        forgetBorrowed(0, trackedLayers);
        std::uint64_t uploadedBytes = layers == nullptr ? guestBytes : 0;
        if (layers != nullptr) {
            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                if ((*layers)[layer]) uploadedBytes += trackedLayerBytes;
            }
        }
        const bool direct = singlePass();
        auto linear = direct ? nullptr : std::make_shared<DeviceBuffer>(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        detiler.BeginBatch();
        auto* recorder = Recorder::Active();
        std::unique_ptr<CommandBatch> batch;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        auto timing = Recorder::NoTiming;
        if (recorder != nullptr) {
            recorder->FlushStoresOverlapping(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
            commands = recorder->Commands();
            timing = recorder->BeginGpuTiming(Recorder::CommandClass::StorageUpload);
            if (linear) recorder->Keep(linear, linear->Size());
            if (auto self = weak_from_this().lock()) keepStorageTexture(*recorder, std::move(self));
            recorder->NotePendingRead(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), Recorder::ReadKind::StorageUpload);
            Recorder::CountBarriers(Recorder::CommandClass::StorageUpload, direct ? 2 : 3);
            if (Recorder::BarrierValidate()) {
                const std::pair<std::uint64_t, std::uint64_t> read{descriptor.baseAddress, descriptor.baseAddress + guestBytes};
                const std::pair<VkImage, bool> written{image, true};
                recorder->NoteAccess(Recorder::CommandClass::StorageUpload, Recorder::Access{std::span(&read, 1), {}, std::span(&written, 1), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT});
            }
        } else {
            batch = std::make_unique<CommandBatch>(context);
            commands = batch->Handle();
        }
        const auto importOffset = HostImportOffset(*import, descriptor.baseAddress);
        if (direct) {
            recordDirectUploadBarrier(commands, layers == nullptr);
            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                if (layers != nullptr && !(*layers)[layer]) continue;
                for (std::uint32_t level = 0; level < mips.size(); ++level) {
                    if (!geometry.HasLayer(level, layer)) continue;
                    const auto& mip = mips[level];
                    detiler.DispatchImage(commands, descriptor.tileMode, elementBytes, import->buffer, importOffset + geometry.GuestOffset(layer, mip), elementLayerView(level, layer), mip, false, layer, {.pipeBankXor = descriptor.pipeBankXor});
                }
            }
            recordDirectUploadDone(commands);
            singlePassMoves.fetch_add(1, std::memory_order_relaxed);
            if (batch) batch->SubmitAndWait();
            else recorder->EndGpuTiming(timing, uploadedBytes);
            countStorageUpload(1, uploadedBytes);
            ++Profile().storageDirectUploads;
            ++version;
            if (profile) LookupOutcomes::Add(LookupOutcomes::UploadDirect, start);
            return;
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            if (layers != nullptr && !(*layers)[layer]) continue;
            for (std::uint32_t level = 0; level < mips.size(); ++level) {
                if (!geometry.HasLayer(level, layer)) continue;
                const auto& mip = mips[level];
                detiler.Dispatch(commands, descriptor.tileMode, elementBytes, import->buffer, importOffset + geometry.GuestOffset(layer, mip), linear->Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, mip, false, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
            }
        }
        const auto linearRead = WholeBufferBarrier(linear->Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.srcAccessMask = layers != nullptr ? VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0u;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = layers != nullptr ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearRead, 1, &toTransfer);
        const auto regions = CopyRegions(layers);
        context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
        VkImageMemoryBarrier toGeneral = toTransfer;
        toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
        if (batch) batch->SubmitAndWait();
        else recorder->EndGpuTiming(timing, uploadedBytes);
        countStorageUpload(1, uploadedBytes);
        ++Profile().storageDirectUploads;
        ++version;
        if (profile) LookupOutcomes::Add(LookupOutcomes::UploadDirect, start);
        return;
    }
    const auto cpuLayers = uploadedKeys == DccKeys::Uncompressed && layers != nullptr ? cpuUploadLayers(*layers) : std::vector<bool>{};
    const bool selectedCpu = !cpuLayers.empty();
    std::uint64_t uploadedBytes = guestBytes;
    if (selectedCpu) {
        captureGuestBytes(layers);
        for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
            if ((*layers)[unit]) forgetBorrowed(unit, 1);
        }
        stampLayers(true);
        uploadedBytes = 0;
        for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
            if (cpuLayers[layer]) uploadedBytes += geometry.layerBytes;
        }
    } else {
        layerOriginal.assign(trackedLayers, true);
        cancelStoredSaves();
        forgetBorrowed(0, trackedLayers);
        stampLayers(false);
        original.resize(static_cast<std::size_t>(guestBytes));
        GuestMemory::ReadCommitted(descriptor.baseAddress, original);
    }
    {
            Buffer staging(context, original.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            if (selectedCpu) {
                for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                    if (!cpuLayers[layer]) continue;
                    const auto offset = geometry.GuestLayerOffset(layer);
                    std::memcpy(staging.Bytes().data() + offset, original.data() + offset, static_cast<std::size_t>(geometry.layerBytes));
                }
            } else if (uploadedKeys == DccKeys::Uncompressed) std::memcpy(staging.Bytes().data(), original.data(), original.size());
            else ReadTextureSurface(descriptor, uploadedKeys, staging.Bytes().first(original.size()));
            DeviceBuffer tiled(context, original.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            DeviceBuffer linear(context, static_cast<std::size_t>(linearBytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            detiler.BeginBatch();
            CommandBatch batch(context);
            const auto commands = batch.Handle();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            if (selectedCpu) {
                for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                    if (!cpuLayers[layer]) continue;
                    const auto offset = geometry.GuestLayerOffset(layer);
                    CopyBuffer(context, commands, staging.Handle(), offset, tiled.Handle(), offset, geometry.layerBytes);
                }
            } else CopyBuffer(context, commands, staging.Handle(), 0, tiled.Handle(), 0, original.size());
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            for (std::uint32_t layer = 0; layer < arrayLayers; ++layer) {
                if (selectedCpu && !cpuLayers[layer]) continue;
                for (std::uint32_t level = 0; level < mips.size(); ++level) {
                    if (!geometry.HasLayer(level, layer)) continue;
                    const auto& mip = mips[level];
                    detiler.Dispatch(commands, descriptor.tileMode, elementBytes, tiled.Handle(), geometry.GuestOffset(layer, mip), linear.Handle(), geometry.LinearLayerOffset(layer) + mip.linearOffset, mip, false, layer, geometry.thick, {.pipeBankXor = descriptor.pipeBankXor});
                }
            }
            const auto linearRead = WholeBufferBarrier(linear.Handle(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransfer.srcAccessMask = selectedCpu ? VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0u;
            toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransfer.oldLayout = selectedCpu ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
            toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer.image = image;
            toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, descriptor.mipCount, 0, VK_REMAINING_ARRAY_LAYERS};
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | (selectedCpu ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : 0u), VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &linearRead, 1, &toTransfer);
            const auto regions = CopyRegions(selectedCpu ? &cpuLayers : nullptr);
            context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear.Handle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data());
            VkImageMemoryBarrier toGeneral = toTransfer;
            toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
            APS5_LOG_CHARS_OUT_DEBUG("StorageTexture upload submit");
            batch.SubmitAndWait();
            APS5_LOG_CHARS_OUT_DEBUG("StorageTexture upload done");
    }
    countStorageUpload(2, uploadedBytes);
    ++version;
    if (profile) LookupOutcomes::Add(LookupOutcomes::UploadCpu, start);
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
