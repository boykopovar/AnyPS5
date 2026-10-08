#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/WaitMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/WorkerSampler.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

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
                notify = driver.idleWaiters != 0 || driver.orderHolders.load(std::memory_order_acquire) != 0;
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
    {
        char role[32];
        std::snprintf(role, sizeof role, "queue worker 0x%x", id);
        PinWorkerThread(role);
    }

    GuestMemory::TagGpuLockThread(id);
    if (id == 0) StartWorkerSampler();
    Submission submission;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& costs = submissionCosts(id);
    try {
        for (;;) {
            submission = Submission{};
            static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
            {
                std::unique_lock lock(mutex);
                auto& worker = workers.at(id);
                auto& pending = worker.pending;
                workerQueued() = &worker.queued;
                if (traceGpu && pending.empty()) std::fprintf(stderr, "[gpu] %.1f idle queue=0x%x\n", TraceMs(), id);
                const auto ready = [&] { return failure || stopping || !pending.empty(); };
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
                        if (id == 0) queue0Dormant.store(true, std::memory_order_relaxed);
                        changed.wait(lock, ready);
                        if (id == 0) queue0Dormant.store(false, std::memory_order_relaxed);
                        break;
                    }
                    if (poll(ready)) break;
                    lock.unlock();
                    reapCompletionLabels();
                    lock.lock();
                }
                rethrowFailure();
                if (stopping || shutdownToken.stop_requested() || pending.empty()) {
                    break;
                }
                submission = std::move(pending.front());
                pending.pop_front();
                if (id == 0) queue0Executing = submission.suspend ? 0 : submission.received;
                if (submission.waitFree) {
                    orderHolders.fetch_add(1, std::memory_order_acq_rel);
                    const auto released = [&] { return failure || stopping || orderReleased(id, submission.received); };
                    while (!released()) poll(released);
                    orderHolders.fetch_sub(1, std::memory_order_acq_rel);
                    rethrowFailure();
                }
                worker.queued.fetch_sub(1, std::memory_order_acq_rel);
                if (profile && submission.enqueuedAt != std::chrono::steady_clock::time_point{}) costs.dequeueNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - submission.enqueuedAt).count());
            }
            {
                struct Running {
                    std::atomic<std::uint32_t>& count;
                    explicit Running(std::atomic<std::uint32_t>& count) : count(count) { count.fetch_add(1, std::memory_order_acq_rel); }
                    ~Running() { count.fetch_sub(1, std::memory_order_acq_rel); }
                } running{runningWorkers};
                execute(submission);
            }
            if (traceGpu) std::fprintf(stderr, "[gpu] %.1f done serial=%llu queue=0x%x\n", TraceMs(), static_cast<unsigned long long>(submission.serial), id);
            const auto completeStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
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
        ReportFailure(error);
        for (const auto& [offset, flip] : submission.flips) flip->Fail(error);
    }
}

}
