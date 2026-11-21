#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libkernel/Time/include/SleepWake.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionSnapshots.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/MeshArguments_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBack_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBackBound_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleCounter_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleDumps_spv.h"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/DispatchArguments_spv.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include "prx/libc/include/general/AtomicSharedPtr.hpp"

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
std::uint64_t Recorder::SubmitAndEpoch() {
    Submit();
    return inFlight.empty() ? 0 : submissions;
}

std::uint64_t Recorder::OpenBatchSerial() {
    return open != nullptr ? submissions + 1 : 0;
}

namespace {

struct UnlockedWaiter {
    std::atomic<int>& count;
    explicit UnlockedWaiter(std::uint64_t recorderId) : count(WaitersOf(recorderId)) { count.fetch_add(1, std::memory_order_acq_rel); }
    ~UnlockedWaiter() { count.fetch_sub(1, std::memory_order_acq_rel); }
    UnlockedWaiter(const UnlockedWaiter&) = delete;
    UnlockedWaiter& operator=(const UnlockedWaiter&) = delete;
};

struct RestoreSyncSite {
    std::size_t site;
    ~RestoreSyncSite() { activeSyncSite = site; }
};

VkResult WaitTimeline(VkDevice device, VkSemaphore timeline, PFN_vkWaitSemaphoresKHR waitSemaphores, std::uint64_t serial) {
    VkSemaphoreWaitInfoKHR wait{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR};
    wait.semaphoreCount = 1;
    wait.pSemaphores = &timeline;
    wait.pValues = &serial;
    auto result = waitSemaphores(device, &wait, 5'000'000'000ull);
    for (int waited = 5; result == VK_TIMEOUT; waited += 5) {
        std::fprintf(stderr, "[gpu] recorded batch %llu still running on the GPU after %d s (timeline wait)\n", static_cast<unsigned long long>(serial), waited);
        result = waitSemaphores(device, &wait, 5'000'000'000ull);
    }
    return result;
}

}

void Recorder::WaitSerial(std::uint64_t serial) {
    PerformanceTimer frameTiming("Recorder.WaitSerial");
    if (timeline == VK_NULL_HANDLE || serial == 0) return;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const UnlockedWaiter waiter{id};
    const auto result = WaitTimeline(context.device, timeline, context.Function<PFN_vkWaitSemaphoresKHR>("vkWaitSemaphoresKHR"), serial);
    if (profile) {
        unlockedWaits.fetch_add(1, std::memory_order_relaxed);
        unlockedWaitedUs.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()), std::memory_order_relaxed);
    }
    Check(result, "vkWaitSemaphoresKHR recorder");
}

bool Recorder::WaitSerialFor(std::uint64_t serial, std::uint64_t timeoutNanos) {
    PerformanceTimer frameTiming("Recorder.WaitSerialFor");
    if (timeline == VK_NULL_HANDLE || serial == 0) return false;
    const UnlockedWaiter waiter{id};
    VkSemaphoreWaitInfoKHR wait{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR};
    wait.semaphoreCount = 1;
    wait.pSemaphores = &timeline;
    wait.pValues = &serial;
    const auto result = context.Function<PFN_vkWaitSemaphoresKHR>("vkWaitSemaphoresKHR")(context.device, &wait, timeoutNanos);
    if (result == VK_TIMEOUT) return false;
    Check(result, "vkWaitSemaphoresKHR submission progress");
    return true;
}

void Recorder::FinishUpTo(std::uint64_t serial) {
    ++syncCounts[0];
    announcedSource = 4;
    const void* site = std::exchange(announcedSite, nullptr);
    const auto previousSite = inFlight.empty() || inFlight.front()->serial > serial ? activeSyncSite : BeginSyncSite(0, site != nullptr ? site : __builtin_return_address(0));
    while (!inFlight.empty() && inFlight.front()->serial <= serial) {
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), true, 0);
    }
    activeSyncSite = previousSite;
}

