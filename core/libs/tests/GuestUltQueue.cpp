#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <stdexcept>
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

static constexpr int Null = -2139029503;
static constexpr int Alignment = -2139029502;
static constexpr int Range = -2139029501;
static constexpr int Invalid = -2139029500;
static constexpr int State = -2139029498;
static constexpr int Busy = -2139029497;
static constexpr int Again = -2139029496;

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ULT queue check failed at line %d\n", line);
        std::abort();
    }
}

#define CHECK(value) Require((value), __LINE__)

struct alignas(8) Object { std::array<std::uint8_t, 512> bytes{}; };

struct Fixture {
    Object pool;
    Object first;
    Object second;

    void Create(std::uint32_t slots, std::uint32_t queues = 1, std::uint64_t size = sizeof(std::uint64_t)) {
        CHECK(sceUltQueueDataResourcePoolCreate(&pool, "pool", slots, size, queues, nullptr, nullptr, nullptr, 0) == 0);
        CHECK(sceUltQueueCreate(&first, "first", size, nullptr, &pool, nullptr, 0) == 0);
        if (queues > 1) CHECK(sceUltQueueCreate(&second, "second", size, nullptr, &pool, nullptr, 0) == 0);
    }

    void Destroy(bool secondQueue = false) {
        CHECK(sceUltQueueDestroy(&first) == 0);
        if (secondQueue) CHECK(sceUltQueueDestroy(&second) == 0);
        CHECK(sceUltQueueDataResourcePoolDestroy(&pool) == 0);
    }
};

