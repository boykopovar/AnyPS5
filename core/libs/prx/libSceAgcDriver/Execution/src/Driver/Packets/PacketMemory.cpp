#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include <cstdlib>
#include <cstring>
#include <limits>

namespace AgcDriver::DriverDetail {

bool Driver::preparePacketMemory(const Submission& submission, QueueState& queue, std::span<const std::uint32_t> packet, std::uint32_t header, std::uint32_t opcode, bool& wroteOnGpu, bool& endOfPipeInterrupt, bool& interruptDeferred, bool& drawPacket, bool& sampleDump) {
    static const bool drainAll = std::getenv("APS5_DRAIN_ALL") != nullptr;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    wroteOnGpu = false;

    bool orderedAlready = false;

    const auto interruptSelect = (packet[2] >> 24u) & 7u;
    endOfPipeInterrupt = opcode == 0x49 && interruptSelect != 0 && interruptSelect != 3;
    interruptDeferred = false;
    if (!drainAll && endOfPipeInterrupt) {
        const auto label = Pm4::DecodeLabelWrite(packet);
        const bool storesNothing = (packet[2] >> 29u) == 0 || (packet[3] | (static_cast<std::uint64_t>(packet[4]) << 32u)) == 0;
        if (label.has_value() || storesNothing) {
            bumpEpoch(&EpochBumps::drains);
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            const auto localDevice = device.Load();
            recordDeferredLabels(localDevice.get(), submission.queue);
            const bool workOpen = Graphics::Recorder::RecordedWorkSinceSubmit() != 0;
            int reason = localDevice != nullptr ? 0 : 1;
            if (label.has_value()) {
                const auto bytes = label->Bytes();
                const auto stamp = ++eventSerial;
                reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(label->address, bytes, stamp, submission.queue) : 4;
                if (reason == 1) GuestMemory::Write(label->address, bytes, 4);
                if (reason == 0 || reason == 1 || reason == 5 || reason == 6) {
                    noteLabelStore(label->address, bytes, stamp);
                    Graphics::Recorder::CloseLabelGroup(GuestMemory::TrackerGeneration());
                }
                countLabelOutcome(reason);
                ++immediateLabels;
            } else {
                ++noOpLabels;
            }
            if (reason == 0 || reason == 5 || reason == 6) {
                const auto queueId = submission.queue;
                interruptDeferred = localDevice->AfterRecordedWork([queueId] { AgcDriverDeliverEopInterrupt(queueId); }, submission.queue == 0);
                if (interruptDeferred && workOpen) localDevice->SubmitRecorded(submission.queue == 0);
                wroteOnGpu = true;
            } else if (reason == 1) {
                wroteOnGpu = true;
            }
        }
    }
    if (!drainAll && !endOfPipeInterrupt && (opcode == 0x49 || opcode == 0x37)) {
        if (const auto label = Pm4::DecodeLabelWrite(packet)) {
            const auto bytes = label->Bytes();
            if (DeferLabels() && bytes.size() <= DeferredLabel::Capacity && bytes.size() % 4 == 0 && label->address % 4 == 0) {

                auto& deferred = deferredLabels();
                if (deferred.labels.empty()) deferred.since = std::chrono::steady_clock::now();
                auto& entry = deferred.labels.emplace_back();
                entry.address = label->address;
                entry.size = bytes.size();
                std::memcpy(entry.bytes.data(), bytes.data(), bytes.size());
                ++queuedLabels;
                wroteOnGpu = true;
            } else {
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
                std::lock_guard gpuLock(GuestMemory::GpuMutex());
                const auto localDevice = device.Load();

                recordDeferredLabels(localDevice.get(), submission.queue);
                const auto stamp = ++eventSerial;
                const auto reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(label->address, bytes, stamp, submission.queue) : 4;
                wroteOnGpu = reason == 0 || reason == 5 || reason == 6;
                if (reason == 1) {

                    GuestMemory::Write(label->address, bytes, 4);
                    wroteOnGpu = true;
                }
                if (wroteOnGpu) noteLabelStore(label->address, bytes, stamp);
                Graphics::Recorder::CloseLabelGroup(GuestMemory::TrackerGeneration());
                countLabelOutcome(reason);
                ++immediateLabels;
            }
        } else if (opcode == 0x49 ? ((packet[2] >> 29u) == 0 || (packet[3] | (static_cast<std::uint64_t>(packet[4]) << 32u)) == 0) : (packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)) == 0) {
            orderedAlready = true;
            ++noOpLabels;
        } else {
            ++labelFallbacks[4];
        }
    }

