#include "prx/libSceUlt/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
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

struct alignas(8) Object { std::array<std::uint8_t, 512> bytes{}; };

struct Fixture {
    Object pool;
    Object first;
    Object second;

    void Create(std::uint32_t slots, std::uint32_t queues = 1, std::uint64_t size = sizeof(std::uint64_t)) {
        Require(sceUltQueueDataResourcePoolCreate(&pool, "pool", slots, size, queues, nullptr, nullptr, nullptr, 0) == 0);
        Require(sceUltQueueCreate(&first, "first", size, nullptr, &pool, nullptr, 0) == 0);
        if (queues > 1) Require(sceUltQueueCreate(&second, "second", size, nullptr, &pool, nullptr, 0) == 0);
    }

    void Destroy(bool secondQueue = false) {
        Require(sceUltQueueDestroy(&first) == 0);
        if (secondQueue) Require(sceUltQueueDestroy(&second) == 0);
        Require(sceUltQueueDataResourcePoolDestroy(&pool) == 0);
    }
};

static void Validation() {
    Fixture fixture;
    Object extra;
    std::uint64_t value = 99;
    Require(sceUltQueueDataResourcePoolCreate(nullptr, nullptr, 1, 8, 1, nullptr, nullptr, nullptr, 0) == Null);
    Require(sceUltQueueDataResourcePoolCreate(fixture.pool.bytes.data() + 1, nullptr, 1, 8, 1, nullptr, nullptr, nullptr, 0) == Alignment);
    Require(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 0, 8, 1, nullptr, nullptr, nullptr, 0) == Range);
    Require(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 1, 0, 1, nullptr, nullptr, nullptr, 0) == Range);
    Require(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 1, 8, 0, nullptr, nullptr, nullptr, 0) == Range);
    Require(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 2, UINT64_MAX, 1, nullptr, nullptr, nullptr, 0) == Range);
    Require(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 1, 8, 1, &extra, nullptr, nullptr, 0) == Invalid);
    Require(sceUltQueueCreate(&fixture.first, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == Invalid);
    fixture.Create(2);
    Require(sceUltQueuePush(&fixture.first, &value) == 0);
    Require(sceUltQueueDataResourcePoolCreate(&fixture.pool, nullptr, 1, 8, 1, nullptr, nullptr, nullptr, 0) == State);
    Require(sceUltQueueCreate(&fixture.first, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == State);
    std::uint64_t saved = 0;
    Require(sceUltQueueTryPop(&fixture.first, &saved) == 0 && saved == value);
    Require(sceUltQueueCreate(&extra, nullptr, 9, nullptr, &fixture.pool, nullptr, 0) == Invalid);
    Require(sceUltQueueCreate(&extra, nullptr, 8, &extra, &fixture.pool, nullptr, 0) == Invalid);
    Require(sceUltQueueCreate(&extra, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == Busy);
    Require(sceUltQueueCreate(nullptr, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == Null);
    Require(sceUltQueueCreate(extra.bytes.data() + 1, nullptr, 8, nullptr, &fixture.pool, nullptr, 0) == Alignment);
    Require(sceUltQueueCreate(&extra, nullptr, 0, nullptr, &fixture.pool, nullptr, 0) == Range);
    Require(sceUltQueueTryPop(&fixture.first, &value) == Again && value == 99);
    Require(sceUltQueueTryPush(&fixture.first, nullptr) == Null);
    Require(sceUltQueuePop(&fixture.first, nullptr) == Null);
    Require(sceUltQueuePush(nullptr, &value) == Null);
    Require(sceUltQueuePop(&extra, &value) == State);
    Require(sceUltQueueDataResourcePoolDestroy(&fixture.pool) == Busy);
    Require(sceUltQueueDataResourcePoolDestroy(nullptr) == Null);
    Require(sceUltQueueDestroy(nullptr) == Null);
    Require(sceUltQueueDataResourcePoolGetWorkAreaSize(3, 9, 2) == 379);
    Require(sceUltQueueDataResourcePoolGetWorkAreaSize(0, 0, 0) == 96);
    Require(sceUltWaitingQueueResourcePoolGetWorkAreaSize(0, 0) == 64);
    Require(sceUltWaitingQueueResourcePoolGetWorkAreaSize(3, 2) == 320);
    Require(sceUltWaitingQueueResourcePoolGetWorkAreaSize(0, 0x08000000u) == 0x100000040ull);
    Require(sceUltWaitingQueueResourcePoolGetWorkAreaSize(0x80000000u, 0) == 0x40ull);
    Require(sceUltQueueDataResourcePoolGetWorkAreaSize(0x80000000u, 1, 0) == 0x80000060ull);
    bool overflow = false;
    try { sceUltQueueDataResourcePoolGetWorkAreaSize(1, UINT64_MAX, 1); }
    catch (const std::out_of_range&) { overflow = true; }
    Require(overflow);
    overflow = false;
    try { sceUltQueueDataResourcePoolGetWorkAreaSize(UINT32_MAX, UINT64_MAX / 2, 1); }
    catch (const std::out_of_range&) { overflow = true; }
    Require(overflow);
    fixture.Destroy();
    Require(sceUltQueueTryPush(&fixture.first, &value) == State);
    Require(sceUltQueueDestroy(&fixture.first) == State);
    Require(sceUltQueueDataResourcePoolDestroy(&fixture.pool) == State);
    fixture.Create(1);
    fixture.Destroy();
    std::array<std::uint8_t, 128> optParam{};
    optParam.fill(0x5A);
    Require(sceUltQueueDataResourcePoolCreate(&fixture.pool, "pool", 1, 8, 1, nullptr, nullptr, optParam.data(), 0) == 0);
    Require(sceUltQueueCreate(&fixture.first, "first", 8, nullptr, &fixture.pool, optParam.data(), 0) == 0);
    std::uint64_t output = 0;
    Require(sceUltQueuePush(&fixture.first, &value) == 0);
    Require(sceUltQueuePop(&fixture.first, &output) == 0 && output == value);
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
        for (int index = 0; index < 3; ++index) Require(sceUltQueueTryPush(&fixture.first, items[index].data()) == 0);
        Require(sceUltQueueTryPush(&fixture.first, items[3].data()) == Again);
        std::array<std::uint8_t, 15> output;
        for (int index = 0; index < 3; ++index) {
            output.fill(0xAA);
            Require(sceUltQueuePop(&fixture.first, output.data() + 1) == 0);
            Require(output.front() == 0xAA && output.back() == 0xAA);
            Require(std::memcmp(output.data() + 1, items[index].data(), 13) == 0);
        }
        Require(sceUltQueueTryPop(&fixture.first, output.data()) == Again);
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
    Require(sceUltQueueTryPush(&fixture.first, unreadableInput) == Null && errno == 71);
    Require(sceUltQueuePush(&fixture.first, unreadableInput) == Null && errno == 71);
    Require(sceUltQueueTryPop(&fixture.first, &output) == Again && output == 0);
    Require(sceUltQueuePush(&fixture.first, &first) == 0);
    Require(sceUltQueueTryPop(&fixture.first, unwritableOutput) == Null && errno == 71);
    Require(sceUltQueuePop(&fixture.first, unwritableOutput) == Null && errno == 71);
    Require(sceUltQueuePush(&fixture.first, &second) == 0);
    Require(sceUltQueueTryPush(&fixture.first, unreadableInput) == Again);
    Require(sceUltQueueTryPop(&fixture.first, &output) == 0 && output == first);
    Require(sceUltQueuePop(&fixture.first, &output) == 0 && output == second);
    Require(sceUltQueueTryPop(&fixture.first, &output) == Again);
    fixture.Destroy();

    auto clobberErrno = [](std::uintptr_t, std::size_t) { errno = 99; };
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(clobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(clobberErrno);

    Object waitPool;
    Object rwlock;
    Require(sceUltWaitingQueueResourcePoolCreate(&waitPool, "wait", 4, 4, nullptr, nullptr, 0) == 0);
    Require(sceUltReaderWriterLockCreate(&rwlock, "rwlock", &waitPool, nullptr, 0) == 0);
    Require(sceUltReaderWriterLockLockWrite(&rwlock) == 0);

    std::thread waiter([&] {
        Require(sceUltReaderWriterLockLockRead(&rwlock) == 0);
        Require(sceUltReaderWriterLockUnlockRead(&rwlock) == 0);
    });
    for (;;) {
        UltReaderWriterLockInfo pollInfo{};
        Require(sceUltGetReaderWriterLockInfo(&rwlock, &pollInfo, nullptr, 0) == 0);
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
    Require(roInfoPage != nullptr && roWaiterPage != nullptr);
    std::memcpy(roInfoPage, &sentinelInfo, sizeof(sentinelInfo));
    std::memcpy(roWaiterPage, &validWaiter, sizeof(validWaiter));
    DWORD oldProtect = 0;
    Require(VirtualProtect(roInfoPage, pageSize, PAGE_READONLY, &oldProtect) != 0);
    Require(VirtualProtect(roWaiterPage, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    const long sysPage = sysconf(_SC_PAGESIZE);
    if (sysPage > 0) pageSize = static_cast<std::size_t>(sysPage);
    roInfoPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    roWaiterPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(roInfoPage != MAP_FAILED && roWaiterPage != MAP_FAILED);
    std::memcpy(roInfoPage, &sentinelInfo, sizeof(sentinelInfo));
    std::memcpy(roWaiterPage, &validWaiter, sizeof(validWaiter));
    Require(mprotect(roInfoPage, pageSize, PROT_READ) == 0);
    Require(mprotect(roWaiterPage, pageSize, PROT_READ) == 0);
#endif

    errno = 71;
    Require(sceUltGetReaderWriterLockInfo(&rwlock, reinterpret_cast<UltReaderWriterLockInfo*>(0x10), nullptr, 0) == Null && errno == 71);
    errno = 71;
    Require(sceUltGetReaderWriterLockInfo(&rwlock, static_cast<UltReaderWriterLockInfo*>(roInfoPage), nullptr, 0) == Null && errno == 71);
    Require(std::memcmp(roInfoPage, &sentinelInfo, sizeof(sentinelInfo)) == 0);

    UltReaderWriterLockInfo probeInfo{};
    std::memset(&probeInfo, 0x55, sizeof(probeInfo));
    const UltReaderWriterLockInfo expectedProbe = probeInfo;
    errno = 71;
    Require(sceUltGetReaderWriterLockInfo(&rwlock, &probeInfo, reinterpret_cast<UltWaitingThreadInfo*>(0x10), 1) == Null && errno == 71);
    Require(std::memcmp(&probeInfo, &expectedProbe, sizeof(probeInfo)) == 0);

    errno = 71;
    Require(sceUltGetReaderWriterLockInfo(&rwlock, &probeInfo, static_cast<UltWaitingThreadInfo*>(roWaiterPage), 1) == Null && errno == 71);
    Require(std::memcmp(&probeInfo, &expectedProbe, sizeof(probeInfo)) == 0);
    Require(std::memcmp(roWaiterPage, &validWaiter, sizeof(validWaiter)) == 0);

    UltWaitingThreadInfo waiterOut{};
    waiterOut.size = sizeof(UltWaitingThreadInfo);
    UltReaderWriterLockInfo validInfo{};
    errno = 71;
    Require(sceUltGetReaderWriterLockInfo(&rwlock, &validInfo, &waiterOut, 1) == 0 && errno == 71);
    Require(validInfo.waitingThreads == 1 && waiterOut.size == sizeof(UltWaitingThreadInfo) && waiterOut.thread != nullptr);

#ifdef _WIN32
    VirtualFree(roInfoPage, 0, MEM_RELEASE);
    VirtualFree(roWaiterPage, 0, MEM_RELEASE);
#else
    munmap(roInfoPage, pageSize);
    munmap(roWaiterPage, pageSize);
#endif
    Require(sceUltReaderWriterLockUnlockWrite(&rwlock) == 0);
    waiter.join();

    Object sem;
    Require(sceUltSemaphoreCreate(&sem, "sem", 0, &waitPool, nullptr, 0) == 0);
    std::thread semWaiter([&] {
        Require(sceUltSemaphoreAcquire(&sem, 1) == 0);
        Require(sceUltSemaphoreRelease(&sem, 1) == 0);
    });
    for (;;) {
        UltSemaphoreInfo pollSem{};
        Require(sceUltGetSemaphoreInfo(&sem, &pollSem, nullptr, 0) == 0);
        if (pollSem.waitingThreads == 1) break;
        std::this_thread::yield();
    }
    UltSemaphoreInfo probeSem{};
    std::memset(&probeSem, 0x55, sizeof(probeSem));
    const UltSemaphoreInfo expectedProbeSem = probeSem;
    errno = 71;
    Require(sceUltGetSemaphoreInfo(&sem, &probeSem, reinterpret_cast<UltWaitingThreadInfo*>(0x10), 1) == Null && errno == 71);
    Require(std::memcmp(&probeSem, &expectedProbeSem, sizeof(probeSem)) == 0);
    Require(sceUltSemaphoreRelease(&sem, 1) == 0);
    semWaiter.join();
    Require(sceUltSemaphoreDestroy(&sem) == 0);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    Require(sceUltReaderWriterLockDestroy(&rwlock) == 0);
    Require(sceUltWaitingQueueResourcePoolDestroy(&waitPool) == 0);
}

static void WaitUntilBlocked(std::future<int>& result) {
    Require(result.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);
}

static void Blocking() {
    Fixture fixture;
    fixture.Create(1);
    std::uint64_t first = 11, second = 22, output = 0;
    Require(sceUltQueuePush(&fixture.first, &first) == 0);
    auto producer = std::async(std::launch::async, [&] { return sceUltQueuePush(&fixture.first, &second); });
    WaitUntilBlocked(producer);
    Require(sceUltQueueDestroy(&fixture.first) == State);
    Require(sceUltQueuePop(&fixture.first, &output) == 0 && output == first);
    Require(producer.get() == 0);
    Require(sceUltQueuePop(&fixture.first, &output) == 0 && output == second);
    auto consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&fixture.first, &output); });
    WaitUntilBlocked(consumer);
    Require(sceUltQueueDestroy(&fixture.first) == State);
    Require(sceUltQueuePush(&fixture.first, &first) == 0);
    Require(consumer.get() == 0 && output == first);
    fixture.Destroy();
}

static void SharedPool() {
    Fixture fixture;
    fixture.Create(1, 2);
    std::uint64_t first = 111, second = 222, output = 0;
    auto consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&fixture.second, &output); });
    WaitUntilBlocked(consumer);
    Require(sceUltQueuePush(&fixture.first, &first) == 0);
    WaitUntilBlocked(consumer);
    Require(sceUltQueueTryPush(&fixture.second, &second) == Again);
    Require(sceUltQueueTryPop(&fixture.second, &output) == Again);
    auto producer = std::async(std::launch::async, [&] { return sceUltQueuePush(&fixture.second, &second); });
    WaitUntilBlocked(producer);
    Require(sceUltQueueDestroy(&fixture.second) == State);
    Require(sceUltQueueDestroy(&fixture.first) == 0);
    Require(producer.get() == 0);
    Require(consumer.get() == 0 && output == second);
    Require(sceUltQueueCreate(&fixture.first, "replacement", 4, nullptr, &fixture.pool, nullptr, 0) == 0);
    const std::uint32_t small = 42;
    Require(sceUltQueuePush(&fixture.first, &small) == 0);
    std::uint32_t smallOutput = 0;
    Require(sceUltQueuePop(&fixture.first, &smallOutput) == 0 && smallOutput == small);
    fixture.Destroy(true);
}

static void Finalize() {
    Fixture fixture;
    fixture.Create(1, 2);
    std::uint64_t value = 7, output = 123;
    Require(sceUltQueuePush(&fixture.first, &value) == 0);
    auto producer = std::async(std::launch::async, [&] { return sceUltQueuePush(&fixture.first, &value); });
    auto consumer = std::async(std::launch::async, [&] { return sceUltQueuePop(&fixture.second, &output); });
    WaitUntilBlocked(producer);
    WaitUntilBlocked(consumer);
    Require(sceUltQueueDestroy(&fixture.first) == State);
    Require(sceUltQueueDestroy(&fixture.second) == State);
    Require(sceUltFinalize() == 0);
    Require(producer.get() == State);
    Require(consumer.get() == State && output == 123);
    Require(sceUltQueueTryPop(&fixture.first, &output) == State);
    Require(sceUltInitialize() == 0);
    fixture.Create(1, 2);
    Require(sceUltQueueTryPop(&fixture.first, &output) == Again);
    Require(sceUltQueuePush(&fixture.second, &value) == 0);
    Require(sceUltQueuePop(&fixture.second, &output) == 0 && output == value);
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
                Require(sceUltQueuePop(&fixture.first, &value) == 0);
                Require(value < seen.size());
                ++seen[value];
            }
        });
        producers.emplace_back([&, worker] {
            for (std::size_t index = 0; index < perWorker; ++index) {
                const std::uint64_t value = worker * perWorker + index;
                Require(sceUltQueuePush(&fixture.first, &value) == 0);
            }
        });
    }
    for (auto& producer : producers) producer.join();
    for (auto& consumer : consumers) consumer.join();
    for (const auto& count : seen) Require(count == 1);
    std::uint64_t output = 0;
    Require(sceUltQueueTryPop(&fixture.first, &output) == Again);
    fixture.Destroy();
}

int main(int argc, char** argv) {
    Require(argc == 2);
    Require(sceUltInitialize() == 0);
    if (std::strcmp(argv[1], "validation") == 0) Validation();
    else if (std::strcmp(argv[1], "fifo") == 0) Fifo();
    else if (std::strcmp(argv[1], "guest-pointers") == 0) GuestPointers();
    else if (std::strcmp(argv[1], "blocking") == 0) Blocking();
    else if (std::strcmp(argv[1], "shared-pool") == 0) SharedPool();
    else if (std::strcmp(argv[1], "finalize") == 0) Finalize();
    else if (std::strcmp(argv[1], "stress") == 0) Stress();
    else Require(false);
    Require(sceUltFinalize() == 0);
}
