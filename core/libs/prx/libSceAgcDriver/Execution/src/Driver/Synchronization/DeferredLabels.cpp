#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "ThreadOwned.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

DeferredLabels& deferredLabels() {
    static thread_local DeferredLabels* deferred = nullptr;
    return ShaderRecompiler::ThreadOwned(deferred);
}

bool DeferLabels() {

    static const bool defer = std::getenv("APS5_LABEL_LOCK_EACH") == nullptr && std::getenv("APS5_DRAIN_COMPLETION_LABELS") == nullptr;
    return defer;
}

bool QueuedLabelTable() {
    static const bool enabled = std::getenv("APS5_NO_QUEUED_LABELS") == nullptr && std::getenv("APS5_NO_LABEL_BATCHING") == nullptr;
    return enabled;
}

bool LabelBatchSubmit() {
    static const bool enabled = std::getenv("APS5_NO_LABEL_BATCH_SUBMIT") == nullptr && std::getenv("APS5_NO_LABEL_BATCHING") == nullptr;
    return enabled;
}

std::size_t& queuedLabelsNoted() {
    static thread_local std::size_t noted = 0;
    return noted;
}

bool NeedsRecordedLabels(std::uint32_t header) {
    if (header == FlipPacketHeader) return true;
    switch ((header >> 8u) & 0xffu) {
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x26: case 0x28: case 0x2a: case 0x2f:
        case 0x42: case 0x46: case 0x58: case 0x68: case 0x69: case 0x76: case 0x78: case 0x79: case 0x7a: case 0x81:
            return false;
        default: return true;
    }
}

bool PacketLocksItself(std::uint32_t header) {
    if (header == FlipPacketHeader) return true;
    switch ((header >> 8u) & 0xffu) {
        case 0x15: case 0x16: case 0x27: case 0x2d: case 0x35: case 0x24: case 0x25: case 0x2c: case 0x38: return true;
        default: return false;
    }
}

bool Driver::labelTryEachPacket() {
    static const bool each = std::getenv("APS5_LABEL_TRY_EACH_PACKET") != nullptr;
    return each;
}

std::chrono::microseconds Driver::labelFlushDeadline() {
    static const std::chrono::microseconds value = [] {
        const char* text = std::getenv("APS5_LABEL_FLUSH_US");
        return std::chrono::microseconds(text ? std::atoi(text) : (LabelBatchSubmit() ? 1000 : 250));
    }();
    return value;
}

bool Driver::labelFlushDue() {
    if (QueuedLabelTable() && !deferredLabels().labels.empty()) Get().recordQueuedLabelsByTry(GuestMemory::GpuLockThreadTag(), pollLabelRecords);
    const auto since = Graphics::Recorder::PendingLabelSince();
    return since.has_value() && std::chrono::steady_clock::now() - *since >= labelFlushDeadline();
}

bool Driver::completionsPending() {
    return Graphics::Recorder::PendingCompletionLabels() != 0 || Graphics::Recorder::PendingWriteBackCompletions() != 0;
}

bool Driver::reapEachPacket() {

    static const bool each = std::getenv("APS5_REAP_EACH_PACKET") != nullptr;
    return each;
}

std::uint64_t Driver::batchCap() {
    static const std::uint64_t value = [] {
        const char* text = std::getenv("APS5_BATCH_CAP");
        return text ? std::strtoull(text, nullptr, 10) : 64ull;
    }();
    return value;
}