void Recorder::Sync() {
    PerformanceTimer frameTiming("Recorder.Sync");
    const auto source = std::exchange(announcedSource, 4);
    const void* site = std::exchange(announcedSite, nullptr);
    Submit();
    const auto previousSite = inFlight.empty() ? activeSyncSite : BeginSyncSite(source, site != nullptr ? site : __builtin_return_address(0));
    while (!inFlight.empty()) {
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), true, source);
    }
    activeSyncSite = previousSite;
}

void Recorder::FlushDeferredReleases() {
    holdKept.clear();
    ReleaseDeferredKeeps();
}

void Recorder::SyncThrough(std::uint64_t address, std::size_t bytes, bool waitUnlocked) {
    if (bytes == 0) return;
    const auto end = address + bytes;
    if (completionDepth() != 0 && !CompletionStoreSyncs()) {
        announcedSource = 4;
        announcedSite = nullptr;
        ++holdCounters.completionSyncsSkipped;
        return;
    }
    if (completionDepth() != 0) ++holdCounters.completionSyncsWaited;
    const auto completionWaitStart = completionDepth() != 0 && DrawProfiled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    struct CompletionWait {
        std::chrono::steady_clock::time_point start;
        ~CompletionWait() {
            if (start != std::chrono::steady_clock::time_point{}) holdCounters.completionSyncWaitMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
    } completionWait{completionWaitStart};
    if (waitUnlocked) {
        const auto source = announcedSource;
        const void* site = announcedSite != nullptr ? announcedSite : __builtin_return_address(0);
        if (syncThroughUnlocked(address, end, source, site)) return;
        if (completionDepth() == 0) ++holdCounters.hookLockedWaits;
    }
    if (!SyncThroughEnabled() || (open != nullptr && overlaps(*open, address, end))) {
        if (announcedSite == nullptr) announcedSite = __builtin_return_address(0);
        if (open != nullptr || (!inFlight.empty() && unsignaled(inFlight.back()->serial))) ++hookRealWaits;
        Sync();
        return;
    }
    const auto source = std::exchange(announcedSource, 4);
    const void* site = std::exchange(announcedSite, nullptr);
    std::uint64_t targetSerial = 0;
    for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it) {
        if (overlaps(**it, address, end)) {
            targetSerial = (*it)->serial;
            break;
        }
    }
    if (targetSerial == 0) return;
    if (unsignaled(targetSerial)) ++hookRealWaits;
    targetedSyncs.fetch_add(1, std::memory_order_relaxed);
    const RestoreSyncSite restoreSite{BeginSyncSite(source, site != nullptr ? site : __builtin_return_address(0))};
    while (!inFlight.empty() && inFlight.front()->serial <= targetSerial) {
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), true, source);
    }
    batchesLeftInFlight.fetch_add(inFlight.size(), std::memory_order_relaxed);
}