static void Validation() {
    Fixture fixture;
    Object extra;
    std::uint64_t value = 99;
    CHECK(sceUltQueueDataResourcePoolCreate(nullptr, nullptr, 1, 8, 1, nullptr, nullptr, nullptr, 0) == Null);
    CHECK(sceUltQueueDataResourcePoolCreate(fixture.pool.bytes.data() + 1, nullptr, 1, 8, 1, nullptr, nullptr, nullptr, 0) == Alignment);
    CHECK(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 0, 8, 1, nullptr, nullptr, nullptr, 0) == Range);
    CHECK(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 1, 0, 1, nullptr, nullptr, nullptr, 0) == Range);
    CHECK(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 1, 8, 0, nullptr, nullptr, nullptr, 0) == Range);
    CHECK(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 2, UINT64_MAX, 1, nullptr, nullptr, nullptr, 0) == Range);
    CHECK(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 1, 8, 1, &extra, nullptr, nullptr, 0) == Invalid);
    CHECK(sceUltQueueCreate(&fixture.first, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == Invalid);
    fixture.Create(2);
    CHECK(sceUltQueuePush(&fixture.first, &value) == 0);
    CHECK(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 1, 8, 1, nullptr, nullptr, nullptr, 0) == State);
    CHECK(sceUltQueueCreate(&fixture.first, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == State);
    std::uint64_t saved = 0;
    CHECK(sceUltQueueTryPop(&fixture.first, &saved) == 0 && saved == value);
    CHECK(sceUltQueueCreate(&extra, nullptr, 9, nullptr, &fixture.pool, nullptr, 0) == Invalid);
    CHECK(sceUltQueueCreate(&extra, nullptr, 8, &extra, &fixture.pool, nullptr, 0) == Invalid);
    CHECK(sceUltQueueCreate(&extra, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == Again);
    CHECK(sceUltQueueCreate(nullptr, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == Null);
    CHECK(sceUltQueueCreate(extra.bytes.data() + 1, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == Alignment);
    CHECK(sceUltQueueCreate(&extra, nullptr, 0, nullptr, &fixture.pool, nullptr, 0) == Range);
    CHECK(sceUltQueueTryPop(&fixture.first, &value) == Again && value == 99);
    CHECK(sceUltQueueTryPush(&fixture.first, nullptr) == Null);
    CHECK(sceUltQueuePop(&fixture.first, nullptr) == Null);
    CHECK(sceUltQueuePush(nullptr, &value) == Null);
    CHECK(sceUltQueuePop(&extra, &value) == State);
    CHECK(sceUltQueueDataResourcePoolDestroy(&fixture.pool) == Busy);
    CHECK(sceUltQueueDataResourcePoolDestroy(nullptr) == Null);
    CHECK(sceUltQueueDestroy(nullptr) == Null);
    CHECK(sceUltQueueDataResourcePoolGetWorkAreaSize(3, 9, 2) == 1072);
    bool overflow = false;
    try { sceUltQueueDataResourcePoolGetWorkAreaSize(1, UINT64_MAX, 1); }
    catch (const std::out_of_range&) { overflow = true; }
    CHECK(overflow);
    bool rejected = false;
    try { sceUltQueueCreate(&extra, nullptr, 8, nullptr, &fixture.pool, &extra, 0); }
    catch (const std::runtime_error&) { rejected = true; }
    CHECK(rejected);
    rejected = false;
    try { sceUltQueueDataResourcePoolCreate(&extra, nullptr, 1, 8, 1, nullptr, nullptr, &extra, 0); }
    catch (const std::runtime_error&) { rejected = true; }
    CHECK(rejected);
    overflow = false;
    try { sceUltQueueDataResourcePoolGetWorkAreaSize(UINT32_MAX, UINT64_MAX / 2, 1); }
    catch (const std::out_of_range&) { overflow = true; }
    CHECK(overflow);
    fixture.Destroy();
    CHECK(sceUltQueueTryPush(&fixture.first, &value) == State);
    CHECK(sceUltQueueDestroy(&fixture.first) == State);
    CHECK(sceUltQueueDataResourcePoolDestroy(&fixture.pool) == State);
    fixture.Create(1);
    fixture.Destroy();
}

static void Fifo() {
    Fixture fixture;
    fixture.Create(3, 1, 13);
    std::array<std::array<std::uint8_t, 13>, 4> items{};
    for (std::size_t index = 0; index < items.size(); ++index) {
        for (std::size_t byte = 0; byte < items[index].size(); ++byte) items[index][byte] = index * 31 + byte;
    }
    for (int round = 0; round < 100; ++round) {
        for (int index = 0; index < 3; ++index) CHECK(sceUltQueueTryPush(&fixture.first, items[index].data()) == 0);
        CHECK(sceUltQueueTryPush(&fixture.first, items[3].data()) == Again);
        std::array<std::uint8_t, 15> output;
        for (int index = 0; index < 3; ++index) {
            output.fill(0xAA);
            CHECK(sceUltQueuePop(&fixture.first, output.data() + 1) == 0);
            CHECK(output.front() == 0xAA && output.back() == 0xAA);
            CHECK(std::memcmp(output.data() + 1, items[index].data(), 13) == 0);
        }
        CHECK(sceUltQueueTryPop(&fixture.first, output.data()) == Again);
    }
    fixture.Destroy();
}

static void WaitUntilBlocked(std::future<int>& result) {
    CHECK(result.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);
}

static void Blocking() {
    Fixture fixture;
    fixture.Create(1);
    std::uint64_t first = 11, second = 22, output = 0;
    CHECK(sceUltQueuePush(&fixture.first, &first) == 0);
    auto producer = std::async(std::launch::async, [&] { return sceUltQueuePush(&fixture.first, &second); });
    WaitUntilBlocked(producer);
    CHECK(sceUltQueueDestroy(&fixture.first) == Busy);
    CHECK(sceUltQueuePop(&fixture.first, &output) == 0 && output == first);
    CHECK(producer.get() == 0);
    CHECK(sceUltQueuePop(&fixture.first, &output) == 0 && output == second);
    auto consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&fixture.first, &output); });
    WaitUntilBlocked(consumer);
    CHECK(sceUltQueueDestroy(&fixture.first) == Busy);
    CHECK(sceUltQueuePush(&fixture.first, &first) == 0);
    CHECK(consumer.get() == 0 && output == first);
    fixture.Destroy();
}

