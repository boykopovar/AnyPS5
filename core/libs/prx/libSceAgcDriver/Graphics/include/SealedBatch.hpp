#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SEALEDBATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SEALEDBATCH_HPP

#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace AgcDriver::Graphics {

struct BatchCancelled : std::runtime_error {
    BatchCancelled() : std::runtime_error("AGC graphics: the batch was cancelled after an earlier batch failed or the device shut down") {}
};

struct RecordingShutDown : std::runtime_error {
    RecordingShutDown() : std::runtime_error("AGC graphics: the recorder shut down while a wait was pending") {}
};

class FailureLatch {
public:
    static constexpr std::uint64_t NoSerial = std::numeric_limits<std::uint64_t>::max();
    bool Fail(std::uint64_t serial, std::exception_ptr error) noexcept;
    bool LoseDevice(std::exception_ptr error) noexcept;
    void Shutdown() noexcept;
    bool Failed() const noexcept { return failedSerial.load(std::memory_order_acquire) != NoSerial; }
    bool DeviceLost() const noexcept { return deviceLost.load(std::memory_order_acquire); }
    bool ShuttingDown() const noexcept { return shuttingDown.load(std::memory_order_acquire); }
    std::uint64_t FailedSerial() const noexcept { return failedSerial.load(std::memory_order_acquire); }
    std::exception_ptr Error() const;
    void ThrowFor(std::uint64_t serial) const;

private:
    std::atomic<std::uint64_t> failedSerial{NoSerial};
    std::atomic<bool> deviceLost{false};
    std::atomic<bool> shuttingDown{false};
    mutable std::mutex mutex;
    std::exception_ptr error;
};

template<typename WaitSlice>
void WaitObservingFailure(const FailureLatch& latch, std::uint64_t serial, std::chrono::nanoseconds slice, WaitSlice&& waitSlice) {
    while (!waitSlice(slice)) latch.ThrowFor(serial);
}

class WorkBudget {
public:
    struct Limits {
        std::size_t encodingBatches = 4;
        std::size_t outstandingBatches = 16;
        std::size_t outstandingBytes = 64u << 20;
    };
    enum class Mode : std::uint8_t { Deferred, Direct };
    explicit WorkBudget(Limits limits) : limits(limits) {}
    Mode Choose() const noexcept;
    void Open(const FailureLatch& latch, std::uint64_t serial, std::chrono::nanoseconds slice = std::chrono::milliseconds(1));
    bool TryOpen() noexcept;
    void Sealed() noexcept;
    void ReplayReleased() noexcept;
    void Charge(std::size_t bytes) noexcept;
    void Retired(std::size_t bytes) noexcept;
    void Uncharge(std::size_t bytes) noexcept;
    std::size_t StreamCap() const noexcept { return limits.outstandingBytes / std::max<std::size_t>(limits.outstandingBatches, 1); }
    bool OverBytes() const noexcept { return bytes.load(std::memory_order_relaxed) >= limits.outstandingBytes; }
    std::size_t Encoding() const noexcept { return encoding.load(std::memory_order_relaxed); }
    std::size_t Outstanding() const noexcept { return outstanding.load(std::memory_order_relaxed); }
    std::size_t OutstandingBytes() const noexcept { return bytes.load(std::memory_order_relaxed); }
    std::uint64_t RoomWaits() const noexcept { return roomWaits.load(std::memory_order_relaxed); }
    std::uint64_t Refusals() const noexcept { return refusals.load(std::memory_order_relaxed); }

private:
    bool room() const noexcept { return outstanding.load(std::memory_order_relaxed) < limits.outstandingBatches && bytes.load(std::memory_order_relaxed) < limits.outstandingBytes; }
    Limits limits;
    std::atomic<std::size_t> encoding{0};
    std::atomic<std::size_t> outstanding{0};
    std::atomic<std::size_t> bytes{0};
    std::atomic<std::uint64_t> roomWaits{0};
    std::atomic<std::uint64_t> refusals{0};
    std::mutex mutex;
    std::condition_variable retired;
};

class SealedBatch {
public:
    enum class Outcome : std::uint8_t { Pending, Encoded, ReplayFailed, Cancelled, Submitted, SubmitFailed, DeviceLost };
    SealedBatch(std::uint64_t serial, CommandStream& stream, const CommandFunctions& real, FailureLatch& latch, WorkBudget* budget = nullptr);
    ~SealedBatch();
    SealedBatch(const SealedBatch&) = delete;
    SealedBatch& operator=(const SealedBatch&) = delete;

    std::uint64_t Serial() const noexcept { return serial; }
    const CommandStream& Stream() const noexcept { return stream; }
    const CommandFunctions& Real() const noexcept { return real; }

