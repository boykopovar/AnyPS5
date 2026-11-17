#include "prx/libSceAgcDriver/Graphics/include/CommandList.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SealedBatch.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::BatchCancelled;
using AgcDriver::Graphics::CommandBufferPool;
using AgcDriver::Graphics::CommandStream;
using AgcDriver::Graphics::CommandFunctions;
using AgcDriver::Graphics::FailureLatch;
using AgcDriver::Graphics::InlineExecutor;
using AgcDriver::Graphics::RecordingShutDown;
using AgcDriver::Graphics::SealedBatch;
using AgcDriver::Graphics::SingleExecutor;
using AgcDriver::Graphics::WorkBudget;
using Outcome = SealedBatch::Outcome;

void Require(bool condition, const std::string& message) {
    if (condition) return;
    std::fprintf(stderr, "sealed batch: %s\n", message.c_str());
    std::fflush(stderr);
    throw std::runtime_error(message);
}

template<typename Error, typename Call>
bool Throws(Call&& call) {
    try {
        call();
    } catch (const Error&) {
        return true;
    }
    return false;
}

const CommandFunctions& Real() {
    static const CommandFunctions functions{};
    return functions;
}

struct Seen {
    std::mutex mutex;
    std::vector<std::uint32_t> values;
    std::vector<VkCommandBuffer> targets;
    std::atomic<int> replayThreadCalls{0};
};

Seen& seen() {
    static Seen value;
    return value;
}

struct Value {
    std::uint32_t value;
    std::uint32_t count;
};

void runValue(const std::byte* payload, VkCommandBuffer commands, const CommandFunctions&) {
    Value header;
    std::memcpy(&header, payload, sizeof(header));
    const auto* array = payload + CommandStream::Aligned(sizeof(Value));
    std::lock_guard lock(seen().mutex);
    seen().values.push_back(header.value);
    for (std::uint32_t i = 0; i < header.count; ++i) {
        std::uint32_t element;
        std::memcpy(&element, array + i * sizeof(std::uint32_t), sizeof(element));
        seen().values.push_back(element);
    }
    seen().targets.push_back(commands);
    if (AgcDriver::Graphics::OnReplayThread()) seen().replayThreadCalls.fetch_add(1);
}

void append(CommandStream& stream, std::uint32_t value, std::span<const std::uint32_t> array = {}) {
    stream.Emplace(&runValue, Value{value, static_cast<std::uint32_t>(array.size())}, array);
}

std::vector<std::uint32_t> takeSeen() {
    std::lock_guard lock(seen().mutex);
    auto values = std::move(seen().values);
    seen().values.clear();
    seen().targets.clear();
    return values;
}

VkCommandBuffer fakeBuffer(std::uintptr_t id) {
    return reinterpret_cast<VkCommandBuffer>(id << 4);
}