namespace {

struct SyncWaitSite {
    std::uint64_t count = 0;
    std::uint64_t batchesToTarget = 0;
    std::uint64_t unsignaledAtStart = 0;
    std::uint64_t newest = 0;
    double waitedMs = 0;
    double targetAgeMs = 0;
};

struct SyncWaitStats {
    std::array<SyncWaitSite, static_cast<std::size_t>(GuestMemory::ReadSite::Count)> bySite{};
    std::map<std::uint32_t, std::uint64_t> aheadByQueue;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

SyncWaitStats& SyncWaits() {
    static SyncWaitStats stats;
    return stats;
}

bool TraceCapSync() {
    static const bool trace = std::getenv("APS5_TRACE_CAPSYNC") != nullptr;
    return trace;
}

void ReportSyncWaits(SyncWaitStats& stats) {
    std::string report = "[syncwait] unlocked hook waits by read site (10 s; count/GPU wait, avg batches up to the target, avg of them unsignaled at the start, target was the newest submission, avg target submit-to-signal ms):";
    for (std::size_t site = 0; site < stats.bySite.size(); ++site) {
        const auto& entry = stats.bySite[site];
        if (entry.count == 0) continue;
        char text[160];
        std::snprintf(text, sizeof(text), " %s %llu/%.0fms %.1f %.1f %.0f%% %.1f", GuestMemory::ReadSiteName(static_cast<GuestMemory::ReadSite>(site)), static_cast<unsigned long long>(entry.count), entry.waitedMs, static_cast<double>(entry.batchesToTarget) / entry.count, static_cast<double>(entry.unsignaledAtStart) / entry.count, entry.newest * 100.0 / entry.count, entry.targetAgeMs / entry.count);
        report += text;
    }
    report += "; batches ahead by queue:";
    for (const auto& [queue, count] : stats.aheadByQueue) {
        char text[48];
        if (queue == 0xffffffffu) std::snprintf(text, sizeof(text), " untagged %llu", static_cast<unsigned long long>(count));
        else std::snprintf(text, sizeof(text), " 0x%x %llu", queue, static_cast<unsigned long long>(count));
        report += text;
    }
    AgcDriver::ProfilePrint_nid_no_patch("%s\n", report.c_str());
    stats = SyncWaitStats{};
}

}

bool Recorder::syncThroughUnlocked(std::uint64_t address, std::uint64_t end, int source, const void* site) {
    if (HookLockedWait() || timeline == VK_NULL_HANDLE || GuestMemory::GpuMutex().DepthOnThisThread() != 1) return false;
    std::uint64_t targetSerial = 0;
    if (!SyncThroughEnabled() || (open != nullptr && overlaps(*open, address, end))) {
        Submit();
        targetSerial = submissions;
    } else {
        for (auto it = inFlight.rbegin(); it != inFlight.rend(); ++it) {
            if (overlaps(**it, address, end)) {
                targetSerial = (*it)->serial;
                break;
            }
        }
    }
    announcedSource = 4;
    announcedSite = nullptr;
    if (targetSerial == 0) return true;
    if (unsignaled(targetSerial)) ++hookRealWaits;
    const auto myId = id;
    const auto device = context.device;
    const auto semaphore = timeline;
    const auto waitSemaphores = context.Function<PFN_vkWaitSemaphoresKHR>("vkWaitSemaphoresKHR");
    const RestoreSyncSite restoreSite{BeginSyncSite(source, site)};
    const bool profile = DrawProfiled();
    const bool trace = TraceCapSync();
    const auto readSite = GuestMemory::CurrentReadSite();
    const auto newestSerial = submissions;
    std::size_t batchesToTarget = 0, unsignaledAtStart = 0, targetRanges = 0;
    std::pair<std::uint64_t, std::uint64_t> hit{0, 0};
    std::chrono::steady_clock::time_point targetSubmittedAt{};
    std::map<std::uint32_t, std::uint64_t> aheadByQueue;
    if (profile || trace) {
        const auto status = context.Function<PFN_vkGetFenceStatus>("vkGetFenceStatus");
        for (const auto& batch : inFlight) {
            if (batch->serial > targetSerial) break;
            ++batchesToTarget;
            ++aheadByQueue[batch->queue];
            if (status(context.device, batch->fence) != VK_SUCCESS) ++unsignaledAtStart;
            if (batch->serial != targetSerial) continue;
            targetSubmittedAt = batch->submittedAt;
            targetRanges = batch->writes.size();
            for (const auto& range : batch->writes) {
                if (address < range.second && range.first < end) {
                    hit = range;
                    break;
                }
            }
        }
    }
    const auto start = profile || trace ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    auto waited = start;
    auto& mutex = GuestMemory::GpuMutex();
    VkResult result = VK_SUCCESS;
    {
        const UnlockedWaiter waiter{myId};
        mutex.unlock();
        result = WaitTimeline(device, semaphore, waitSemaphores, targetSerial);
        if (profile || trace) waited = std::chrono::steady_clock::now();
    }
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Hook);
    mutex.lock();
    const auto ms = std::chrono::duration<double, std::milli>(waited - start).count();
    if (profile) {
        syncWaitedMs[source] += ms;
        CountThreadSync(source, ms);
        CountSiteWait(ms);
        ++holdCounters.hookUnlockedWaits;
        holdCounters.hookUnlockedWaitMs += ms;
        if (source >= 0 && source < 5) {
            ++holdCounters.hookUnlockedWaitsBySource[source];
            holdCounters.hookUnlockedWaitMsBySource[source] += ms;
        }
        holdCounters.hookRelockMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - waited).count();
        auto& stats = SyncWaits();
        auto& siteStats = stats.bySite[static_cast<std::size_t>(readSite)];
        ++siteStats.count;
        siteStats.waitedMs += ms;
        siteStats.batchesToTarget += batchesToTarget;
        siteStats.unsignaledAtStart += unsignaledAtStart;
        if (targetSerial == newestSerial) ++siteStats.newest;
        if (targetSubmittedAt != std::chrono::steady_clock::time_point{}) siteStats.targetAgeMs += std::chrono::duration<double, std::milli>(waited - targetSubmittedAt).count();
        for (const auto& [queue, count] : aheadByQueue) stats.aheadByQueue[queue] += count;
        if (std::chrono::steady_clock::now() - stats.lastReport > std::chrono::seconds(10)) ReportSyncWaits(stats);
    }
    if (trace) {
        static std::atomic<int> lines{0};
        if (lines.fetch_add(1) < 600) {
            const auto packet = GuestMemory::CurrentPacket();
            std::fprintf(stderr, "[capsync] sync q0x%x %s %s 0x%llx+0x%llx: target %llu (newest %llu), %zu batches up to it (%zu unsignaled at the start), target noted %zu ranges, hit 0x%llx+0x%llx, GPU wait %.1f ms\n", packet.queue, PacketName(packet.opcode).c_str(), GuestMemory::ReadSiteName(readSite), static_cast<unsigned long long>(address), static_cast<unsigned long long>(end - address), static_cast<unsigned long long>(targetSerial), static_cast<unsigned long long>(newestSerial), batchesToTarget, unsignaledAtStart, targetRanges, static_cast<unsigned long long>(hit.first), static_cast<unsigned long long>(hit.second - hit.first), ms);
        }
    }
    if (!RecorderAlive(myId)) {
        ++holdCounters.hookUnlockedTornDown;
        Check(result, "vkWaitSemaphoresKHR recorder");
        return true;
    }
    Check(result, "vkWaitSemaphoresKHR recorder");
    targetedSyncs.fetch_add(1, std::memory_order_relaxed);
    while (!inFlight.empty() && inFlight.front()->serial <= targetSerial) {
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), true, source);
    }
    batchesLeftInFlight.fetch_add(inFlight.size(), std::memory_order_relaxed);
    return true;
}

