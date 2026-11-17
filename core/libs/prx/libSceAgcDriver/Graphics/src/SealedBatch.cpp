#include "prx/libSceAgcDriver/Graphics/include/SealedBatch.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

[[noreturn]] void refuse(const char* message) {
    std::fprintf(stderr, "AGC graphics: %s\n", message);
    std::fflush(stderr);
    std::abort();
}

std::exception_ptr submitError(VkResult result) {
    return std::make_exception_ptr(std::runtime_error("AGC graphics: vkQueueSubmit failed with VkResult " + std::to_string(static_cast<int>(result))));
}

}

bool FailureLatch::Fail(std::uint64_t serial, std::exception_ptr reason) noexcept {
    std::lock_guard lock(mutex);
    const auto current = failedSerial.load(std::memory_order_relaxed);
    if (serial >= current) return false;
    if (current == NoSerial || error == nullptr) error = reason;
    failedSerial.store(serial, std::memory_order_release);
    return true;
}

bool FailureLatch::LoseDevice(std::exception_ptr reason) noexcept {
    std::lock_guard lock(mutex);
    if (error == nullptr) error = reason;
    return !deviceLost.exchange(true, std::memory_order_acq_rel);
}

void FailureLatch::Shutdown() noexcept {
    shuttingDown.store(true, std::memory_order_release);
}

std::exception_ptr FailureLatch::Error() const {
    std::lock_guard lock(mutex);
    return error;
}

void FailureLatch::ThrowFor(std::uint64_t serial) const {
    if (DeviceLost() || FailedSerial() <= serial) {
        if (const auto reason = Error(); reason != nullptr) std::rethrow_exception(reason);
        throw BatchCancelled();
    }
    if (ShuttingDown()) throw RecordingShutDown();
}

WorkBudget::Mode WorkBudget::Choose() const noexcept {
    return encoding.load(std::memory_order_relaxed) >= limits.encodingBatches ? Mode::Direct : Mode::Deferred;
}

void WorkBudget::Open(const FailureLatch& latch, std::uint64_t serial, std::chrono::nanoseconds slice) {
    std::unique_lock lock(mutex);
    if (!room()) {
        roomWaits.fetch_add(1, std::memory_order_relaxed);
        while (!retired.wait_for(lock, slice, [&] { return room(); })) latch.ThrowFor(serial);
    }
    outstanding.fetch_add(1, std::memory_order_relaxed);
}

void WorkBudget::Sealed() noexcept {
    encoding.fetch_add(1, std::memory_order_relaxed);
}

void WorkBudget::ReplayReleased() noexcept {
    encoding.fetch_sub(1, std::memory_order_relaxed);
}

void WorkBudget::Charge(std::size_t charge) noexcept {
    bytes.fetch_add(charge, std::memory_order_relaxed);
}

void WorkBudget::Retired(std::size_t charge) noexcept {
    {
        std::lock_guard lock(mutex);
        bytes.fetch_sub(charge, std::memory_order_relaxed);
        outstanding.fetch_sub(1, std::memory_order_relaxed);
    }
    retired.notify_all();
}

SealedBatch::SealedBatch(std::uint64_t serial, CommandStream& stream, const DeviceFunctions& real, FailureLatch& latch, WorkBudget* budget) : serial(serial), stream(stream), real(real), latch(latch), budget(budget) {
    stream.Seal();
    if (budget != nullptr) budget->Sealed();
}

SealedBatch::~SealedBatch() {
    if (!ReplayReleased()) refuse("a sealed batch was destroyed while a replay could still read its stream");
    if (!SubmitReturned()) refuse("a sealed batch was destroyed before its submission returned or was skipped");
    if (!GpuReleased()) refuse("a sealed batch was destroyed before the GPU released it");
}

bool SealedBatch::Superseded() const noexcept {
    return latch.DeviceLost() || latch.ShuttingDown() || latch.FailedSerial() < serial;
}

