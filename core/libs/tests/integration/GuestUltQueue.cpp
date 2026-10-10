#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" {
int APS5_VABI sceUltInitialize();
int APS5_VABI sceUltFinalize();
std::uint64_t APS5_VABI sceUltQueueDataResourcePoolGetWorkAreaSize(std::uint32_t, std::uint64_t, std::uint32_t);
int APS5_VABI sceUltQueueDataResourcePoolCreate(void*, const char*, std::uint32_t, std::uint64_t, std::uint32_t, void*, void*, const void*, std::uint32_t);
int APS5_VABI sceUltQueueDataResourcePoolDestroy(void*);
int APS5_VABI sceUltQueueCreate(void*, const char*, std::uint64_t, void*, void*, const void*, std::uint32_t);
int APS5_VABI sceUltQueueDestroy(void*);
int APS5_VABI sceUltQueuePush(void*, const void*);
int APS5_VABI sceUltQueueTryPush(void*, const void*);
int APS5_VABI sceUltQueuePop(void*, void*);
int APS5_VABI sceUltQueueTryPop(void*, void*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int Ok = 0;
constexpr int Null = -2139029503;
constexpr int Alignment = -2139029502;
constexpr int Range = -2139029501;
constexpr int Invalid = -2139029500;
constexpr int State = -2139029498;
constexpr int Busy = -2139029497;
constexpr int Again = -2139029496;

class UltSession {
public:
    UltSession() {
        RequireEqual(sceUltInitialize(), Ok, "initialize the ULT library");
    }

    ~UltSession() {
        sceUltFinalize();
    }

    UltSession(const UltSession&) = delete;
    UltSession& operator=(const UltSession&) = delete;
};

struct alignas(8) Object {
    std::array<std::uint8_t, 512> bytes{};
};

struct Queues {
    Object pool;
    Object first;
    Object second;

    void Create(std::uint32_t slots, std::uint32_t queues = 1, std::uint64_t size = sizeof(std::uint64_t)) {
        RequireEqual(sceUltQueueDataResourcePoolCreate(&pool, "pool", slots, size, queues, nullptr, nullptr, nullptr, 0), Ok, "create the data pool");
        RequireEqual(sceUltQueueCreate(&first, "first", size, nullptr, &pool, nullptr, 0), Ok, "create the first queue");
        if (queues > 1) RequireEqual(sceUltQueueCreate(&second, "second", size, nullptr, &pool, nullptr, 0), Ok, "create the second queue");
    }

    void Destroy(bool secondQueue = false) {
        RequireEqual(sceUltQueueDestroy(&first), Ok, "destroy the first queue");
        if (secondQueue) RequireEqual(sceUltQueueDestroy(&second), Ok, "destroy the second queue");
        RequireEqual(sceUltQueueDataResourcePoolDestroy(&pool), Ok, "destroy the data pool");
    }
};

void RequireFinalized() {
    RequireEqual(sceUltFinalize(), Ok, "finalize the ULT library");
}

void WaitUntilBlocked(std::future<int>& result, const char* message) {
    Require(result.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout, message);
}

const Case poolCreateInvalidArguments{"QueueDataResourcePoolCreate_InvalidArguments_ReturnErrors", [] {
    Queues queues;
    Object extra;
    const UltSession session;
    RequireEqual(sceUltQueueDataResourcePoolCreate(nullptr, nullptr, 1, 8, 1, nullptr, nullptr, nullptr, 0), Null, "null pool");
    RequireEqual(sceUltQueueDataResourcePoolCreate(queues.pool.bytes.data() + 1, nullptr, 1, 8, 1, nullptr, nullptr, nullptr, 0), Alignment, "misaligned pool");
    RequireEqual(sceUltQueueDataResourcePoolCreate(&queues.pool, nullptr, 0, 8, 1, nullptr, nullptr, nullptr, 0), Range, "zero slots");
    RequireEqual(sceUltQueueDataResourcePoolCreate(&queues.pool, nullptr, 1, 0, 1, nullptr, nullptr, nullptr, 0), Range, "zero data size");
    RequireEqual(sceUltQueueDataResourcePoolCreate(&queues.pool, nullptr, 1, 8, 0, nullptr, nullptr, nullptr, 0), Range, "zero queue objects");
    RequireEqual(sceUltQueueDataResourcePoolCreate(&queues.pool, nullptr, 2, UINT64_MAX, 1, nullptr, nullptr, nullptr, 0), Range, "data area overflow");
    RequireEqual(sceUltQueueDataResourcePoolCreate(&queues.pool, nullptr, 1, 8, 1, &extra, nullptr, nullptr, 0), Invalid, "unknown waiting queue pool");
    RequireFinalized();
}};

const Case queueCreateUnknownPool{"QueueCreate_UnknownDataPool_ReturnsInvalid", [] {
    Queues queues;
    const UltSession session;
    RequireEqual(sceUltQueueCreate(&queues.first, nullptr, 8, nullptr, &queues.pool, nullptr, 0), Invalid, "queue on an unregistered pool");
    RequireFinalized();
}};

const Case createRegisteredObjects{"QueueCreate_AlreadyRegisteredObjects_ReturnStateAndKeepQueuedData", [] {
    Queues queues;
    std::uint64_t value = 99;
    const UltSession session;
    queues.Create(2);
    RequireEqual(sceUltQueuePush(&queues.first, &value), Ok, "push before re-creating");
    RequireEqual(sceUltQueueDataResourcePoolCreate(&queues.pool, nullptr, 1, 8, 1, nullptr, nullptr, nullptr, 0), State, "re-create a live pool");
    RequireEqual(sceUltQueueCreate(&queues.first, nullptr, 8, nullptr, &queues.pool, nullptr, 0), State, "re-create a live queue");
    std::uint64_t saved = 0;
    RequireEqual(sceUltQueueTryPop(&queues.first, &saved), Ok, "pop after the rejected re-creation");
    RequireEqual(saved, value, "queued value survives the rejected re-creation");
    queues.Destroy();
    RequireFinalized();
}};

const Case queueCreateInvalidArguments{"QueueCreate_InvalidArgumentsOnLivePool_ReturnErrors", [] {
    Queues queues;
    Object extra;
    const UltSession session;
    queues.Create(2);
    RequireEqual(sceUltQueueCreate(&extra, nullptr, 9, nullptr, &queues.pool, nullptr, 0), Invalid, "data size larger than the pool slot");
    RequireEqual(sceUltQueueCreate(&extra, nullptr, 8, &extra, &queues.pool, nullptr, 0), Invalid, "waiting queue pool differs from the data pool");
    RequireEqual(sceUltQueueCreate(&extra, nullptr, 8, nullptr, &queues.pool, nullptr, 0), Again, "pool has no free queue object");
    RequireEqual(sceUltQueueCreate(nullptr, nullptr, 8, nullptr, &queues.pool, nullptr, 0), Null, "null queue");
    RequireEqual(sceUltQueueCreate(extra.bytes.data() + 1, nullptr, 8, nullptr, &queues.pool, nullptr, 0), Alignment, "misaligned queue");
    RequireEqual(sceUltQueueCreate(&extra, nullptr, 0, nullptr, &queues.pool, nullptr, 0), Range, "zero data size");
    queues.Destroy();
    RequireFinalized();
}};

const Case tryPopEmpty{"QueueTryPop_EmptyQueue_ReturnsAgainAndKeepsOutput", [] {
    Queues queues;
    std::uint64_t value = 99;
    const UltSession session;
    queues.Create(2);
    RequireEqual(sceUltQueueTryPop(&queues.first, &value), Again, "pop from an empty queue");
    RequireEqual(value, std::uint64_t{99}, "output untouched by the failed pop");
    queues.Destroy();
    RequireFinalized();
}};

const Case transferInvalidArguments{"QueueTransfer_NullOrUnregisteredArguments_ReturnNullOrState", [] {
    Queues queues;
    Object extra;
    std::uint64_t value = 99;
    const UltSession session;
    queues.Create(2);
    RequireEqual(sceUltQueueTryPush(&queues.first, nullptr), Null, "try-push null data");
    RequireEqual(sceUltQueuePop(&queues.first, nullptr), Null, "pop into null data");
    RequireEqual(sceUltQueuePush(nullptr, &value), Null, "push to a null queue");
    RequireEqual(sceUltQueuePop(&extra, &value), State, "pop from an unregistered queue");
    queues.Destroy();
    RequireFinalized();
}};

const Case destroyInvalid{"QueueDestroy_PoolInUseOrNullObjects_ReturnBusyOrNull", [] {
    Queues queues;
    const UltSession session;
    queues.Create(2);
    RequireEqual(sceUltQueueDataResourcePoolDestroy(&queues.pool), Busy, "destroy a pool with a live queue");
    RequireEqual(sceUltQueueDataResourcePoolDestroy(nullptr), Null, "destroy a null pool");
    RequireEqual(sceUltQueueDestroy(nullptr), Null, "destroy a null queue");
    queues.Destroy();
    RequireFinalized();
}};

const Case workAreaSize{"QueueDataResourcePoolGetWorkAreaSize_ValidArguments_ReturnsAlignedSize", [] {
    RequireEqual(sceUltQueueDataResourcePoolGetWorkAreaSize(3, 9, 2), std::uint64_t{1072}, "3 slots of 9 bytes and 2 queues");
}};

const Case workAreaOverflow{"QueueDataResourcePoolGetWorkAreaSize_Overflow_ThrowsOutOfRange", [] {
    RequireThrows<std::out_of_range>([] { sceUltQueueDataResourcePoolGetWorkAreaSize(1, UINT64_MAX, 1); }, "data size overflow");
    RequireThrows<std::out_of_range>([] { sceUltQueueDataResourcePoolGetWorkAreaSize(UINT32_MAX, UINT64_MAX / 2, 1); }, "work area overflow");
}};

const Case optParamRejected{"QueueCreate_OptParam_ThrowsNotImplemented", [] {
    Queues queues;
    Object extra;
    const UltSession session;
    queues.Create(2);
    RequireThrows<std::runtime_error>([&] { sceUltQueueCreate(&extra, nullptr, 8, nullptr, &queues.pool, &extra, 0); }, "queue optParam");
    RequireThrows<std::runtime_error>([&] { sceUltQueueDataResourcePoolCreate(&extra, nullptr, 1, 8, 1, nullptr, nullptr, &extra, 0); }, "data pool optParam");
    queues.Destroy();
    RequireFinalized();
}};

const Case destroyedObjects{"Queue_DestroyedObjects_ReturnState", [] {
    Queues queues;
    std::uint64_t value = 99;
    const UltSession session;
    queues.Create(2);
    queues.Destroy();
    RequireEqual(sceUltQueueTryPush(&queues.first, &value), State, "push to a destroyed queue");
    RequireEqual(sceUltQueueDestroy(&queues.first), State, "destroy a destroyed queue");
    RequireEqual(sceUltQueueDataResourcePoolDestroy(&queues.pool), State, "destroy a destroyed pool");
    RequireFinalized();
}};

const Case recreateAfterDestroy{"Queue_RecreateAfterDestroy_Succeeds", [] {
    Queues queues;
    const UltSession session;
    queues.Create(2);
    queues.Destroy();
    queues.Create(1);
    queues.Destroy();
    RequireFinalized();
}};

const Case fifo{"Queue_RepeatedFillAndDrain_PreservesFifoOrderAndBounds", [] {
    Queues queues;
    const UltSession session;
    queues.Create(3, 1, 13);
    std::array<std::array<std::uint8_t, 13>, 4> items{};
    for (std::size_t index = 0; index < items.size(); ++index) {
        for (std::size_t byte = 0; byte < items[index].size(); ++byte) items[index][byte] = index * 31 + byte;
    }
    for (int round = 0; round < 100; ++round) {
        const std::string where = "round " + std::to_string(round);
        for (int index = 0; index < 3; ++index) {
            RequireEqual(sceUltQueueTryPush(&queues.first, items[index].data()), Ok, where + ": push item " + std::to_string(index));
        }
        RequireEqual(sceUltQueueTryPush(&queues.first, items[3].data()), Again, where + ": push into a full queue");
        std::array<std::uint8_t, 15> output;
        for (int index = 0; index < 3; ++index) {
            const std::string item = where + ": item " + std::to_string(index);
            output.fill(0xAA);
            RequireEqual(sceUltQueuePop(&queues.first, output.data() + 1), Ok, item + " pop");
            RequireEqual(output.front(), std::uint8_t{0xAA}, item + " leading guard byte");
            RequireEqual(output.back(), std::uint8_t{0xAA}, item + " trailing guard byte");
            Require(std::memcmp(output.data() + 1, items[index].data(), 13) == 0, item + " payload matches the pushed bytes");
        }
        RequireEqual(sceUltQueueTryPop(&queues.first, output.data()), Again, where + ": pop from a drained queue");
    }
    queues.Destroy();
    RequireFinalized();
}};

const Case blockingPush{"QueuePush_FullQueue_BlocksUntilPopped", [] {
    Queues queues;
    std::uint64_t first = 11;
    std::uint64_t second = 22;
    std::uint64_t output = 0;
    std::future<int> producer;
    const UltSession session;
    queues.Create(1);
    RequireEqual(sceUltQueuePush(&queues.first, &first), Ok, "fill the queue");
    producer = std::async(std::launch::async, [&] { return sceUltQueuePush(&queues.first, &second); });
    WaitUntilBlocked(producer, "producer blocks on a full queue");
    RequireEqual(sceUltQueueDestroy(&queues.first), Busy, "destroy a queue with a blocked producer");
    RequireEqual(sceUltQueuePop(&queues.first, &output), Ok, "pop the first value");
    RequireEqual(output, first, "first value");
    RequireEqual(producer.get(), Ok, "blocked producer completes");
    RequireEqual(sceUltQueuePop(&queues.first, &output), Ok, "pop the second value");
    RequireEqual(output, second, "second value");
    queues.Destroy();
    RequireFinalized();
}};

const Case blockingPop{"QueuePop_EmptyQueue_BlocksUntilPushed", [] {
    Queues queues;
    std::uint64_t first = 11;
    std::uint64_t output = 0;
    std::future<int> consumer;
    const UltSession session;
    queues.Create(1);
    consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&queues.first, &output); });
    WaitUntilBlocked(consumer, "consumer blocks on an empty queue");
    RequireEqual(sceUltQueueDestroy(&queues.first), Busy, "destroy a queue with a blocked consumer");
    RequireEqual(sceUltQueuePush(&queues.first, &first), Ok, "push a value");
    RequireEqual(consumer.get(), Ok, "blocked consumer completes");
    RequireEqual(output, first, "consumer received the pushed value");
    queues.Destroy();
    RequireFinalized();
}};