bool Recorder::Reap() {
    PerformanceTimer frameTiming("Recorder.Reap");
    dropStaleHoldKept();
    const bool profile = DrawProfiled();
    const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    std::uint64_t retired = 0;
    while (!inFlight.empty()) {
        if (fenceStatus(inFlight.front()->fence) != VK_SUCCESS) break;
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), false, 4);
        ++retired;
    }
    if (profile) {
        ++holdCounters.reaps;
        if (retired != 0) ++holdCounters.reapsWithWork;
        holdCounters.reapBatches += retired;
        holdCounters.reapMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    return inFlight.empty();
}

void Recorder::finish(std::unique_ptr<Batch> batch, bool wait, int source, bool releaseNow) {
    inFlightKeptBytes -= batch->keptBytes;
    struct Finishing {
        Recorder& recorder;
        const Batch* batch;
        ~Finishing() {
            auto& list = recorder.finishing;
            list.erase(std::remove(list.begin(), list.end(), batch), list.end());
            if (batch->completionLabelCount != 0) completionLabels.fetch_sub(batch->completionLabelCount, std::memory_order_acq_rel);
            if (batch->completionStoreCount != 0) completionStores.fetch_sub(batch->completionStoreCount, std::memory_order_acq_rel);
            if (batch->writeBackCompletionCount != 0) writeBackCompletions.fetch_sub(batch->writeBackCompletionCount, std::memory_order_acq_rel);
            try {
                recorder.publishPendingWrites();
            } catch (...) {
            }
            try {
                BdaResources::NoteFinished(recorder.context.device, batch->serial);
            } catch (...) {
            }
        }
    } finishingScope{*this, batch.get()};
    finishing.push_back(batch.get());
    if (wait) {
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        static double waitedMs = 0;
        static std::uint64_t waits = 0;
        static auto lastReport = std::chrono::steady_clock::now();
        const auto waitStart = std::chrono::steady_clock::now();
        const bool signaledAtStart = !profile || context.Function<PFN_vkGetFenceStatus>("vkGetFenceStatus")(context.device, batch->fence) == VK_SUCCESS;
        struct Report {
            bool enabled;
            int source;
            std::chrono::steady_clock::time_point start;
            const Recorder& recorder;
            bool signaledAtStart;
            ~Report() {
                if (!enabled) return;
                const auto now = std::chrono::steady_clock::now();
                const auto ms = std::chrono::duration<double, std::milli>(now - start).count();
                waitedMs += ms;
                syncWaitedMs[source] += ms;
                ++waits;
                CountThreadSync(source, ms);
                CountSiteWait(ms);
                if (!signaledAtStart) GuestMemory::NoteLockedGpuWait(ms);
                if (now - lastReport > std::chrono::seconds(10)) {
                    lastReport = now;
                    const auto& h = holdCounters;
                    AgcDriver::ProfilePrint_nid_no_patch("[recorder] %llu syncs waited %.1f s for the GPU in total (sources, count/wait: idle %llu/%.1fs, pending write %llu/%.1fs, recorded store %llu/%.1fs, address-based %llu/%.1fs, other %llu/%.1fs); hook %llu calls, %llu locked; %llu targeted syncs left %llu batches in flight; snapshot %llu rebuilds %.0f ms, %llu notes covered; %llu unlocked timeline waits %.1f s; %llu submissions, %zu label entries, %llu completion labels pending; fence waits by thread (count/wait):%s; top sync sites (source@caller syncs/batches/wait):%s; under holds (cumulative): %llu completions ran %.0f ms (kept objects released %.0f ms), pending-write syncs from completions: %llu skipped, %llu waited %.0f ms; %llu reaps (%llu with work) retired %llu batches in %.0f ms; hook waits unlocked %llu / %.0f ms GPU (pending write %llu / %.0f ms, recorded store %llu / %.0f ms) + %.0f ms relock (%llu found the recorder torn down), locked %llu; deferred releases: on the release thread %llu batches (%llu objects) in %.0f ms, inline %llu batches (%llu objects) in %.0f ms (%llu batches over the queue bound of %zu), queue max %llu batches, %llu pending\n",static_cast<unsigned long long>(waits), waitedMs / 1000, static_cast<unsigned long long>(syncCounts[0]), syncWaitedMs[0] / 1000, static_cast<unsigned long long>(syncCounts[1]), syncWaitedMs[1] / 1000, static_cast<unsigned long long>(syncCounts[2]), syncWaitedMs[2] / 1000, static_cast<unsigned long long>(syncCounts[3]), syncWaitedMs[3] / 1000, static_cast<unsigned long long>(syncCounts[4]), syncWaitedMs[4] / 1000, static_cast<unsigned long long>(hookCalls.load()), static_cast<unsigned long long>(hookLocks.load()), static_cast<unsigned long long>(targetedSyncs.load()), static_cast<unsigned long long>(batchesLeftInFlight.load()), static_cast<unsigned long long>(snapshotRebuilds), snapshotRebuildMs, static_cast<unsigned long long>(snapshotCovered), static_cast<unsigned long long>(unlockedWaits.load()), unlockedWaitedUs.load() / 1e6, static_cast<unsigned long long>(recorder.submissions), recorder.PendingLabels(), static_cast<unsigned long long>(completionLabels.load()), ThreadSyncReport().c_str(), SyncSiteReport().c_str(), static_cast<unsigned long long>(h.completions), h.completionMs, h.keptReleaseMs, static_cast<unsigned long long>(h.completionSyncsSkipped), static_cast<unsigned long long>(h.completionSyncsWaited), h.completionSyncWaitMs, static_cast<unsigned long long>(h.reaps), static_cast<unsigned long long>(h.reapsWithWork), static_cast<unsigned long long>(h.reapBatches), h.reapMs, static_cast<unsigned long long>(h.hookUnlockedWaits), h.hookUnlockedWaitMs, static_cast<unsigned long long>(h.hookUnlockedWaitsBySource[1]), h.hookUnlockedWaitMsBySource[1], static_cast<unsigned long long>(h.hookUnlockedWaitsBySource[2]), h.hookUnlockedWaitMsBySource[2], h.hookRelockMs, static_cast<unsigned long long>(h.hookUnlockedTornDown), static_cast<unsigned long long>(h.hookLockedWaits), static_cast<unsigned long long>(threadReleases.load()), static_cast<unsigned long long>(threadObjects.load()), threadReleaseUs.load() / 1000.0, static_cast<unsigned long long>(inlineReleases.load()), static_cast<unsigned long long>(inlineObjects.load()), inlineReleaseUs.load() / 1000.0, static_cast<unsigned long long>(inlineOverBound.load()), ReleaseQueueBound(), static_cast<unsigned long long>(releaseQueueMax.load()), static_cast<unsigned long long>(deferredPending.load()));
                    AgcDriver::ProfilePrint_nid_no_patch("[recorder] completion label stores: %llu run, %llu skipped (no CPU write-back overlapped them); %llu counted pending at a write-back; %llu write-backs over a tracked label; %llu write-back completions pending\n", static_cast<unsigned long long>(completionStoresRun.load()), static_cast<unsigned long long>(completionStoresSkipped.load()), static_cast<unsigned long long>(completionLabelsCountedLate.load()), static_cast<unsigned long long>(writeBacksOverLabels.load()), static_cast<unsigned long long>(writeBackCompletions.load()));
                    AgcDriver::ProfilePrint_nid_no_patch("[recorder] submits %llu, vkQueueSubmit mean %.1f us, max %.1f us; command buffers freed at batch release %llu, fences kept for reuse %llu\n", static_cast<unsigned long long>(submitCount), submitCount != 0 ? submitUs / static_cast<double>(submitCount) : 0.0, submitMaxUs, static_cast<unsigned long long>(commandBuffersFreed.load()), static_cast<unsigned long long>(fencesKept.load()));
                    const auto reads = Recorder::ReadCounts();
                    AgcDriver::ProfilePrint_nid_no_patch("[recorder] in-place reads: %llu noted, %llu queries, hits by reader: dispatch element %llu, gpu copy %llu, address-based %llu, indirect %llu, storage upload %llu, copy source %llu, draw input %llu, address-based read-only %llu; %llu hits on signaled batches ignored\n", static_cast<unsigned long long>(reads.noted), static_cast<unsigned long long>(reads.queries), static_cast<unsigned long long>(reads.hits[0]), static_cast<unsigned long long>(reads.hits[1]), static_cast<unsigned long long>(reads.hits[2]), static_cast<unsigned long long>(reads.hits[3]), static_cast<unsigned long long>(reads.hits[4]), static_cast<unsigned long long>(reads.hits[5]), static_cast<unsigned long long>(reads.hits[6]), static_cast<unsigned long long>(reads.hits[7]), static_cast<unsigned long long>(reads.staleIgnored));
                }
            }
        } report{profile, source, waitStart, *this, signaledAtStart};
        const auto waitFences = function(waitForFences, "vkWaitForFences");
        auto result = waitFences(context.device, 1, &batch->fence, VK_TRUE, 5'000'000'000ull);
        for (int waited = 5; result == VK_TIMEOUT; waited += 5) {
            std::fprintf(stderr, "[gpu] recorded batch still running on the GPU after %d s\n", waited);
            result = waitFences(context.device, 1, &batch->fence, VK_TRUE, 5'000'000'000ull);
        }
        if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) {
            const auto idle = context.Function<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")(context.device);
            Check(idle, "vkDeviceWaitIdle after recorder fence failure");
        }
        if (result != VK_SUCCESS) {
            release(*batch);
            Check(result, "vkWaitForFences recorder");
        }
    }
    readGpuTiming(*batch);
    readSamples(*batch);
    if (batch->serial != 0) {
        std::lock_guard ringLock(completedMutex);
        auto& entry = completed[batch->serial % completed.size()];
        entry.serial = batch->serial;
        entry.gpuStartNs = batch->gpuStartNs;
        entry.gpuEndNs = batch->gpuEndNs;
        entry.submittedAt = batch->submittedAt;
        entry.fenceSeenAt = std::chrono::steady_clock::now();
        entry.readGeneration = batch->readGeneration;
        entry.reads.clear();
        if (FlipReadCheck()) {
            entry.reads = notedReads(*batch);
        }
    }
    const bool profile = DrawProfiled();
    const auto completionStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    {
        struct InCompletion {
            InCompletion() { HostThreadSlot<int, CompletionDepthSlot>::Set(completionDepth() + 1); }
            ~InCompletion() { HostThreadSlot<int, CompletionDepthSlot>::Set(completionDepth() - 1); }
        } inCompletion;
        for (auto& action : batch->completions) {
            try {
                action();
            } catch (const BdaUnalignedAccess& error) {
                static std::mutex failuresMutex;
                static std::unordered_map<std::uint32_t, std::uint64_t> failures;
                constexpr std::uint64_t kDeferredFailureFirst = 8, kDeferredFailureEvery = 1024;
                const auto count = [&] {
                    std::lock_guard lock(failuresMutex);
                    return ++failures[error.instruction];
                }();
                if (count <= kDeferredFailureFirst) std::fprintf(stderr, "[gpu] deferred write-back failed: %s\n", error.what());
                else if (count % kDeferredFailureEvery == 0) std::fprintf(stderr, "[gpu] deferred write-back failed with an unaligned BDA access at pc 0x%x %llu times so far; latest: %s\n", error.instruction, static_cast<unsigned long long>(count), error.what());
            } catch (const std::exception& error) {
                AgcDriver::LogCoalescedStderr_nid_no_patch("[gpu] deferred write-back failed: %s\n", error.what());
            }
        }
    }
    if (!batch->completions.empty()) KernelWakeSleepers_nid_no_patch();
    if (profile) holdCounters.completions += batch->completions.size();
    const auto keptStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    if (releaseNow || ReleaseUnderLock() || !GuestMemory::GpuMutex().HeldByThisThread()) {
        batch->completions.clear();
        batch->kept.clear();
    } else {
        try {
            DeferredBatches().push_back({std::move(batch->kept), std::move(batch->completions)});
            deferredPending.fetch_add(1, std::memory_order_acq_rel);
            DeferredBytes() += batch->keptBytes;
        } catch (...) {
        }
        batch->completions.clear();
        batch->kept.clear();
    }
    if (profile) {
        const auto now = std::chrono::steady_clock::now();
        holdCounters.keptReleaseMs += std::chrono::duration<double, std::milli>(now - keptStart).count();
        holdCounters.completionMs += std::chrono::duration<double, std::milli>(now - completionStart).count();
    }
    removeLabels(*batch);
    release(*batch);
}

void Recorder::discard(std::unique_ptr<Batch> batch) noexcept {
    removeLabels(*batch);
    if (batch->completionLabelCount != 0) completionLabels.fetch_sub(batch->completionLabelCount, std::memory_order_acq_rel);
    if (batch->completionStoreCount != 0) completionStores.fetch_sub(batch->completionStoreCount, std::memory_order_acq_rel);
    if (batch->writeBackCompletionCount != 0) writeBackCompletions.fetch_sub(batch->writeBackCompletionCount, std::memory_order_acq_rel);
    if (activeRecorder == this) pendingLabelSince.store(NoPendingLabel, std::memory_order_release);
    release(*batch);
    try {
        publishPendingWrites();
    } catch (...) {
    }
}

}
}
}
}
}
}
}