bool SealedBatch::Claim() noexcept {
    auto expected = static_cast<std::uint32_t>(Unclaimed);
    return replay.compare_exchange_strong(expected, Claimed, std::memory_order_acq_rel);
}

void SealedBatch::Encoded(VkCommandBuffer buffer, std::uint32_t by) noexcept {
    if (replay.load(std::memory_order_relaxed) != Claimed) refuse("a batch was reported encoded by a worker that had not claimed it");
    if (cancelRequested.load(std::memory_order_acquire)) releaseReplay(Outcome::Cancelled, std::make_exception_ptr(BatchCancelled()), buffer, by);
    else releaseReplay(Outcome::Encoded, nullptr, buffer, by);
}

void SealedBatch::Failed(std::exception_ptr reason, VkCommandBuffer buffer, std::uint32_t by) noexcept {
    if (replay.load(std::memory_order_relaxed) != Claimed) refuse("a batch was reported failed by a worker that had not claimed it");
    latch.Fail(serial, reason);
    releaseReplay(Outcome::ReplayFailed, reason, buffer, by);
}

void SealedBatch::Cancel() noexcept {
    cancelRequested.store(true, std::memory_order_release);
    if (Claim()) releaseReplay(Outcome::Cancelled, std::make_exception_ptr(BatchCancelled()), VK_NULL_HANDLE, 0);
}

void SealedBatch::releaseReplay(Outcome result, std::exception_ptr reason, VkCommandBuffer buffer, std::uint32_t by) noexcept {
    commands = buffer;
    worker = by;
    {
        std::lock_guard lock(errorMutex);
        error = reason;
    }
    if (budget != nullptr) budget->ReplayReleased();
    outcome.store(result, std::memory_order_release);
    replay.store(Released, std::memory_order_release);
    replay.notify_all();
    maybeRetire();
}

VkCommandBuffer SealedBatch::Await() {
    for (auto state = replay.load(std::memory_order_acquire); state != Released; state = replay.load(std::memory_order_acquire)) replay.wait(state, std::memory_order_acquire);
    if (outcome.load(std::memory_order_acquire) == Outcome::Encoded) return commands;
    if (const auto reason = Error(); reason != nullptr) std::rethrow_exception(reason);
    throw BatchCancelled();
}

void SealedBatch::Returned(VkResult result) noexcept {
    if (!ReplayReleased()) refuse("a batch's submission returned before its replay released it");
    if (result == VK_SUCCESS) {
        outcome.store(Outcome::Submitted, std::memory_order_release);
    } else if (result == VK_ERROR_DEVICE_LOST) {
        const auto reason = submitError(result);
        {
            std::lock_guard lock(errorMutex);
            error = reason;
        }
        latch.LoseDevice(reason);
        outcome.store(Outcome::DeviceLost, std::memory_order_release);
    } else {
        const auto reason = submitError(result);
        {
            std::lock_guard lock(errorMutex);
            error = reason;
        }
        latch.Fail(serial, reason);
        outcome.store(Outcome::SubmitFailed, std::memory_order_release);
        gpuReleased.store(true, std::memory_order_release);
    }
    submitReturned.store(true, std::memory_order_release);
    maybeRetire();
}

void SealedBatch::NotSubmitted() noexcept {
    if (!ReplayReleased()) refuse("a batch was skipped by its submitter before its replay released it");
    if (outcome.load(std::memory_order_acquire) == Outcome::Encoded) outcome.store(Outcome::Cancelled, std::memory_order_release);
    gpuReleased.store(true, std::memory_order_release);
    submitReturned.store(true, std::memory_order_release);
    maybeRetire();
}

void SealedBatch::GpuDone() noexcept {
    if (outcome.load(std::memory_order_acquire) == Outcome::DeviceLost) return;
    gpuReleased.store(true, std::memory_order_release);
    maybeRetire();
}