void Driver::recordDeferredLabels(VulkanDevice* localDevice, std::uint32_t queue) {
    auto& deferred = deferredLabels();
    if (deferred.labels.empty()) return;

    static thread_local std::vector<DeferredLabel>* recording = nullptr;
    auto& labels = ShaderRecompiler::ThreadOwned(recording);
    labels.clear();
    labels.swap(deferred.labels);
    struct Clear {
        std::vector<DeferredLabel>& labels;
        std::uint32_t queue;
        ~Clear() {
            const bool failed = std::uncaught_exceptions() != 0;
            if (failed) std::fprintf(stderr, "[gpu] queue 0x%x dropped %zu queued labels: their record failed\n", queue, labels.size());
            labels.clear();

            Graphics::Recorder::CloseLabelGroup(failed ? 0 : GuestMemory::TrackerGeneration());

            Graphics::Recorder::ForgetQueuedLabels();
            queuedLabelsNoted() = 0;
        }
    } clear{labels, queue};
    bool first = true;
    for (const auto& label : labels) {
        const auto bytes = std::span<const std::byte>(label.bytes).first(label.size);
        const auto stamp = ++eventSerial;
        const int reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(label.address, bytes, stamp, queue, first) : 4;
        first = false;
        countLabelOutcome(reason);
        if (reason != 0 && reason != 5 && reason != 6) {
            if (reason != 1 && localDevice != nullptr) localDevice->WaitIdle();
            GuestMemory::Write(label.address, bytes, 4);
        }
        noteLabelStore(label.address, bytes, stamp);
    }
    ++labelGroups;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;

    if (profile && (labelGroups.load(std::memory_order_relaxed) & 63u) == 0 && std::chrono::steady_clock::now() - lastSyncReport > std::chrono::seconds(10)) {
        lastSyncReport = std::chrono::steady_clock::now();
        reportSync();
    }
}

bool Driver::recordLabelsForPacket(VulkanDevice* localDevice, std::uint32_t queue) {
    static const bool packetFlush = !LabelBatchSubmit() && std::getenv("APS5_NO_LABEL_PACKET_FLUSH") == nullptr;

    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    if (!deferredLabels().labels.empty()) {
        recordDeferredLabels(localDevice, queue);
        ++packetLockRecords;
        if (profile) packetLockRecordUs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
    }
    if (localDevice == nullptr || !packetFlush || !Graphics::Recorder::PendingLabelSince().has_value()) return false;

    const auto submitStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    localDevice->SubmitRecorded(queue == 0);
    ++packetLockSubmits;
    if (profile) packetLockSubmitUs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - submitStart).count());
    return true;
}

void Driver::recordQueuedLabelsBeforeRead(std::uint32_t queue) {
    if (deferredLabels().labels.empty()) return;
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    const auto localDevice = device.Load();
    recordDeferredLabels(localDevice.get(), queue);
}

bool Driver::recordQueuedLabelsAfterCapture(std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> regions) {
    const auto& labels = deferredLabels().labels;
    if (labels.empty()) return false;
    for (const auto& label : labels) {
        for (const auto& region : regions) {
            if (label.address < region.guestAddress + region.bytes.size() && region.guestAddress < label.address + label.size) {
                recordQueuedLabelsBeforeRead(queue);
                ++captureRetries;
                return true;
            }
        }
    }
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
    std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
    if (!gpuLock.try_lock()) {
        ++triesFailed[TryCapture];
        return false;
    }
    const auto localDevice = device.Load();
    recordDeferredLabels(localDevice.get(), queue);
    ++captureTryRecords;
    return false;
}

void Driver::noteQueuedLabels(std::uint32_t queue) {
    static const bool installed = (Graphics::Recorder::SetQueuedLabelRecorder(&Driver::recordQueuedLabelsFromHook), true);
    (void)installed;
    const auto& labels = deferredLabels().labels;
    for (auto& noted = queuedLabelsNoted(); noted < labels.size(); ++noted) {
        const auto& label = labels[noted];
        Graphics::Recorder::NoteQueuedLabel(label.address, std::span<const std::byte>(label.bytes).first(label.size), ++eventSerial, queue);
    }
}

void Driver::recordQueuedLabelsByTry(std::uint32_t queue, std::atomic<std::uint64_t>& counter) {
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
    std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
    if (!gpuLock.try_lock()) {
        ++triesFailed[TryPollRecord];
        return;
    }
    const auto localDevice = device.Load();
    recordDeferredLabels(localDevice.get(), queue);
    ++counter;
}

void Driver::recordQueuedLabelsFromHook() {
    if (deferredLabels().labels.empty()) return;
    auto& driver = Get();
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    const auto localDevice = driver.device.Load();
    driver.recordDeferredLabels(localDevice.get(), GuestMemory::GpuLockThreadTag());
    ++hookLabelRecords;
}

}