    static const bool cpuStores = std::getenv("APS5_CPU_STORES") != nullptr;
    if (!drainAll && !cpuStores && (opcode == 0x40 || opcode == 0x50 || opcode == 0x83)) {
        constexpr std::size_t gpuStoreLimit = 65536;
        bool drained = true;
        if (const auto store = Pm4::ResolveStore(packet, queue, gpuStoreLimit)) {
            const auto bytes = store->Bytes();
            if (bytes.empty()) {

                orderedAlready = true;
                drained = false;
            } else {

                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
                std::lock_guard gpuLock(GuestMemory::GpuMutex());
                const auto localDevice = device.Load();

                Graphics::StorageTexture::FlushPending(store->address, bytes.size(), nullptr, "packet store", Graphics::PublishScope::PartialUnits);

                recordDeferredLabels(localDevice.get(), submission.queue);
                const auto stamp = ++eventSerial;
                const auto reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(store->address, bytes, stamp, submission.queue) : 4;
                if (reason == 0 || reason == 1 || reason == 5 || reason == 6) noteLabelStore(store->address, bytes, stamp);
                if (reason == 0 || reason == 5 || reason == 6) {
                    if (reason == 0) ++storesOnGpu;
                    else ++storesBehindCompletions;
                    wroteOnGpu = true;
                    drained = false;
                } else if (reason == 1) {
                    GuestMemory::Write(store->address, bytes, 1);
                    ++storesOnCpu;
                    wroteOnGpu = true;
                    drained = false;
                }
                Graphics::Recorder::CloseLabelGroup(GuestMemory::TrackerGeneration());
            }
        }
        if (drained && opcode == 0x50) {
            const auto copy = Pm4::DecodeMemoryCopy(packet);
            if (copy.has_value() && copy->bytes > gpuStoreLimit && GuestMemory::Accessible(reinterpret_cast<const void*>(copy->source), copy->bytes) && GuestMemory::Accessible(reinterpret_cast<const void*>(copy->destination), copy->bytes, true)) {
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Copy);
                std::lock_guard gpuLock(GuestMemory::GpuMutex());
                if (const auto localDevice = device.Load()) {
                    recordDeferredLabels(localDevice.get(), submission.queue);
                    const auto outcome = localDevice->CopyBuffer(copy->destination, copy->source, copy->bytes, 0, std::numeric_limits<std::size_t>::max(), 0, 0, submission.queue, [](std::span<const std::byte>, std::uint64_t) {});
                    if (outcome.path == 1 || outcome.path == 3) {
                        wroteOnGpu = true;
                        drained = false;
                    }
                }
            }
        }
        if (drained) ++storesDrained;
    }

    static const bool drawDrain = std::getenv("APS5_DRAW_DRAIN") != nullptr;
    drawPacket = Pm4::DrawOpcode(opcode);
    sampleDump = opcode == 0x46 && (packet[1] & 0x3fu) == 0x39u;
    if (sampleDump && !drainAll) {
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        if (const auto localDevice = device.Load()) {
            recordDeferredLabels(localDevice.get(), submission.queue);
            wroteOnGpu = localDevice->DumpSamplesOnGpu(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u));
        }
    }

    static const bool syncFlip = std::getenv("APS5_SYNC_FLIP") != nullptr;
    const bool drains = drainAll ? ((Pm4::AccessesMemory(header) && opcode != 0x16) || opcode == 0x42 || opcode == 0x46 || opcode == 0x58 || header == FlipPacketHeader)
                                 : (!wroteOnGpu && !orderedAlready && (opcode == 0x49 || opcode == 0x37 || opcode == 0x40 || opcode == 0x45 || opcode == 0x50 || opcode == 0x83 || sampleDump || (drawPacket && drawDrain) || (header == FlipPacketHeader && syncFlip)));
    if (drains) {

        static const bool unlockedDrain = std::getenv("APS5_NO_UNLOCKED_DRAIN") == nullptr && !drainAll;

        bumpEpoch(&EpochBumps::drains);
        std::shared_ptr<VulkanDevice> draining;
        std::uint64_t epoch = 0;
        {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            if (profile) {
                ++drainCounts[header == FlipPacketHeader ? 0xffffu : opcode];
                ++drainTotal;
                if (std::chrono::steady_clock::now() - lastSyncReport > std::chrono::seconds(10)) {
                    lastSyncReport = std::chrono::steady_clock::now();
                    reportSync();
                }
            }
            draining = device.Load();

            recordDeferredLabels(draining.get(), submission.queue);
            if (draining != nullptr) {
                if (unlockedDrain && draining->CanWaitUnlocked()) epoch = draining->SubmitAndEpoch();
                else draining->WaitIdle();
            }
            if (epoch == 0) draining.reset();
        }
        if (epoch != 0) {
            ++unlockedDrains;
            try {
                draining->WaitRecorded(epoch);
            } catch (...) {

                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                std::lock_guard gpuLock(GuestMemory::GpuMutex());
                draining.reset();
                throw;
            }
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            draining->ReapRecorded(epoch);
            draining.reset();
        }
    }
    return drains;
}