void SealedBatch::DeviceIdleAfterLoss() noexcept {
    if (!latch.DeviceLost()) refuse("a batch was released as idle after device loss while the device was not lost");
    gpuReleased.store(true, std::memory_order_release);
    maybeRetire();
}

void SealedBatch::maybeRetire() noexcept {
    if (!Retirable() || retiredCharge.exchange(true, std::memory_order_acq_rel)) return;
    if (budget != nullptr) budget->Retired(stream.Charged());
}

std::exception_ptr SealedBatch::Error() const {
    std::lock_guard lock(errorMutex);
    return error;
}

void ReplayBatch(SealedBatch& batch, CommandBufferPool& pool, std::uint32_t worker) noexcept {
    if (batch.Superseded()) {
        batch.Cancel();
        return;
    }
    if (!batch.Claim()) return;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    try {
        commands = pool.Begin();
        Replay(batch.Stream(), commands, batch.Real());
        pool.End(commands);
        batch.Encoded(commands, worker);
    } catch (...) {
        if (commands != VK_NULL_HANDLE) pool.Recycle(commands);
        batch.Failed(std::current_exception(), VK_NULL_HANDLE, worker);
    }
}

void InlineExecutor::Execute(std::shared_ptr<SealedBatch> batch) noexcept {
    ReplayBatch(*batch, *pool, 0);
}

void InlineExecutor::Retire(VkCommandBuffer commands, std::uint32_t) noexcept {
    if (commands != VK_NULL_HANDLE) pool->Recycle(commands);
}

SingleExecutor::SingleExecutor(PoolFactory makePool) : thread([this, makePool = std::move(makePool)]() mutable { run(std::move(makePool)); }) {}

SingleExecutor::~SingleExecutor() {
    Shutdown();
}

void SingleExecutor::Execute(std::shared_ptr<SealedBatch> batch) noexcept {
    {
        std::lock_guard lock(mutex);
        if (!stopping) {
            queue.push_back(std::move(batch));
            batch = nullptr;
        }
    }
    if (batch != nullptr) {
        batch->Cancel();
        return;
    }
    wake.notify_one();
}

void SingleExecutor::Retire(VkCommandBuffer commands, std::uint32_t) noexcept {
    if (commands == VK_NULL_HANDLE) return;
    {
        std::lock_guard lock(mutex);
        recycled.push_back(commands);
    }
    wake.notify_one();
}

void SingleExecutor::Shutdown() noexcept {
    {
        std::lock_guard lock(mutex);
        stopping = true;
    }
    wake.notify_one();
    if (thread.joinable() && thread.get_id() != std::this_thread::get_id()) thread.join();
}

void SingleExecutor::run(PoolFactory makePool) noexcept {
    std::unique_ptr<CommandBufferPool> pool;
    std::exception_ptr poolError;
    try {
        pool = makePool();
        if (pool == nullptr) throw std::runtime_error("AGC graphics: the replay worker's command pool factory returned no pool");
    } catch (...) {
        poolError = std::current_exception();
    }
    std::deque<std::shared_ptr<SealedBatch>> work;
    std::vector<VkCommandBuffer> returned;
    for (;;) {
        bool stop = false;
        {
            std::unique_lock lock(mutex);
            wake.wait(lock, [&] { return stopping || !queue.empty() || !recycled.empty(); });
            work.swap(queue);
            returned.swap(recycled);
            stop = stopping;
        }
        for (const auto commands : returned) {
            if (pool != nullptr) pool->Recycle(commands);
        }
        returned.clear();
        for (auto& batch : work) {
            if (stop) {
                batch->Cancel();
            } else if (pool == nullptr) {
                if (batch->Claim()) batch->Failed(poolError, VK_NULL_HANDLE, 0);
            } else {
                ReplayBatch(*batch, *pool, 0);
                replayed.fetch_add(1, std::memory_order_relaxed);
            }
            batch.reset();
        }
        work.clear();
        if (stop) break;
    }
    pool.reset();
}

}
