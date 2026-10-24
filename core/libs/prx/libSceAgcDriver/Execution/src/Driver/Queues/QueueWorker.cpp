#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionRing.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/WaitMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/WorkerSampler.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace AgcDriver::DriverDetail {

namespace {

constexpr auto IdleResultsQuietPeriod = std::chrono::milliseconds(50);

}

FrameTiming* Driver::frameTiming() {
    if (!APS5_ENABLE_TIMING_LOG) return nullptr;
    if (pendingFrameTiming == nullptr) pendingFrameTiming = std::make_shared<FrameTiming>(++frameSerial, true);
    return pendingFrameTiming.get();
}

FrameTiming* Driver::includeTimingSubmission(const Submission& submission, bool firstSegment) {
    if (!APS5_ENABLE_TIMING_LOG) return nullptr;
    auto* frame = submission.queue == 0 ? frameTiming() : FrameTiming::Async();
    require(submission.receivedAt != FrameTiming::Clock::time_point{} && submission.receivedAt <= submission.enqueuedAt && submission.enqueuedAt <= submission.dequeuedAt && submission.dequeuedAt <= submission.orderedAt, "invalid submission timing order");
    if (submission.queue == 0) frame->IncludeSubmission(submission.serial, submission.receivedAt, submission.enqueuedAt, submission.orderedAt, firstSegment);
    else if (firstSegment) {
        frame->Add(frame->Get("Submission", "accept"), submission.enqueuedAt - submission.receivedAt);
        frame->Add(frame->Get("Submission", "queue"), submission.orderedAt - submission.enqueuedAt);
    }
    if (firstSegment) {
        frame->Add(frame->Get("Submission", "dequeue"), submission.dequeuedAt - submission.enqueuedAt);
        frame->Add(frame->Get("Submission", "order_wait"), submission.orderedAt - submission.dequeuedAt);
        if (!submission.suspend) {
            frame->Add(frame->Get("Submission", "copy"), submission.copiedAt - submission.receivedAt, submission.commands.size() * sizeof(std::uint32_t));
            frame->Add(frame->Get("Submission", "validate"), submission.validatedAt - submission.copiedAt);
            frame->Add(frame->Get("Submission", "flip_room_wait"), submission.roomReadyAt - submission.validatedAt);
            frame->Add(frame->Get("Submission", "reserve_enqueue"), submission.enqueuedAt - submission.roomReadyAt);
        }
    }
    return frame;
}

void Driver::markCompleted(std::uint64_t serial) {
    completedOutOfOrder.insert(serial);
    while (!completedOutOfOrder.empty() && *completedOutOfOrder.begin() == completed + 1) {
        completed = *completedOutOfOrder.begin();
        completedOutOfOrder.erase(completedOutOfOrder.begin());
    }
}

bool& Driver::completionDeferred() {
    static thread_local bool deferred = false;
    return deferred;
}

void Driver::deferCompletion(const Submission& submission) {
    std::uint64_t received = submission.received;
    {
        std::lock_guard lock(mutex);
        if (queue0Executing != 0) received = std::min(received, queue0Executing);
        queue0Uncommitted.push_back(received);
    }
    completionDeferred() = true;
    DrawPipeline::Queue0().Enqueue([this, serial = submission.serial, received, labelWrites = submission.labelWrites] {
        completeSubmission(serial, received, labelWrites);
    }, {});
}

void Driver::submitOpenWork() {
    if (deferredLabels().labels.empty() && !Graphics::Recorder::PendingLabelSince().has_value() && Graphics::Recorder::RecordedWorkSinceSubmit() == 0) return;
    ++submissionCosts(0).endSubmits;
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::End);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    const auto localDevice = device.Load();
    recordDeferredLabels(localDevice.get(), 0);
    if (localDevice != nullptr) localDevice->SubmitRecorded(true);
}

void Driver::wakeQueueZeroForCompletions() {
    if (!completionsPending()) return;
    std::lock_guard lock(mutex);
    changed.notify_all();
}