static void SharedPool() {
    Fixture fixture;
    fixture.Create(1, 2);
    std::uint64_t first = 111, second = 222, output = 0;
    auto consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&fixture.second, &output); });
    WaitUntilBlocked(consumer);
    CHECK(sceUltQueuePush(&fixture.first, &first) == 0);
    WaitUntilBlocked(consumer);
    CHECK(sceUltQueueTryPush(&fixture.second, &second) == Again);
    CHECK(sceUltQueueTryPop(&fixture.second, &output) == Again);
    auto producer = std::async(std::launch::async, [&] { return sceUltQueuePush(&fixture.second, &second); });
    WaitUntilBlocked(producer);
    CHECK(sceUltQueueDestroy(&fixture.second) == Busy);
    CHECK(sceUltQueueDestroy(&fixture.first) == 0);
    CHECK(producer.get() == 0);
    CHECK(consumer.get() == 0 && output == second);
    CHECK(sceUltQueueCreate(&fixture.first, "replacement", 4, nullptr, &fixture.pool, nullptr, 0) == 0);
    const std::uint32_t small = 42;
    CHECK(sceUltQueuePush(&fixture.first, &small) == 0);
    std::uint32_t smallOutput = 0;
    CHECK(sceUltQueuePop(&fixture.first, &smallOutput) == 0 && smallOutput == small);
    fixture.Destroy(true);
}

static void Finalize() {
    Fixture fixture;
    fixture.Create(1, 2);
    std::uint64_t value = 7, output = 123;
    CHECK(sceUltQueuePush(&fixture.first, &value) == 0);
    auto producer = std::async(std::launch::async, [&] { return sceUltQueuePush(&fixture.first, &value); });
    auto consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&fixture.second, &output); });
    WaitUntilBlocked(producer);
    WaitUntilBlocked(consumer);
    CHECK(sceUltQueueDestroy(&fixture.first) == Busy);
    CHECK(sceUltQueueDestroy(&fixture.second) == Busy);
    CHECK(sceUltFinalize() == 0);
    CHECK(producer.get() == State);
    CHECK(consumer.get() == State && output == 123);
    CHECK(sceUltQueueTryPop(&fixture.first, &output) == State);
    CHECK(sceUltInitialize() == 0);
    fixture.Create(1, 2);
    CHECK(sceUltQueueTryPop(&fixture.first, &output) == Again);
    CHECK(sceUltQueuePush(&fixture.second, &value) == 0);
    CHECK(sceUltQueuePop(&fixture.second, &output) == 0 && output == value);
    fixture.Destroy(true);
}

static void Stress() {
    Fixture fixture;
    fixture.Create(7);
    constexpr std::size_t workers = 4;
    constexpr std::size_t perWorker = 2000;
    std::array<std::atomic<unsigned>, workers * perWorker> seen{};
    std::vector<std::thread> producers, consumers;
    for (std::size_t worker = 0; worker < workers; ++worker) {
        consumers.emplace_back([&] {
            for (std::size_t index = 0; index < perWorker; ++index) {
                std::uint64_t value = UINT64_MAX;
                CHECK(sceUltQueuePop(&fixture.first, &value) == 0);
                CHECK(value < seen.size());
                ++seen[value];
            }
        });
        producers.emplace_back([&, worker] {
            for (std::size_t index = 0; index < perWorker; ++index) {
                const std::uint64_t value = worker * perWorker + index;
                CHECK(sceUltQueuePush(&fixture.first, &value) == 0);
            }
        });
    }
    for (auto& producer : producers) producer.join();
    for (auto& consumer : consumers) consumer.join();
    for (const auto& count : seen) CHECK(count == 1);
    std::uint64_t output = 0;
    CHECK(sceUltQueueTryPop(&fixture.first, &output) == Again);
    fixture.Destroy();
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    CHECK(sceUltInitialize() == 0);
    if (std::strcmp(argv[1], "validation") == 0) Validation();
    else if (std::strcmp(argv[1], "fifo") == 0) Fifo();
    else if (std::strcmp(argv[1], "blocking") == 0) Blocking();
    else if (std::strcmp(argv[1], "shared-pool") == 0) SharedPool();
    else if (std::strcmp(argv[1], "finalize") == 0) Finalize();
    else if (std::strcmp(argv[1], "stress") == 0) Stress();
    else CHECK(false);
    CHECK(sceUltFinalize() == 0);
}
