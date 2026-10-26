#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

void Driver::flushBetweenPackets(std::uint32_t queue, std::uint32_t header, bool labelPacket) {
    static const bool packetFlush = !LabelBatchSubmit() && std::getenv("APS5_NO_LABEL_PACKET_FLUSH") == nullptr;
    static const bool ownLock = std::getenv("APS5_LABEL_OWN_LOCK") != nullptr;
    static std::atomic<std::uint64_t> packetFlushes{0}, deadlineFlushes{0}, capFlushes{0}, boundaryReaps{0}, overdueLocks{0};
    auto& deferred = deferredLabels();
    const bool queued = !deferred.labels.empty();
    const bool queuedTable = queued && QueuedLabelTable();
    if (queuedTable) noteQueuedLabels(queue);
    const bool selfLocking = !ownLock && PacketLocksItself(header);
    bool submit = false, record = false, needsRecord = false, deferredDue = false, pendingDue = false;
    const auto pending = Graphics::Recorder::PendingLabelSince();

    thread_local std::chrono::steady_clock::time_point lastReapTry{};
    const bool completions = completionsPending();
    const bool reapDue = completions && (reapEachPacket() || queue == 0);

    const auto now = queued || pending.has_value() || reapDue ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};

    bool recordAtLock = false;
    if (queued) {
        if (!labelPacket) {
            record = true;

            needsRecord = !queuedTable && NeedsRecordedLabels(header) && !selfLocking;
        }
        if (now - deferred.since >= labelFlushDeadline()) record = deferredDue = true;
        if (record && !deferredDue && selfLocking && !labelTryEachPacket()) {
            record = false;
            recordAtLock = true;
        }
    }
    if (pending.has_value()) {
        if (!labelPacket && packetFlush) submit = true;
        if (now - *pending >= labelFlushDeadline()) submit = pendingDue = true;
    }
    const auto work = Graphics::Recorder::RecordedWorkSinceSubmit();
    const bool capped = !submit && !record && batchCap() != 0 && work >= batchCap();
    const bool reap = !submit && !record && !capped && reapDue && (reapEachPacket() || now - lastReapTry >= ReapInterval);
    if (!submit && !record && !capped && !reap) {
        if (recordAtLock) ++recordTriesSkipped;
        return;
    }
    if (reap) lastReapTry = now;
    if (DrawPipeline::Active() && (submit || record || capped || recordAtLock)) {
        static const bool committerSubmits = std::getenv("APS5_PIPELINE_DRAIN_LABELS") == nullptr;
        if (committerSubmits && !record && !recordAtLock && DrawPipeline::Queue0().Busy()) return;
        DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Flush);
    }
    std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);

    if (record) GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
    else if (submit || capped) GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);

    bool outright = needsRecord;
    if (LabelBatchSubmit()) {
        const auto grace = 4 * labelFlushDeadline();
        const bool overdue = (deferredDue && now - deferred.since >= grace) || (pendingDue && now - *pending >= grace) || (capped && work >= 2 * batchCap());
        if (overdue) ++overdueLocks;
        outright = outright || overdue;
    } else if (deferredDue || (queue == 0 && (capped || (submit && (!selfLocking || pendingDue))))) {
        outright = true;
    }
    if (outright) {
        gpuLock.lock();
    } else if (!gpuLock.try_lock()) {
        ++triesFailed[record ? TryLabel : submit || capped ? TryFlush : TryReap];
        if (selfLocking && record) ++packetLockDeferred;
        else if (selfLocking && submit) ++packetSubmitDeferred;
        return;
    }
    const auto localDevice = device.Load();
    if (record || recordAtLock) {
        recordDeferredLabels(localDevice.get(), queue);

        if ((!labelPacket && packetFlush) || deferredDue) submit = true;
    }
    if (localDevice == nullptr) return;
    if (submit || capped) {

        if (!Graphics::Recorder::PendingLabelSince().has_value() && (batchCap() == 0 || Graphics::Recorder::RecordedWorkSinceSubmit() < batchCap())) return;
        localDevice->SubmitRecorded(queue == 0);
        if (capped) ++capFlushes;
        else if (!labelPacket && packetFlush) ++packetFlushes;
        else ++deadlineFlushes;
    } else if (reap) {
        localDevice->ReapRecorded();
        ++boundaryReaps;
    }
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;

    static std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    if (!profile) return;
    const auto reportNow = std::chrono::steady_clock::now();
    if (reportNow - lastReport < std::chrono::seconds(10)) return;
    lastReport = reportNow;
    const auto stores = Graphics::Recorder::StoreCounts();
    const auto failed = [](TrySite site) { return static_cast<unsigned long long>(triesFailed[site].load()); };
    AgcDriver::ProfilePrint_nid_no_patch("[labels] batch flushes between packets: %llu at the next packet, %llu by deadline, %llu by size cap (%llu well overdue, waited for the mutex); %llu boundary reaps; record tries skipped (self-locking next) %llu; failed tries: label %llu, flush %llu, reap %llu, poll %llu, idle %llu, capture %llu, poll-record %llu; at the packet's own lock: %llu label groups (%.0f ms), %llu submits (%.0f ms incl. queue 0's reaps), left to it by a failed try: %llu groups, %llu submits; after a capture: %llu groups recorded by a try, %llu packets redone for a queued label over the capture; %llu suspend points; stores: %llu in %llu runs (%llu joined, %llu replaced, %llu WAW barriers, %llu joins refused; %llu runs recorded at submit, %llu in place before a writer or reader); DCC key stores: %llu queued in %llu runs (%llu before a writer, %llu joined a queued range); queued labels: %llu noted (%llu over a recorded entry), %llu same-queue wait hits, %llu groups recorded from a poll loop, %llu from the flush hook (%llu hook accesses, %llu inside a completion skipped)\n", static_cast<unsigned long long>(packetFlushes.load()), static_cast<unsigned long long>(deadlineFlushes.load()), static_cast<unsigned long long>(capFlushes.load()), static_cast<unsigned long long>(overdueLocks.load()), static_cast<unsigned long long>(boundaryReaps.load()), static_cast<unsigned long long>(recordTriesSkipped.load()), failed(TryLabel), failed(TryFlush), failed(TryReap), failed(TryPoll), failed(TryIdle), failed(TryCapture), failed(TryPollRecord), static_cast<unsigned long long>(packetLockRecords.load()), packetLockRecordUs.load() / 1000.0, static_cast<unsigned long long>(packetLockSubmits.load()), packetLockSubmitUs.load() / 1000.0, static_cast<unsigned long long>(packetLockDeferred.load()), static_cast<unsigned long long>(packetSubmitDeferred.load()), static_cast<unsigned long long>(captureTryRecords.load()), static_cast<unsigned long long>(captureRetries.load()), static_cast<unsigned long long>(suspendPoints.load()), static_cast<unsigned long long>(stores.stores), static_cast<unsigned long long>(stores.runs), static_cast<unsigned long long>(stores.joined), static_cast<unsigned long long>(stores.replaced), static_cast<unsigned long long>(stores.wawBarriers), static_cast<unsigned long long>(stores.joinsRefused), static_cast<unsigned long long>(stores.runsAtSubmit), static_cast<unsigned long long>(stores.runsForced), static_cast<unsigned long long>(stores.keyStores), static_cast<unsigned long long>(stores.keyStoreRuns), static_cast<unsigned long long>(stores.keyStoreRunsForWriter), static_cast<unsigned long long>(stores.keyStoresJoined), static_cast<unsigned long long>(stores.queuedNoted), static_cast<unsigned long long>(stores.queuedOverRecorded), static_cast<unsigned long long>(stores.queuedHits), static_cast<unsigned long long>(pollLabelRecords.load()), static_cast<unsigned long long>(hookLabelRecords.load()), static_cast<unsigned long long>(stores.queuedHookRecords), static_cast<unsigned long long>(stores.queuedHookInCompletion));
}

}