void Driver::completeSubmission(std::uint64_t serial, std::uint64_t received, std::span<const std::uint64_t> labelWrites) {
    struct Complete {
        Driver& driver;
        std::uint64_t serial, received;
        std::span<const std::uint64_t> labelWrites;
        ~Complete() {
            bool notify = true;
            {
                std::lock_guard lock(driver.mutex);
                driver.markCompleted(serial);
                driver.forgetUnfinishedWrites(driver.workers.at(0), labelWrites);
                const auto found = std::find(driver.queue0Uncommitted.begin(), driver.queue0Uncommitted.end(), received);
                if (found != driver.queue0Uncommitted.end()) driver.queue0Uncommitted.erase(found);
                notify = driver.idleWaiters != 0 || driver.orderHolders.load(std::memory_order_acquire) != 0 || completionsPending();
            }
            if (notify) driver.changed.notify_all();
        }
    } complete{*this, serial, received, labelWrites};
    submitOpenWork();
}

const std::atomic<std::uint64_t>*& Driver::workerQueued() {
    static thread_local const std::atomic<std::uint64_t>* queued = nullptr;
    return queued;
}

void Driver::reapCompletionLabels() {
    std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::try_to_lock);
    if (!gpuLock.owns_lock()) {
        ++triesFailed[TryIdle];
        return;
    }

    if (!completionsPending()) return;

    bumpEpoch(&EpochBumps::reaps);
    if (const auto localDevice = device.Load()) localDevice->ReapRecorded();
}