const Case sharedPool{"Queue_SharedPool_QueuesCompeteForSlotsAndReleaseThemOnDestroy", [] {
    Queues queues;
    std::uint64_t first = 111;
    std::uint64_t second = 222;
    std::uint64_t output = 0;
    std::future<int> consumer;
    std::future<int> producer;
    const UltSession session;
    queues.Create(1, 2);
    consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&queues.second, &output); });
    WaitUntilBlocked(consumer, "consumer blocks on the empty second queue");
    RequireEqual(sceUltQueuePush(&queues.first, &first), Ok, "push to the first queue takes the only slot");
    WaitUntilBlocked(consumer, "consumer stays blocked when the other queue receives data");
    RequireEqual(sceUltQueueTryPush(&queues.second, &second), Again, "try-push without a free slot");
    RequireEqual(sceUltQueueTryPop(&queues.second, &output), Again, "try-pop from the empty second queue");
    producer = std::async(std::launch::async, [&] { return sceUltQueuePush(&queues.second, &second); });
    WaitUntilBlocked(producer, "producer blocks without a free slot");
    RequireEqual(sceUltQueueDestroy(&queues.second), Busy, "destroy the second queue with waiters");
    RequireEqual(sceUltQueueDestroy(&queues.first), Ok, "destroy the first queue releases its slot");
    RequireEqual(producer.get(), Ok, "blocked producer completes");
    RequireEqual(consumer.get(), Ok, "blocked consumer completes");
    RequireEqual(output, second, "consumer received the second value");
    RequireEqual(sceUltQueueCreate(&queues.first, "replacement", 4, nullptr, &queues.pool, nullptr, 0), Ok, "create a smaller replacement queue");
    const std::uint32_t small = 42;
    RequireEqual(sceUltQueuePush(&queues.first, &small), Ok, "push to the replacement queue");
    std::uint32_t smallOutput = 0;
    RequireEqual(sceUltQueuePop(&queues.first, &smallOutput), Ok, "pop from the replacement queue");
    RequireEqual(smallOutput, small, "replacement queue value");
    queues.Destroy(true);
    RequireFinalized();
}};