class FakePool final : public CommandBufferPool {
public:
    struct Shared {
        std::mutex mutex;
        std::vector<std::thread::id> users;
        std::vector<VkCommandBuffer> recycled;
        std::atomic<int> begun{0};
        std::atomic<bool> hold{false};
        std::atomic<bool> holding{false};
        std::atomic<bool> failEnd{false};
    };
    explicit FakePool(std::shared_ptr<Shared> shared) : shared(std::move(shared)) {}
    VkCommandBuffer Begin() override {
        note();
        if (shared->hold.load()) {
            shared->holding.store(true);
            while (shared->hold.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return fakeBuffer(static_cast<std::uintptr_t>(++shared->begun));
    }
    void End(VkCommandBuffer) override {
        note();
        if (shared->failEnd.load()) throw std::runtime_error("vkEndCommandBuffer failed");
    }
    void Recycle(VkCommandBuffer commands) noexcept override {
        note();
        std::lock_guard lock(shared->mutex);
        shared->recycled.push_back(commands);
    }

private:
    void note() {
        std::lock_guard lock(shared->mutex);
        shared->users.push_back(std::this_thread::get_id());
    }
    std::shared_ptr<Shared> shared;
};

void retireUnsubmitted(SealedBatch& batch) {
    batch.NotSubmitted();
    Require(batch.Retirable(), "a batch skipped by its submitter is not retirable");
}

void StreamsReplayInOrderAcrossChunks() {
    CommandStream stream(Real());
    Require(CommandStream::IsHandle(stream.Handle()) && &CommandStream::From(stream.Handle()) == &stream, "the stream handle does not round-trip to its stream");
    Require(!CommandStream::IsHandle(fakeBuffer(7)), "a real command buffer reads as a stream handle");
    std::vector<std::uint32_t> large(40000);
    for (std::size_t i = 0; i < large.size(); ++i) large[i] = static_cast<std::uint32_t>(i);
    const std::array<std::uint32_t, 3> small{7, 8, 9};
    append(stream, 1, small);
    append(stream, 2, large);
    append(stream, 3);
    for (std::uint32_t i = 0; i < 5000; ++i) append(stream, 100 + i);
    Require(stream.Ops() == 5003, "the stream counted " + std::to_string(stream.Ops()) + " ops");
    Require(stream.Charged() > CommandStream::ChunkBytes, "a record larger than a chunk did not get its own chunk");
    Require(Throws<std::logic_error>([&] { AgcDriver::Graphics::Replay(stream, fakeBuffer(1), Real()); }), "an unsealed stream was replayed");
    stream.Seal();
    Require(Throws<std::logic_error>([&] { AgcDriver::Graphics::Replay(stream, stream.Handle(), Real()); }), "a stream was replayed into a stream handle");
    takeSeen();
    AgcDriver::Graphics::Replay(stream, fakeBuffer(1), Real());
    const auto values = takeSeen();
    Require(values.size() == 4 + 1 + large.size() + 1 + 5000, "replay produced " + std::to_string(values.size()) + " values");
    Require(values[0] == 1 && values[1] == 7 && values[3] == 9 && values[4] == 2 && values[5] == 0 && values[4 + large.size()] == 39999 && values[5 + large.size()] == 3 && values.back() == 5099, "replay did not keep the records and their arrays in order");
    Require(seen().replayThreadCalls.exchange(0) == 5003, "replay did not mark its thread as a replay thread");
    Require(!AgcDriver::Graphics::OnReplayThread(), "the replay thread mark outlived the replay");
}

void SealedAndFailedAppendsPoison() {
    CommandStream stream(Real());
    append(stream, 1);
    stream.Seal();
    Require(Throws<std::logic_error>([&] { append(stream, 2); }), "an append after Seal did not throw");
    Require(stream.Poisoned(), "an append after Seal did not poison the stream even though the caller caught the error");
    Require(Throws<std::logic_error>([&] { AgcDriver::Graphics::Replay(stream, fakeBuffer(1), Real()); }), "a poisoned stream replayed");
    CommandStream poisoned(Real());
    append(poisoned, 1);
    poisoned.Poison(std::make_exception_ptr(std::invalid_argument("unknown pNext")));
    poisoned.Poison(std::make_exception_ptr(std::runtime_error("second reason")));
    Require(Throws<std::invalid_argument>([&] { append(poisoned, 2); }), "an append to a poisoned stream did not rethrow the first reason");
}

void InlineReplayFailureReachesTheLatch() {
    FailureLatch latch;
    auto shared = std::make_shared<FakePool::Shared>();
    InlineExecutor executor(std::make_unique<FakePool>(shared));
    CommandStream good(Real());
    append(good, 11);
    auto first = std::make_shared<SealedBatch>(1, good, Real(), latch);
    executor.Execute(first);
    Require(first->Result() == Outcome::Encoded && first->Await() != VK_NULL_HANDLE, "the inline executor did not encode a good batch");
    first->Returned(VK_SUCCESS);
    Require(!first->Retirable(), "a submitted batch was retirable before its fence");
    first->GpuDone();
    Require(first->Retirable(), "a submitted batch whose fence signalled is not retirable");
    executor.Retire(first->Commands(), first->Worker());
    CommandStream bad(Real());
    append(bad, 12);
    bad.Poison(std::make_exception_ptr(std::invalid_argument("unknown pNext")));
    auto second = std::make_shared<SealedBatch>(2, bad, Real(), latch);
    executor.Execute(second);
    Require(second->Result() == Outcome::ReplayFailed, "a poisoned batch was not reported failed");
    Require(Throws<std::invalid_argument>([&] { second->Await(); }), "Await did not rethrow the replay error");
    Require(latch.FailedSerial() == 2, "the replay failure did not reach the latch");
    retireUnsubmitted(*second);
    CommandStream later(Real());
    append(later, 13);
    auto third = std::make_shared<SealedBatch>(3, later, Real(), latch);
    executor.Execute(third);
    Require(third->Result() == Outcome::Cancelled && Throws<BatchCancelled>([&] { third->Await(); }), "a batch after a failed one was not cancelled");
    retireUnsubmitted(*third);
    shared->failEnd.store(true);
    FailureLatch fresh;
    CommandStream ending(Real());
    append(ending, 14);
    auto fourth = std::make_shared<SealedBatch>(1, ending, Real(), fresh);
    executor.Execute(fourth);
    Require(fourth->Result() == Outcome::ReplayFailed && fourth->Commands() == VK_NULL_HANDLE && shared->recycled.size() == 3, "a buffer whose end failed was not recycled before the failure was published");
    retireUnsubmitted(*fourth);
}

void CancelDoesNotImpersonateTheWorker() {
    FailureLatch latch;
    auto shared = std::make_shared<FakePool::Shared>();
    shared->hold.store(true);
    SingleExecutor executor([shared] { return std::make_unique<FakePool>(shared); });
    CommandStream stream(Real());
    append(stream, 21);
    auto batch = std::make_shared<SealedBatch>(1, stream, Real(), latch);
    executor.Execute(batch);
    while (!shared->holding.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    batch->Cancel();
    Require(!batch->ReplayReleased(), "a cancel released the replay while the worker still held the batch");
    Require(batch->Result() == Outcome::Pending, "a cancel published an outcome while the worker still held the batch");
    shared->hold.store(false);
    Require(Throws<BatchCancelled>([&] { batch->Await(); }), "a batch cancelled while replaying did not end cancelled");
    Require(batch->ReplayReleased() && batch->Commands() != VK_NULL_HANDLE, "a batch cancelled during replay lost the buffer the worker began");
    retireUnsubmitted(*batch);
    executor.Retire(batch->Commands(), batch->Worker());
    CommandStream unclaimed(Real());
    append(unclaimed, 22);
    auto early = std::make_shared<SealedBatch>(2, unclaimed, Real(), latch);
    early->Cancel();
    Require(early->ReplayReleased() && early->Result() == Outcome::Cancelled, "a batch cancelled before any worker claimed it was not released at once");
    Require(!early->Claim(), "a worker claimed a batch that was already cancelled");
    retireUnsubmitted(*early);
}

void FailureReachesSerialWaiters() {
    FailureLatch latch;
    std::atomic<bool> earlierDone{false};
    auto later = std::async(std::launch::async, [&] {
        try {
            AgcDriver::Graphics::WaitObservingFailure(latch, 5, std::chrono::milliseconds(1), [](std::chrono::nanoseconds slice) {
                std::this_thread::sleep_for(slice);
                return false;
            });
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    });
    auto earlier = std::async(std::launch::async, [&] {
        AgcDriver::Graphics::WaitObservingFailure(latch, 2, std::chrono::milliseconds(1), [&](std::chrono::nanoseconds slice) {
            std::this_thread::sleep_for(slice);
            return earlierDone.load();
        });
        return true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    latch.Fail(3, std::make_exception_ptr(std::invalid_argument("replay of batch 3 failed")));
    Require(Throws<std::invalid_argument>([&] { latch.ThrowFor(3); }), "a waiter on the failed serial itself did not see its failure");
    Require(later.wait_for(std::chrono::seconds(5)) == std::future_status::ready && later.get(), "a waiter on a serial after the failed batch did not see a failure that arrived after it started waiting");
    Require(earlier.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout, "a waiter on a serial before the failed batch gave up on work that can still complete");
    earlierDone.store(true);
    Require(earlier.wait_for(std::chrono::seconds(5)) == std::future_status::ready && earlier.get(), "a waiter on an earlier serial did not finish when its work completed");
    Require(!latch.Fail(4, nullptr) && latch.FailedSerial() == 3 && latch.Fail(1, std::make_exception_ptr(std::runtime_error("earlier"))) && latch.FailedSerial() == 1, "the latch did not keep the lowest failed serial");
    FailureLatch closing;
    auto parked = std::async(std::launch::async, [&] {
        return Throws<RecordingShutDown>([&] {
            AgcDriver::Graphics::WaitObservingFailure(closing, 1, std::chrono::milliseconds(1), [](std::chrono::nanoseconds slice) {
                std::this_thread::sleep_for(slice);
                return false;
            });
        });
    });
    closing.Shutdown();
    Require(parked.wait_for(std::chrono::seconds(5)) == std::future_status::ready && parked.get(), "shutdown did not release a serial waiter");
}

void DeviceLossHoldsResourcesUntilTheDeviceIsIdle() {
    FailureLatch latch;
    auto shared = std::make_shared<FakePool::Shared>();
    InlineExecutor executor(std::make_unique<FakePool>(shared));
    CommandStream lostStream(Real());
    append(lostStream, 31);
    auto lost = std::make_shared<SealedBatch>(1, lostStream, Real(), latch);
    executor.Execute(lost);
    static_cast<void>(lost->Await());
    lost->Returned(VK_ERROR_DEVICE_LOST);
    Require(lost->Result() == Outcome::DeviceLost && lost->SubmitReturned() && !lost->GpuReleased() && !lost->Retirable(), "a batch whose submit lost the device was retirable before the device was idled");
    lost->GpuDone();
    Require(!lost->Retirable(), "a fence observation released a batch after device loss");
    Require(latch.DeviceLost() && Throws<std::runtime_error>([&] { latch.ThrowFor(0); }), "device loss did not fail every serial, including earlier ones");
    lost->DeviceIdleAfterLoss();
    Require(lost->Retirable(), "the lost-device cleanup did not release the batch");
    FailureLatch other;
    CommandStream failedStream(Real());
    append(failedStream, 32);
    auto failed = std::make_shared<SealedBatch>(4, failedStream, Real(), other);
    executor.Execute(failed);
    static_cast<void>(failed->Await());
    failed->Returned(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    Require(failed->Result() == Outcome::SubmitFailed && failed->Retirable() && other.FailedSerial() == 4 && !other.DeviceLost(), "an ordinary failed submit was not retirable at once or did not latch its serial");
    CommandStream earlyStream(Real());
    append(earlyStream, 33);
    auto early = std::make_shared<SealedBatch>(5, earlyStream, Real(), other);
    executor.Execute(early);
    Require(early->Result() == Outcome::Cancelled, "a batch after a failed submit was replayed");
    retireUnsubmitted(*early);
}

void FenceBeforeReturnStaysHeld() {
    FailureLatch latch;
    auto shared = std::make_shared<FakePool::Shared>();
    InlineExecutor executor(std::make_unique<FakePool>(shared));
    CommandStream stream(Real());
    append(stream, 41);
    auto batch = std::make_shared<SealedBatch>(1, stream, Real(), latch);
    executor.Execute(batch);
    static_cast<void>(batch->Await());
    batch->GpuDone();
    Require(!batch->Retirable(), "a batch whose fence signalled before vkQueueSubmit returned was retirable");
    batch->Returned(VK_SUCCESS);
    Require(batch->Retirable(), "a batch did not retire once its submit returned after the fence");
}

void BudgetBoundsWorkThroughRetirement() {
    WorkBudget budget(WorkBudget::Limits{2, 3, 1u << 30});
    FailureLatch latch;
    auto shared = std::make_shared<FakePool::Shared>();
    shared->hold.store(true);
    SingleExecutor executor([shared] { return std::make_unique<FakePool>(shared); });
    std::vector<std::unique_ptr<CommandStream>> streams;
    std::vector<std::shared_ptr<SealedBatch>> batches;
    for (std::uint64_t serial = 1; serial <= 3; ++serial) {
        Require(budget.Choose() == (serial <= 2 ? WorkBudget::Mode::Deferred : WorkBudget::Mode::Direct), "batch " + std::to_string(serial) + " chose the wrong mode with " + std::to_string(budget.Encoding()) + " batches encoding");
        budget.Open(latch, serial);
        streams.push_back(std::make_unique<CommandStream>(Real(), &budget));
        append(*streams.back(), static_cast<std::uint32_t>(serial));
        if (serial <= 2) {
            batches.push_back(std::make_shared<SealedBatch>(serial, *streams.back(), Real(), latch, &budget));
            executor.Execute(batches.back());
        }
    }
    Require(budget.Outstanding() == 3 && budget.OutstandingBytes() == 3 * CommandStream::ChunkBytes, "the budget did not count every open batch and its stream bytes");
    auto blocked = std::async(std::launch::async, [&] {
        budget.Open(latch, 4);
        return true;
    });
    Require(blocked.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout, "a fourth batch opened past the outstanding bound");
    shared->hold.store(false);
    static_cast<void>(batches[0]->Await());
    static_cast<void>(batches[1]->Await());
    Require(budget.Encoding() == 0 && budget.Choose() == WorkBudget::Mode::Deferred, "encoding admission did not reopen once the replays were released");
    streams[0]->ReleaseMemory();
    Require(blocked.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout && budget.OutstandingBytes() == 3 * CommandStream::ChunkBytes, "a batch's charge was released when its replay finished instead of when it retired");
    batches[0]->Returned(VK_SUCCESS);
    batches[0]->GpuDone();
    Require(blocked.wait_for(std::chrono::seconds(5)) == std::future_status::ready && blocked.get(), "a retirement did not admit the blocked batch");
    Require(budget.RoomWaits() == 1, "the room wait was not counted");
    batches[0]->GpuDone();
    batches[0]->Returned(VK_SUCCESS);
    Require(budget.Outstanding() == 3 && budget.OutstandingBytes() == 2 * CommandStream::ChunkBytes, "a batch released its budget charge twice");
    auto failing = std::async(std::launch::async, [&] { return Throws<std::invalid_argument>([&] { budget.Open(latch, 5); }); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    latch.Fail(2, std::make_exception_ptr(std::invalid_argument("batch 2 failed")));
    Require(failing.wait_for(std::chrono::seconds(5)) == std::future_status::ready && failing.get(), "a batch waiting for room did not see a later failure");
    retireUnsubmitted(*batches[1]);
    executor.Retire(batches[0]->Commands(), 0);
    executor.Retire(batches[1]->Commands(), 0);
    budget.Retired(streams[2]->Charged());
    budget.Retired(0);
}

void SingleExecutorOwnsItsPoolAndThread() {
    FailureLatch latch;
    auto shared = std::make_shared<FakePool::Shared>();
    std::thread::id worker;
    std::vector<std::unique_ptr<CommandStream>> streams;
    std::vector<std::shared_ptr<SealedBatch>> batches;
    {
        SingleExecutor executor([shared, &worker] {
            worker = std::this_thread::get_id();
            return std::make_unique<FakePool>(shared);
        });
        takeSeen();
        seen().replayThreadCalls.store(0);
        for (std::uint64_t serial = 1; serial <= 8; ++serial) {
            streams.push_back(std::make_unique<CommandStream>(Real()));
            append(*streams.back(), static_cast<std::uint32_t>(serial));
            batches.push_back(std::make_shared<SealedBatch>(serial, *streams.back(), Real(), latch));
            executor.Execute(batches.back());
        }
        for (auto& batch : batches) static_cast<void>(batch->Await());
        const auto values = takeSeen();
        Require(values == std::vector<std::uint32_t>{1, 2, 3, 4, 5, 6, 7, 8}, "the single executor did not replay batches in order");
        Require(seen().replayThreadCalls.exchange(0) == 8, "the single executor's replays were not marked as replay threads");
        for (auto& batch : batches) {
            batch->Returned(VK_SUCCESS);
            batch->GpuDone();
            executor.Retire(batch->Commands(), batch->Worker());
        }
        executor.Shutdown();
        std::lock_guard lock(shared->mutex);
        Require(shared->recycled.size() == 8, "retired buffers were not recycled before shutdown returned");
        for (const auto& id : shared->users) Require(id == worker, "a pool was used off its worker thread");
        CommandStream afterStream(Real());
        append(afterStream, 9);
        auto after = std::make_shared<SealedBatch>(9, afterStream, Real(), latch);
        executor.Execute(after);
        Require(after->Result() == Outcome::Cancelled, "a batch handed to a stopped executor was not cancelled");
        retireUnsubmitted(*after);
    }
    FailureLatch broken;
    CommandStream stream(Real());
    append(stream, 10);
    auto batch = std::make_shared<SealedBatch>(1, stream, Real(), broken);
    {
        SingleExecutor executor([]() -> std::unique_ptr<CommandBufferPool> { throw std::runtime_error("vkCreateCommandPool failed"); });
        executor.Execute(batch);
        Require(Throws<std::runtime_error>([&] { batch->Await(); }) && batch->Result() == Outcome::ReplayFailed && broken.FailedSerial() == 1, "a worker without a pool did not fail its batches");
    }
    retireUnsubmitted(*batch);
}

void ShutdownCancelsQueuedBatches() {
    FailureLatch latch;
    auto shared = std::make_shared<FakePool::Shared>();
    shared->hold.store(true);
    std::vector<std::unique_ptr<CommandStream>> streams;
    std::vector<std::shared_ptr<SealedBatch>> batches;
    {
        SingleExecutor executor([shared] { return std::make_unique<FakePool>(shared); });
        for (std::uint64_t serial = 1; serial <= 3; ++serial) {
            streams.push_back(std::make_unique<CommandStream>(Real()));
            append(*streams.back(), static_cast<std::uint32_t>(serial));
            batches.push_back(std::make_shared<SealedBatch>(serial, *streams.back(), Real(), latch));
            executor.Execute(batches.back());
            while (!shared->holding.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::thread release([shared] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            shared->hold.store(false);
        });
        executor.Shutdown();
        release.join();
    }
    Require(batches[0]->ReplayReleased() && batches[1]->Result() == Outcome::Cancelled && batches[2]->Result() == Outcome::Cancelled, "shutdown left queued batches unreleased");
    for (auto& batch : batches) retireUnsubmitted(*batch);
}

int destroyUnretired() {
    FailureLatch latch;
    CommandStream stream(Real());
    append(stream, 1);
    auto batch = std::make_unique<SealedBatch>(1, stream, Real(), latch);
    batch->Cancel();
    batch.reset();
    return 0;
}

int appendOnReplayThread() {
    CommandStream stream(Real());
    const AgcDriver::Graphics::ReplayThreadScope scope;
    append(stream, 1);
    return 0;
}

void DiesWith(const std::string& self, const std::string& mode, const std::string& expected) {
    const auto output = std::filesystem::temp_directory_path() / ("aps5-sealed-batch" + mode + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    std::string command = "\"" + self + "\" " + mode + " 2> \"" + output.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    const int status = std::system(command.c_str());
    std::stringstream text;
    text << std::ifstream(output).rdbuf();
    std::error_code error;
    std::filesystem::remove(output, error);
    Require(status != 0, "the " + mode + " child did not end abnormally");
    Require(text.str().find(expected) != std::string::npos, "the " + mode + " child did not print \"" + expected + "\": " + text.str());
}

}

void AdmissionOpensDirectInsteadOfWaiting() {
    WorkBudget budget(WorkBudget::Limits{2, 3, 1u << 30});
    FailureLatch latch;
    auto shared = std::make_shared<FakePool::Shared>();
    shared->hold.store(true);
    SingleExecutor executor([shared] { return std::make_unique<FakePool>(shared); });
    std::vector<std::unique_ptr<CommandStream>> streams;
    std::vector<std::shared_ptr<SealedBatch>> batches;
    for (std::uint64_t serial = 1; serial <= 2; ++serial) {
        Require(budget.TryOpen(), "batch " + std::to_string(serial) + " was refused under the encoding bound");
        streams.push_back(std::make_unique<CommandStream>(Real(), &budget));
        append(*streams.back(), static_cast<std::uint32_t>(serial));
        batches.push_back(std::make_shared<SealedBatch>(serial, *streams.back(), Real(), latch, &budget));
        executor.Execute(batches.back());
    }
    const auto start = std::chrono::steady_clock::now();
    Require(!budget.TryOpen(), "a batch was admitted while two sealed batches waited for the replay worker");
    Require(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(50), "a refused admission waited");
    Require(budget.Refusals() == 1 && budget.Outstanding() == 2 && budget.RoomWaits() == 0, "a refused admission was not counted, or took a slot");
    shared->hold.store(false);
    static_cast<void>(batches[0]->Await());
    static_cast<void>(batches[1]->Await());
    Require(budget.TryOpen(), "admission did not reopen once the replays were released");
    Require(!budget.TryOpen(), "a batch was admitted past the outstanding bound");
    Require(budget.Refusals() == 2, "the outstanding refusal was not counted");
    for (auto& batch : batches) {
        batch->Returned(VK_SUCCESS);
        batch->GpuDone();
        executor.Retire(batch->Commands(), 0);
    }
    Require(budget.Outstanding() == 1 && budget.TryOpen(), "retired batches did not give their slots back");
    budget.Retired(0);
    budget.Retired(0);
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    if (argc == 2 && std::string_view(argv[1]) == "--destroy-unretired") return destroyUnretired();
    if (argc == 2 && std::string_view(argv[1]) == "--append-on-replay") return appendOnReplayThread();
    try {
        StreamsReplayInOrderAcrossChunks();
        SealedAndFailedAppendsPoison();
        InlineReplayFailureReachesTheLatch();
        CancelDoesNotImpersonateTheWorker();
        FailureReachesSerialWaiters();
        DeviceLossHoldsResourcesUntilTheDeviceIsIdle();
        FenceBeforeReturnStaysHeld();
        BudgetBoundsWorkThroughRetirement();
        AdmissionOpensDirectInsteadOfWaiting();
        SingleExecutorOwnsItsPoolAndThread();
        ShutdownCancelsQueuedBatches();
        DiesWith(argv[0], "--destroy-unretired", "destroyed before its submission returned");
        DiesWith(argv[0], "--append-on-replay", "appended to a stream from a replay thread");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "sealed batch: %s\n", error.what());
        return 1;
    }
    std::puts("sealed batch: ok");
    return 0;
}