void Driver::dumpSampleCounters(std::uint64_t address) {
    std::uint64_t samples = 0;
    {
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        auto* recorder = Graphics::Recorder::Active();
        require(recorder != nullptr, "occlusion counters without the command recorder are not implemented");
        recorder->CountSamples();
        samples = recorder->SamplesTotal();
    }
    constexpr std::uint64_t ready = 1ull << 63u;
    for (std::uint64_t db = 0; db < 16; ++db) {
        const std::uint64_t value = ready | (db == 0 ? samples : 0u);
        GuestMemory::Write(address + db * 16u, std::as_bytes(std::span(&value, 1)), 8);
    }
}

bool Driver::enqueueDmaPacket(std::span<const std::uint32_t> packet, std::uint32_t opcode, std::uint32_t queue, const QueueState& state) {
    static const bool enabled = [] { const char* text = std::getenv("APS5_PIPELINE_DMA"); return text == nullptr || std::strcmp(text, "0") != 0; }();
    if (!enabled || opcode != 0x50 || queue != 0 || packet.size() < 7 || !DrawPipeline::Active() || !deferredLabels().labels.empty()) return false;
    const auto source = ((packet[1] >> 29u) & 3u) | ((packet[6] >> 24u) & 4u) | ((packet[6] >> 25u) & 8u);
    constexpr std::size_t immediateLimit = std::size_t{16} << 20u;
    if (source == 2) {
        const auto store = Pm4::ResolveStore(packet, state, immediateLimit);
        if (!store.has_value() || store->Bytes().empty() || !GuestMemory::Accessible(reinterpret_cast<const void*>(store->address), store->Bytes().size(), true)) return false;
        const auto address = store->address;
        std::vector<std::byte> bytes(store->Bytes().begin(), store->Bytes().end());
        const auto size = bytes.size();
        DrawPipeline::Queue0().Enqueue([this, queue, address, bytes = std::move(bytes)] {
            GuestMemory::SetCurrentPacket(0x50, queue);
            commitDmaStore(queue, address, bytes);
        }, {{address, address + size}});
        return true;
    }
    const auto copy = Pm4::DecodeMemoryCopy(packet);
    if (!copy.has_value() || copy->destination % 4 != 0 || copy->bytes % 4 != 0 || !GuestMemory::Accessible(reinterpret_cast<const void*>(copy->source), copy->bytes) || !GuestMemory::Accessible(reinterpret_cast<const void*>(copy->destination), copy->bytes, true)) return false;
    DrawPipeline::Queue0().Enqueue([this, queue, copy = *copy] {
        GuestMemory::SetCurrentPacket(0x50, queue);
        constexpr std::size_t gpuStoreLimit = 65536;
        if (copy.bytes > gpuStoreLimit) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Copy);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            if (const auto localDevice = device.Load()) {
                recordDeferredLabels(localDevice.get(), queue);
                const auto outcome = localDevice->CopyBuffer(copy.destination, copy.source, copy.bytes, 0, std::numeric_limits<std::size_t>::max(), 0, 0, queue, [](std::span<const std::byte>, std::uint64_t) {});
                if (outcome.path == 1 || outcome.path == 3) return;
            }
        }
        std::vector<std::byte> bytes(copy.bytes);
        GuestMemory::Read(copy.source, bytes, 1);
        commitDmaStore(queue, copy.destination, bytes);
    }, {{copy->destination, copy->destination + copy->bytes}});
    return true;
}