const Case finalizeWakesWaiters{"Finalize_BlockedQueueWaiters_ReturnStateAndLibraryReinitializes", [] {
    Queues queues;
    std::uint64_t value = 7;
    std::uint64_t output = 123;
    std::future<int> producer;
    std::future<int> consumer;
    const UltSession session;
    queues.Create(1, 2);
    RequireEqual(sceUltQueuePush(&queues.first, &value), Ok, "fill the only slot");
    producer = std::async(std::launch::async, [&] { return sceUltQueuePush(&queues.first, &value); });
    consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&queues.second, &output); });
    WaitUntilBlocked(producer, "producer blocks without a free slot");
    WaitUntilBlocked(consumer, "consumer blocks on the empty second queue");
    RequireEqual(sceUltQueueDestroy(&queues.first), Busy, "destroy the first queue with a waiter");
    RequireEqual(sceUltQueueDestroy(&queues.second), Busy, "destroy the second queue with a waiter");
    RequireEqual(sceUltFinalize(), Ok, "finalize with blocked waiters");
    RequireEqual(producer.get(), State, "producer woken by finalize");
    RequireEqual(consumer.get(), State, "consumer woken by finalize");
    RequireEqual(output, std::uint64_t{123}, "consumer output untouched");
    RequireEqual(sceUltQueueTryPop(&queues.first, &output), State, "queue unregistered by finalize");
    RequireEqual(sceUltInitialize(), Ok, "re-initialize");
    queues.Create(1, 2);
    RequireEqual(sceUltQueueTryPop(&queues.first, &output), Again, "re-created queue starts empty");
    RequireEqual(sceUltQueuePush(&queues.second, &value), Ok, "push after re-initialization");
    RequireEqual(sceUltQueuePop(&queues.second, &output), Ok, "pop after re-initialization");
    RequireEqual(output, value, "value after re-initialization");
    queues.Destroy(true);
    RequireFinalized();
}};

