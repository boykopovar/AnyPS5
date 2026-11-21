#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libkernel/Time/include/SleepWake.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>

namespace AgcDriver::DriverDetail {
namespace {
bool Driver::enqueueConstantRamDump(std::span<const std::uint32_t> packet, std::uint32_t opcode, std::uint32_t queue, const QueueState& state) {
    static const bool enabled = std::getenv("APS5_PIPELINE_DRAIN_DUMPS") == nullptr && std::getenv("APS5_CPU_STORES") == nullptr;
    if (!enabled || opcode != 0x83 || queue != 0 || !DrawPipeline::Active() || !deferredLabels().labels.empty()) return false;
    constexpr std::size_t gpuStoreLimit = 65536;
    const auto store = Pm4::ResolveStore(packet, state, gpuStoreLimit);
    if (!store.has_value()) return false;
    const auto stored = store->Bytes();
    if (stored.empty()) return true;
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(store->address), stored.size(), true)) return false;
    const auto address = store->address;
    std::vector<std::byte> bytes(stored.begin(), stored.end());
    auto queued = bytes;
    DrawPipeline::Queue0().EnqueueStore([this, queue, address, bytes = std::move(bytes)] {
        GuestMemory::SetCurrentPacket(0x83, queue);
        commitStore(queue, address, bytes);
    }, address, std::move(queued));
    return true;
}

void Driver::commitStore(std::uint32_t queue, std::uint64_t address, std::span<const std::byte> bytes) {
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
        noteLabelStore(address, bytes, stamp, queue, false);
        if (reason == 0) ++storesOnGpu;
        else ++storesBehindCompletions;
    } else {
        if (reason != 1 && reason != 7 && localDevice != nullptr) localDevice->WaitIdle();
        GuestMemory::Write(address, bytes, 1);
        noteLabelStore(address, bytes, stamp != 0 ? stamp : ++eventSerial, queue, false);
        if (reason == 7) ++storesSyncedUnimported;
        else ++storesOnCpu;
    }
    Graphics::Recorder::CloseLabelGroup(GuestMemory::TrackerGeneration());
}

bool Driver::labelPacketQueues(std::span<const std::uint32_t> packet, std::uint32_t opcode) {
    static const bool ordered = std::getenv("APS5_PIPELINE_DRAIN_LABELS") == nullptr;
    if (!ordered || !DeferLabels() || (opcode != 0x49 && opcode != 0x37) || !deferredLabels().labels.empty()) return false;
    if (Pm4::DecodeLabelWrite(packet).has_value()) return true;
    return opcode == 0x49 && ((packet[2] >> 29u) == 0 || (packet[3] | (static_cast<std::uint64_t>(packet[4]) << 32u)) == 0);
}

bool Driver::enqueueLabelPacket(std::span<const std::uint32_t> packet, std::uint32_t opcode, std::uint32_t queue, std::optional<std::uint32_t> interrupt) {
    if (!labelPacketQueues(packet, opcode)) return false;
    const bool endOfPipeInterrupt = interrupt.has_value();
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
    if (endOfPipeInterrupt) {
        bumpEpoch(&EpochBumps::drains);
        ++DrainEpochCounts().queuedInterrupts;
    }
    auto stored = bytes;
    auto labelBytes = bytes;
    auto enqueue =[this, queue, opcode, address, bytes = std::move(bytes), interrupt, writes, stored = std::move(stored)]() mutable {
        DrawPipeline::Queue0().Enqueue(DrawPipeline::CommitKind::EndOfPipe, [this, queue, opcode, address, bytes = std::move(bytes), interrupt] {
            GuestMemory::SetCurrentPacket(opcode, queue);
            commitLabel(queue, address, bytes, interrupt);
        }, std::move(writes), address, std::move(stored));
    };
    if (queue == 0 && DrawFinisher::Overlap() && DrawFinisher::Busy()) {
        DrawFinisher::Pending held;
        held.unknown = false;
        held.writes = std::move(writes);
        held.labelAddress = address;
        held.labelBytes = std::move(labelBytes);
        DrawFinisher::Get().Enqueue(std::move(enqueue), std::move(held));
        return true;
    }
    enqueue();
    return true;
}

void Driver::commitLabel(std::uint32_t queue, std::uint64_t address, std::span<const std::byte> bytes, std::optional<std::uint32_t> interrupt) {
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    const auto localDevice = device.Load();
    recordDeferredLabels(localDevice.get(), queue);
    bool workOpen = Graphics::Recorder::RecordedWorkSinceSubmit() != 0;
    int reason = localDevice != nullptr ? 0 : 1;
    if (!bytes.empty()) {
        const auto stamp = ++eventSerial;
        if (localDevice != nullptr && queue == 0 && Graphics::StorageTexture::StoreDrawnLinear("label") != 0) workOpen = true;
        reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(address, bytes, stamp, queue) : 4;
        if (reason != 0 && reason != 5 && reason != 6) {
            if (reason != 1 && reason != 7 && localDevice != nullptr) localDevice->WaitIdle();
            GuestMemory::Write(address, bytes, 4);
            KernelWakeSleepers_nid_no_patch();
        }
        noteLabelStore(address, bytes, stamp, queue);
        Graphics::Recorder::CloseLabelGroup(GuestMemory::TrackerGeneration());
        countLabelOutcome(reason);
        ++immediateLabels;
    } else {
        ++noOpLabels;
    }
    if (interrupt.has_value()) {
        const auto context = *interrupt;
        bool deferred = false;
        if (bytes.empty() && localDevice != nullptr && queue == 0 && Graphics::StorageTexture::StoreDrawnLinear("interrupt") != 0) workOpen = true;
        if (localDevice != nullptr && (reason == 0 || reason == 5 || reason == 6)) {
            deferred = localDevice->AfterRecordedWork([this, queue, context] { deliverGuestEopInterrupt(queue, context); }, queue == 0);
            if (deferred && workOpen) localDevice->SubmitRecorded(queue == 0);
        }
        if (!deferred) deliverGuestEopInterrupt(queue, context);
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
}