void Driver::commitDmaStore(std::uint32_t queue, std::uint64_t address, std::span<const std::byte> bytes) {
    constexpr std::size_t gpuStoreLimit = 65536;
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    const auto localDevice = device.Load();
    Graphics::StorageTexture::FlushPending(address, bytes.size(), nullptr, "packet store", Graphics::PublishScope::PartialUnits);
    recordDeferredLabels(localDevice.get(), queue);
    int reason = 4;
    std::uint64_t stamp = 0;
    if (bytes.size() <= gpuStoreLimit && localDevice != nullptr) {
        stamp = ++eventSerial;
        reason = localDevice->WriteLabelOnGpu(address, bytes, stamp, queue);
    }
    if (reason == 0 || reason == 5 || reason == 6) {
        noteLabelStore(address, bytes, stamp);
        if (reason == 0) ++storesOnGpu;
        else ++storesBehindCompletions;
    } else {
        if (reason != 1 && localDevice != nullptr) localDevice->WaitIdle();
        GuestMemory::Write(address, bytes, 1);
        noteLabelStore(address, bytes, stamp != 0 ? stamp : ++eventSerial);
        ++storesOnCpu;
    }
    Graphics::Recorder::CloseLabelGroup(GuestMemory::TrackerGeneration());
}

bool Driver::enqueueLabelPacket(std::span<const std::uint32_t> packet, std::uint32_t opcode, std::uint32_t queue) {
    static const bool ordered = std::getenv("APS5_PIPELINE_DRAIN_LABELS") == nullptr;
    if (!ordered || !DeferLabels() || (opcode != 0x49 && opcode != 0x37)) return false;
    const bool endOfPipeInterrupt = opcode == 0x49 && ((packet[2] >> 24u) & 7u) != 0;
    const auto label = Pm4::DecodeLabelWrite(packet);
    if (!label.has_value()) {
        if (opcode != 0x49) return false;
        const bool storesNothing = (packet[2] >> 29u) == 0 || (packet[3] | (static_cast<std::uint64_t>(packet[4]) << 32u)) == 0;
        if (!storesNothing) return false;
        if (!endOfPipeInterrupt) {
            ++noOpLabels;
            return true;
        }
    }
    std::uint64_t address = 0;
    std::vector<std::byte> bytes;
    std::vector<DrawPipeline::Range> writes;
    if (label.has_value()) {
        const auto stored = label->Bytes();
        address = label->address;
        bytes.assign(stored.begin(), stored.end());
        writes.emplace_back(address, address + bytes.size());
    }
    if (endOfPipeInterrupt) bumpEpoch(&EpochBumps::drains);
    auto stored = bytes;
    DrawPipeline::Queue0().Enqueue([this, queue, opcode, address, bytes = std::move(bytes), endOfPipeInterrupt] {
        GuestMemory::SetCurrentPacket(opcode, queue);
        commitLabel(queue, address, bytes, endOfPipeInterrupt);
    }, std::move(writes), address, std::move(stored));
    return true;
}

void Driver::commitLabel(std::uint32_t queue, std::uint64_t address, std::span<const std::byte> bytes, bool endOfPipeInterrupt) {
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    const auto localDevice = device.Load();
    recordDeferredLabels(localDevice.get(), queue);
    const bool workOpen = Graphics::Recorder::RecordedWorkSinceSubmit() != 0;
    int reason = localDevice != nullptr ? 0 : 1;
    if (!bytes.empty()) {
        const auto stamp = ++eventSerial;
        reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(address, bytes, stamp, queue) : 4;
        if (reason != 0 && reason != 5 && reason != 6) {
            if (reason != 1 && localDevice != nullptr) localDevice->WaitIdle();
            GuestMemory::Write(address, bytes, 4);
        }
        noteLabelStore(address, bytes, stamp);
        Graphics::Recorder::CloseLabelGroup(GuestMemory::TrackerGeneration());
        countLabelOutcome(reason);
        ++immediateLabels;
    } else {
        ++noOpLabels;
    }
    if (endOfPipeInterrupt) {
        bool deferred = false;
        if (localDevice != nullptr && (reason == 0 || reason == 5 || reason == 6)) {
            deferred = localDevice->AfterRecordedWork([queue] { AgcDriverDeliverEopInterrupt(queue); }, queue == 0);
            if (deferred && workOpen) localDevice->SubmitRecorded(queue == 0);
        }
        if (!deferred) AgcDriverDeliverEopInterrupt(queue);
    }
    submitDueAfterCommit(localDevice.get());
}

void Driver::submitDueAfterCommit(VulkanDevice* localDevice) {
    if (localDevice == nullptr) return;
    const auto pending = Graphics::Recorder::PendingLabelSince();
    const bool due = pending.has_value() && std::chrono::steady_clock::now() - *pending >= labelFlushDeadline();
    const bool capped = batchCap() != 0 && Graphics::Recorder::RecordedWorkSinceSubmit() >= batchCap();
    if (due || capped) localDevice->SubmitRecorded(true);
}

}