const Case stress{"Queue_ConcurrentProducersAndConsumers_DeliverEveryValueOnce", [] {
    constexpr std::size_t workers = 4;
    constexpr std::size_t perWorker = 2000;
    Queues queues;
    std::array<std::atomic<unsigned>, workers * perWorker> seen{};
    std::atomic<unsigned> pushFailures{0};
    std::atomic<unsigned> popFailures{0};
    std::atomic<unsigned> outOfRange{0};
    const UltSession session;
    queues.Create(7);
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    for (std::size_t worker = 0; worker < workers; ++worker) {
        consumers.emplace_back([&] {
            for (std::size_t index = 0; index < perWorker; ++index) {
                std::uint64_t value = UINT64_MAX;
                if (sceUltQueuePop(&queues.first, &value) != Ok) ++popFailures;
                if (value < seen.size()) ++seen[value];
                else ++outOfRange;
            }
        });
        producers.emplace_back([&, worker] {
            for (std::size_t index = 0; index < perWorker; ++index) {
                const std::uint64_t value = worker * perWorker + index;
                if (sceUltQueuePush(&queues.first, &value) != Ok) ++pushFailures;
            }
        });
    }
    for (auto& producer : producers) producer.join();
    if (pushFailures.load() != 0) sceUltFinalize();
    for (auto& consumer : consumers) consumer.join();
    RequireEqual(pushFailures.load(), 0u, "failed pushes");
    RequireEqual(popFailures.load(), 0u, "failed pops");
    RequireEqual(outOfRange.load(), 0u, "popped values outside the pushed range");
    for (std::size_t value = 0; value < seen.size(); ++value) {
        if (seen[value].load() != 1) RequireEqual(seen[value].load(), 1u, "times value " + std::to_string(value) + " was popped");
    }
    std::uint64_t output = 0;
    RequireEqual(sceUltQueueTryPop(&queues.first, &output), Again, "queue drained");
    queues.Destroy();
    RequireFinalized();
}};

} // namespace