void Driver::run(std::uint32_t id) noexcept {
    onWorkerThread() = true;
    char role[32];
    std::snprintf(role, sizeof role, "queue worker 0x%x", id);
    PinWorkerThread(role);

    GuestMemory::TagGpuLockThread(id);
    if (id == 0) StartWorkerSampler();
    Submission submission;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& costs = submissionCosts(id);
    try {
        if (PerformanceTrace::Configured()) PerformanceTrace::BindThread(role);
        for (;;) {
            submission = Submission{};
            struct Running {
                std::atomic<std::uint32_t>& count;
                std::atomic<std::uint64_t>& idleEpoch;
                std::condition_variable& changed;
                bool active = false;
                Running(std::atomic<std::uint32_t>& count, std::atomic<std::uint64_t>& idleEpoch, std::condition_variable& changed)
                    : count(count), idleEpoch(idleEpoch), changed(changed) {}
                void Start() {
                    count.fetch_add(1, std::memory_order_acq_rel);
                    active = true;
                }
                ~Running() {
                    if (active && count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        idleEpoch.fetch_add(1, std::memory_order_release);
                        changed.notify_all();
                    }
                }
            } running{runningWorkers, idleEpoch, changed};
            static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
            {
                PerformanceContext timingContext(id == 0 ? frameTiming() : FrameTiming::Async());
                PerformanceTimer timing("Driver.WorkerWait");
                std::unique_lock lock(mutex);
                timing.Mark("mutex_wait");
                auto& worker = workers.at(id);
                auto& pending = worker.pending;
                workerQueued() = &worker.queued;
                if (traceGpu && pending.empty()) std::fprintf(stderr, "[gpu] %.1f idle queue=0x%x\n", TraceMs(), id);
                const auto ready = [&] { return failure || stopping || !pending.empty(); };
                const auto allQueuesIdle = [&] {
                    return runningWorkers.load(std::memory_order_acquire) == 0
                        && std::all_of(workers.begin(), workers.end(), [](const auto& item) { return item.second.pending.empty(); });
                };
                const auto idleResultsPending = [&] {
                    return id == 0 && Graphics::StorageTexture::AnyPending() && allQueuesIdle() && !completionsPending();
                };
                const auto poll = [&](const auto& done) {
#ifdef _WIN32
                    lock.unlock();
                    PollSleep();
                    lock.lock();
                    return done();
#else
                    return changed.wait_for(lock, std::chrono::milliseconds(1), done);
#endif
                };

                while (!ready()) {
                    if (!completionsPending() || (id != 0 && Graphics::Recorder::PendingCompletionLabels() == 0)) {
                        const auto idleEpochAtCheck = idleEpoch.load(std::memory_order_acquire);
                        const bool storeIdleResults = idleResultsPending();
                        if (storeIdleResults) {
                            queue0Dormant.store(true, std::memory_order_relaxed);
                            const auto quietUntil = std::chrono::steady_clock::now() + IdleResultsQuietPeriod;
                            const auto idleEpochAtWait = idleEpoch.load(std::memory_order_acquire);
                            changed.wait_until(lock, quietUntil, [&] {
                                return ready() || idleEpoch.load(std::memory_order_acquire) != idleEpochAtWait;
                            });
                            queue0Dormant.store(false, std::memory_order_relaxed);
                            if (ready()) break;
                            if (idleEpoch.load(std::memory_order_acquire) != idleEpochAtWait) {
                                continue;
                            }
                            if (!idleResultsPending()) continue;
                        }
                        lock.unlock();
                        if (storeIdleResults) {
                            if (const auto localDevice = device.Load()) localDevice->StoreIdleResults();
                        }
                        lock.lock();
                        if (ready()) break;
                        if (id == 0 && allQueuesIdle() && !completionsPending()
                            && ((!storeIdleResults && Graphics::StorageTexture::AnyPending()) || idleEpoch.load(std::memory_order_acquire) != idleEpochAtCheck)) continue;
                        if (id == 0) queue0Dormant.store(true, std::memory_order_relaxed);
                        const auto idleEpochAtWait = idleEpoch.load(std::memory_order_acquire);
                        changed.wait(lock, [&] { return ready() || (id == 0 && (idleEpoch.load(std::memory_order_acquire) != idleEpochAtWait || completionsPending())); });
                        if (id == 0) queue0Dormant.store(false, std::memory_order_relaxed);
                        if (id == 0 && !ready() && (idleEpoch.load(std::memory_order_acquire) != idleEpochAtWait || completionsPending())) continue;
                        break;
                    }
                    if (poll(ready)) break;
                    lock.unlock();
                    reapCompletionLabels();
                    lock.lock();
                }
                timing.Mark("idle_and_completion_poll");
                rethrowFailure();
                if (stopping || shutdownToken.stop_requested() || pending.empty()) {
                    break;
                }
                submission = std::move(pending.front());
                pending.pop_front();
                if (APS5_ENABLE_TIMING_LOG) submission.dequeuedAt = std::chrono::steady_clock::now();
                if (id == 0) queue0Executing = submission.suspend ? 0 : submission.received;
                if (submission.waitFree) {
                    {
                        struct OrderHold {
                            std::atomic<std::uint32_t>& holders;
                            explicit OrderHold(std::atomic<std::uint32_t>& holders) : holders(holders) { holders.fetch_add(1, std::memory_order_acq_rel); }
                            ~OrderHold() { holders.fetch_sub(1, std::memory_order_acq_rel); }
                            OrderHold(const OrderHold&) = delete;
                            OrderHold& operator=(const OrderHold&) = delete;
                        } hold{orderHolders};
                        const auto released = [&] { return failure || stopping || orderReleased(id, submission.received); };
                        StallWatch stall(id);
                        while (!released() && !poll(released)) stall.Check("order behind queue 0 for event", submission.received);
                    }
                    rethrowFailure();
                }
                if (APS5_ENABLE_TIMING_LOG) submission.orderedAt = std::chrono::steady_clock::now();
                timing.Mark("dequeue_order");
                worker.queued.fetch_sub(1, std::memory_order_acq_rel);
                running.Start();
                if (profile && submission.enqueuedAt != std::chrono::steady_clock::time_point{}) costs.dequeueNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - submission.enqueuedAt).count());
            }
            execute(submission);
            if (traceGpu) std::fprintf(stderr, "[gpu] %.1f done serial=%llu queue=0x%x\n", TraceMs(), static_cast<unsigned long long>(submission.serial), id);
            const auto completeStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            PerformanceContext completionContext(id == 0 ? frameTiming() : FrameTiming::Async());
            PerformanceTimer completionTiming("Driver.Completion");
            bool notify = true;
            {
                std::lock_guard lock(mutex);
                rethrowFailure();
                if (!std::exchange(completionDeferred(), false)) {
                    markCompleted(submission.serial);
                    forgetUnfinishedWrites(workers.at(id), submission);
                }
                if (id == 0) queue0Executing = 0;

                notify = idleWaiters != 0 || orderHolders.load(std::memory_order_acquire) != 0;
            }
            if (notify) changed.notify_all();
            else ++costs.notifiesSkipped;
            if (profile) costs.completeNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - completeStart).count());
        }
    } catch (const ProcessShutdown&) {
        submission = Submission{};
    } catch (...) {
        const auto error = std::current_exception();
        for (const auto& [offset, flip] : submission.flips) flip->Fail(error);
        ReportFailure(error);
    }
}

}
