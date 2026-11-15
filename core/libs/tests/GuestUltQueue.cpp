#include "prx/libSceUlt/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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
    CHECK(sceUltQueueCreate(&extra, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == Busy);
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
    CHECK(sceUltQueueDataResourcePoolGetWorkAreaSize(3, 9, 2) == 379);
    CHECK(sceUltQueueDataResourcePoolGetWorkAreaSize(0, 0, 0) == 96);
    CHECK(sceUltWaitingQueueResourcePoolGetWorkAreaSize(0, 0) == 64);
    CHECK(sceUltWaitingQueueResourcePoolGetWorkAreaSize(3, 2) == 320);
    CHECK(sceUltWaitingQueueResourcePoolGetWorkAreaSize(0, 0x08000000u) == 0x100000040ull);
    CHECK(sceUltWaitingQueueResourcePoolGetWorkAreaSize(0x80000000u, 0) == 0x40ull);
    CHECK(sceUltQueueDataResourcePoolGetWorkAreaSize(0x80000000u, 1, 0) == 0x80000060ull);
    bool overflow = false;
    try { sceUltQueueDataResourcePoolGetWorkAreaSize(1, UINT64_MAX, 1); }
    catch (const std::out_of_range&) { overflow = true; }
    CHECK(overflow);
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
    std::array<std::uint8_t, 128> optParam{};
    optParam.fill(0x5A);
    CHECK(sceUltQueueDataResourcePoolCreate(&fixture.pool, "pool", 1, 8, 1, nullptr, nullptr, optParam.data(), 0) == 0);
    CHECK(sceUltQueueCreate(&fixture.first, "first", 8, nullptr, &fixture.pool, optParam.data(), 0) == 0);
    std::uint64_t output = 0;
    CHECK(sceUltQueuePush(&fixture.first, &value) == 0);
    CHECK(sceUltQueuePop(&fixture.first, &output) == 0 && output == value);
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

static void GuestPointers() {
    Fixture fixture;
    fixture.Create(2);
    const auto* unreadableInput = reinterpret_cast<const std::uint64_t*>(0x10);
    auto* unwritableOutput = reinterpret_cast<std::uint64_t*>(0x10);
    const std::uint64_t first = 0x123456789abcdef0ull;
    const std::uint64_t second = 0xfedcba9876543210ull;
    std::uint64_t output = 0;
    errno = 71;
    CHECK(sceUltQueueTryPush(&fixture.first, unreadableInput) == Null && errno == 71);
    CHECK(sceUltQueuePush(&fixture.first, unreadableInput) == Null && errno == 71);
    CHECK(sceUltQueueTryPop(&fixture.first, &output) == Again && output == 0);
    CHECK(sceUltQueuePush(&fixture.first, &first) == 0);
    CHECK(sceUltQueueTryPop(&fixture.first, unwritableOutput) == Null && errno == 71);
    CHECK(sceUltQueuePop(&fixture.first, unwritableOutput) == Null && errno == 71);
    CHECK(sceUltQueuePush(&fixture.first, &second) == 0);
    CHECK(sceUltQueueTryPush(&fixture.first, unreadableInput) == Again);
    CHECK(sceUltQueueTryPop(&fixture.first, &output) == 0 && output == first);
    CHECK(sceUltQueuePop(&fixture.first, &output) == 0 && output == second);
    CHECK(sceUltQueueTryPop(&fixture.first, &output) == Again);
    fixture.Destroy();

    auto clobberErrno = [](std::uintptr_t, std::size_t) { errno = 99; };
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(clobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(clobberErrno);

    Object waitPool;
    Object rwlock;
    CHECK(sceUltWaitingQueueResourcePoolCreate(&waitPool, "wait", 4, 4, nullptr, nullptr, 0) == 0);
    CHECK(sceUltReaderWriterLockCreate(&rwlock, "rwlock", &waitPool, nullptr, 0) == 0);
    CHECK(sceUltReaderWriterLockLockWrite(&rwlock) == 0);

    std::thread waiter([&] {
        CHECK(sceUltReaderWriterLockLockRead(&rwlock) == 0);
        CHECK(sceUltReaderWriterLockUnlockRead(&rwlock) == 0);
    });
    for (;;) {
        UltReaderWriterLockInfo pollInfo{};
        CHECK(sceUltGetReaderWriterLockInfo(&rwlock, &pollInfo, nullptr, 0) == 0);
        if (pollInfo.waitingThreads == 1) break;
        std::this_thread::yield();
    }

    UltReaderWriterLockInfo sentinelInfo{};
    std::memset(&sentinelInfo, 0xaa, sizeof(sentinelInfo));
    UltWaitingThreadInfo validWaiter{};
    validWaiter.size = sizeof(UltWaitingThreadInfo);

    std::size_t pageSize = 4096u;
    void* roInfoPage = nullptr;
    void* roWaiterPage = nullptr;
#ifdef _WIN32
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    if (sysInfo.dwPageSize != 0) pageSize = sysInfo.dwPageSize;
    roInfoPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    roWaiterPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(roInfoPage != nullptr && roWaiterPage != nullptr);
    std::memcpy(roInfoPage, &sentinelInfo, sizeof(sentinelInfo));
    std::memcpy(roWaiterPage, &validWaiter, sizeof(validWaiter));
    DWORD oldProtect = 0;
    CHECK(VirtualProtect(roInfoPage, pageSize, PAGE_READONLY, &oldProtect) != 0);
    CHECK(VirtualProtect(roWaiterPage, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    const long sysPage = sysconf(_SC_PAGESIZE);
    if (sysPage > 0) pageSize = static_cast<std::size_t>(sysPage);
    roInfoPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    roWaiterPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(roInfoPage != MAP_FAILED && roWaiterPage != MAP_FAILED);
    std::memcpy(roInfoPage, &sentinelInfo, sizeof(sentinelInfo));
    std::memcpy(roWaiterPage, &validWaiter, sizeof(validWaiter));
    CHECK(mprotect(roInfoPage, pageSize, PROT_READ) == 0);
    CHECK(mprotect(roWaiterPage, pageSize, PROT_READ) == 0);
#endif

    errno = 71;
    CHECK(sceUltGetReaderWriterLockInfo(&rwlock, reinterpret_cast<UltReaderWriterLockInfo*>(0x10), nullptr, 0) == Null && errno == 71);
    errno = 71;
    CHECK(sceUltGetReaderWriterLockInfo(&rwlock, static_cast<UltReaderWriterLockInfo*>(roInfoPage), nullptr, 0) == Null && errno == 71);
    CHECK(std::memcmp(roInfoPage, &sentinelInfo, sizeof(sentinelInfo)) == 0);

    UltReaderWriterLockInfo probeInfo{};
    std::memset(&probeInfo, 0x55, sizeof(probeInfo));
    const UltReaderWriterLockInfo expectedProbe = probeInfo;
    errno = 71;
    CHECK(sceUltGetReaderWriterLockInfo(&rwlock, &probeInfo, reinterpret_cast<UltWaitingThreadInfo*>(0x10), 1) == Null && errno == 71);
    CHECK(std::memcmp(&probeInfo, &expectedProbe, sizeof(probeInfo)) == 0);

    errno = 71;
    CHECK(sceUltGetReaderWriterLockInfo(&rwlock, &probeInfo, static_cast<UltWaitingThreadInfo*>(roWaiterPage), 1) == Null && errno == 71);
    CHECK(std::memcmp(&probeInfo, &expectedProbe, sizeof(probeInfo)) == 0);
    CHECK(std::memcmp(roWaiterPage, &validWaiter, sizeof(validWaiter)) == 0);

    UltWaitingThreadInfo waiterOut{};
    waiterOut.size = sizeof(UltWaitingThreadInfo);
    UltReaderWriterLockInfo validInfo{};
    errno = 71;
    CHECK(sceUltGetReaderWriterLockInfo(&rwlock, &validInfo, &waiterOut, 1) == 0 && errno == 71);
    CHECK(validInfo.waitingThreads == 1 && waiterOut.size == sizeof(UltWaitingThreadInfo) && waiterOut.thread != nullptr);

#ifdef _WIN32
    VirtualFree(roInfoPage, 0, MEM_RELEASE);
    VirtualFree(roWaiterPage, 0, MEM_RELEASE);
#else
    munmap(roInfoPage, pageSize);
    munmap(roWaiterPage, pageSize);
#endif
    CHECK(sceUltReaderWriterLockUnlockWrite(&rwlock) == 0);
    waiter.join();

    Object sem;
    CHECK(sceUltSemaphoreCreate(&sem, "sem", &waitPool, 0, 1, nullptr, 0) == 0);
    std::thread semWaiter([&] {
        CHECK(sceUltSemaphoreAcquire(&sem) == 0);
        CHECK(sceUltSemaphoreRelease(&sem) == 0);
    });
    for (;;) {
        UltSemaphoreInfo pollSem{};
        CHECK(sceUltGetSemaphoreInfo(&sem, &pollSem, nullptr, 0) == 0);
        if (pollSem.waitingThreads == 1) break;
        std::this_thread::yield();
    }
    UltSemaphoreInfo probeSem{};
    std::memset(&probeSem, 0x55, sizeof(probeSem));
    const UltSemaphoreInfo expectedProbeSem = probeSem;
    errno = 71;
    CHECK(sceUltGetSemaphoreInfo(&sem, &probeSem, reinterpret_cast<UltWaitingThreadInfo*>(0x10), 1) == Null && errno == 71);
    CHECK(std::memcmp(&probeSem, &expectedProbeSem, sizeof(probeSem)) == 0);
    CHECK(sceUltSemaphoreRelease(&sem) == 0);
    semWaiter.join();
    CHECK(sceUltSemaphoreDestroy(&sem) == 0);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    CHECK(sceUltReaderWriterLockDestroy(&rwlock) == 0);
    CHECK(sceUltWaitingQueueResourcePoolDestroy(&waitPool) == 0);
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
    CHECK(sceUltQueueDestroy(&fixture.first) == State);
    CHECK(sceUltQueuePop(&fixture.first, &output) == 0 && output == first);
    CHECK(producer.get() == 0);
    CHECK(sceUltQueuePop(&fixture.first, &output) == 0 && output == second);
    auto consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&fixture.first, &output); });
    WaitUntilBlocked(consumer);
    CHECK(sceUltQueueDestroy(&fixture.first) == State);
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
    CHECK(sceUltQueueDestroy(&fixture.second) == State);
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
    CHECK(sceUltQueueDestroy(&fixture.first) == State);
    CHECK(sceUltQueueDestroy(&fixture.second) == State);
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
    else if (std::strcmp(argv[1], "guest-pointers") == 0) GuestPointers();
    else if (std::strcmp(argv[1], "blocking") == 0) Blocking();
    else if (std::strcmp(argv[1], "shared-pool") == 0) SharedPool();
    else if (std::strcmp(argv[1], "finalize") == 0) Finalize();
    else if (std::strcmp(argv[1], "stress") == 0) Stress();
    else CHECK(false);
    CHECK(sceUltFinalize() == 0);
}