    bool Superseded() const noexcept;
    bool Claim() noexcept;
    void Encoded(VkCommandBuffer commands, std::uint32_t worker) noexcept;
    void Failed(std::exception_ptr reason, VkCommandBuffer commands, std::uint32_t worker) noexcept;
    void Cancel() noexcept;

    void AwaitReplayReleased() noexcept;
    VkCommandBuffer Await();
    void Returned(VkResult result) noexcept;
    void NotSubmitted() noexcept;
    void GpuDone() noexcept;
    void DeviceIdleAfterLoss() noexcept;

    bool ReplayReleased() const noexcept { return replay.load(std::memory_order_acquire) == Released; }
    bool SubmitReturned() const noexcept { return submitReturned.load(std::memory_order_acquire); }
    bool GpuReleased() const noexcept { return gpuReleased.load(std::memory_order_acquire); }
    bool Retirable() const noexcept { return ReplayReleased() && SubmitReturned() && GpuReleased(); }
    Outcome Result() const noexcept { return outcome.load(std::memory_order_acquire); }
    std::exception_ptr Error() const;
    VkCommandBuffer Commands() const noexcept { return commands; }
    std::uint32_t Worker() const noexcept { return worker; }

private:
    enum ReplayState : std::uint32_t { Unclaimed, Claimed, Released };
    void releaseReplay(Outcome result, std::exception_ptr reason, VkCommandBuffer buffer, std::uint32_t by) noexcept;
    void maybeRetire() noexcept;
    std::uint64_t serial;
    CommandStream& stream;
    const CommandFunctions& real;
    FailureLatch& latch;
    WorkBudget* budget;
    std::size_t charged = 0;
    std::atomic<std::uint32_t> replay{Unclaimed};
    std::atomic<bool> cancelRequested{false};
    std::atomic<Outcome> outcome{Outcome::Pending};
    std::atomic<bool> submitReturned{false};
    std::atomic<bool> gpuReleased{false};
    std::atomic<bool> retiredCharge{false};
    VkCommandBuffer commands = VK_NULL_HANDLE;
    std::uint32_t worker = 0;
    mutable std::mutex errorMutex;
    std::exception_ptr error;
};

class CommandBufferPool {
public:
    virtual VkCommandBuffer Begin() = 0;
    virtual void End(VkCommandBuffer commands) = 0;
    virtual void Recycle(VkCommandBuffer commands) noexcept = 0;
    virtual ~CommandBufferPool() = default;
};

struct ReplayEntryPoints {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkCreateCommandPool createPool = nullptr;
    PFN_vkDestroyCommandPool destroyPool = nullptr;
    PFN_vkAllocateCommandBuffers allocate = nullptr;
    PFN_vkFreeCommandBuffers free = nullptr;
    PFN_vkBeginCommandBuffer begin = nullptr;
    PFN_vkEndCommandBuffer end = nullptr;
};

std::unique_ptr<CommandBufferPool> BorrowReplayPool(const ReplayEntryPoints& entry, VkCommandPool pool);
std::unique_ptr<CommandBufferPool> CreateReplayPool(const ReplayEntryPoints& entry, std::uint32_t queueFamily);

class ReplayExecutor {
public:
    virtual void Execute(std::shared_ptr<SealedBatch> batch) noexcept = 0;
    virtual void Retire(VkCommandBuffer commands, std::uint32_t worker) noexcept = 0;
    virtual void Shutdown() noexcept = 0;
    virtual ~ReplayExecutor() = default;
};

class InlineExecutor final : public ReplayExecutor {
public:
    explicit InlineExecutor(std::unique_ptr<CommandBufferPool> pool) : pool(std::move(pool)) {}
    void Execute(std::shared_ptr<SealedBatch> batch) noexcept override;
    void Retire(VkCommandBuffer commands, std::uint32_t worker) noexcept override;
    void Shutdown() noexcept override {}

private:
    std::unique_ptr<CommandBufferPool> pool;
};

class SingleExecutor final : public ReplayExecutor {
public:
    using PoolFactory = std::function<std::unique_ptr<CommandBufferPool>()>;
    explicit SingleExecutor(PoolFactory makePool);
    ~SingleExecutor() override;
    void Execute(std::shared_ptr<SealedBatch> batch) noexcept override;
    void Retire(VkCommandBuffer commands, std::uint32_t worker) noexcept override;
    void Shutdown() noexcept override;
    std::uint64_t Replayed() const noexcept { return replayed.load(std::memory_order_relaxed); }

private:
    void run(PoolFactory makePool) noexcept;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::shared_ptr<SealedBatch>> queue;
    std::vector<VkCommandBuffer> recycled;
    bool stopping = false;
    std::atomic<std::uint64_t> replayed{0};
    std::thread thread;
};

void ReplayBatch(SealedBatch& batch, CommandBufferPool& pool, std::uint32_t worker) noexcept;

}

#endif
