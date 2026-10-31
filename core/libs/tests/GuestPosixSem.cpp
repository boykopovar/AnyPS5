#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <thread>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void CheckRetire() {
    constexpr int rounds = 20000;
    std::atomic<std::uintptr_t*> published{nullptr};
    std::thread poster([&] {
        for (int round = 0; round < rounds; ++round) {
            published.wait(nullptr, std::memory_order_acquire);
            std::uintptr_t* target = published.exchange(nullptr, std::memory_order_acq_rel);
            Require(sem_post_nid_postfix(target) == 0, "post to a semaphore its waiter destroys must succeed");
        }
    });
    for (int round = 0; round < rounds; ++round) {
        std::uintptr_t slot = 0;
        Require(sem_init_nid_postfix(&slot, 0, 0) == 0, "retired semaphore must initialize");
        published.store(&slot, std::memory_order_release);
        published.notify_one();
        for (int spins = 0; sem_trywait_nid_postfix(&slot) != 0; ++spins) {
            if (spins >= 4096) std::this_thread::yield();
        }
        Require(sem_destroy_nid_postfix(&slot) == 0, "semaphore must be destroyable once its count is taken");
    }
    poster.join();
}

static KernelTimespec RealtimeAfter(std::int64_t nanoseconds) {
    KernelTimespec now{};
    Require(sceKernelClockGettime(0, &now) == 0, "realtime clock must be readable");
    const auto total = now.tv_nsec + nanoseconds;
    return KernelTimespec{now.tv_sec + total / 1000000000, total % 1000000000};
}

static void CheckFutureDeadline() {
    std::uintptr_t sem = 0;
    int value = -1;
    Require(sem_init_nid_postfix(&sem, 0, 0) == 0, "deadline semaphore must initialize");
    const auto soon = RealtimeAfter(50000000);
    const auto start = std::chrono::steady_clock::now();
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&sem, &soon) == -1 && *__error_nid_postfix() == 60, "future deadline must expire with ETIMEDOUT");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40), "future deadline expired early");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "expired deadline must preserve the count");

    int waitResult = -2;
    const auto later = RealtimeAfter(5000000000);
    std::thread waiter([&] { waitResult = sem_timedwait_nid_postfix(&sem, &later); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto posted = std::chrono::steady_clock::now();
    Require(sem_post_nid_postfix(&sem) == 0, "post must release a timed waiter");
    waiter.join();
    Require(waitResult == 0, "timed waiter must succeed after a post before its deadline");
    Require(std::chrono::steady_clock::now() - posted < std::chrono::seconds(4), "timed waiter waited for its deadline");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "timed waiter must consume the posted count");
    Require(sem_destroy_nid_postfix(&sem) == 0, "deadline semaphore destroy must succeed");
}

static void CheckRelativeTimeout() {
    std::uintptr_t sem = 0;
    int value = -1;
    Require(sem_init_nid_postfix(&sem, 0, 2) == 0, "relative-timeout semaphore must initialize");
    Require(sem_reltimedwait_np_nid_postfix(&sem, 50000) == 0, "relative wait on an available semaphore must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 1, "relative wait must decrement");
    Require(sem_wait_nid_postfix(&sem) == 0, "wait on an available semaphore must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "wait must decrement");

    const auto start = std::chrono::steady_clock::now();
    *__error_nid_postfix() = 0;
    Require(sem_reltimedwait_np_nid_postfix(&sem, 50000) == -1 && *__error_nid_postfix() == 60, "relative timeout must expire with ETIMEDOUT");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40), "relative timeout expired early");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "expired relative timeout must preserve the count");

    int waitResult = -2;
    std::thread waiter([&] { waitResult = sem_reltimedwait_np_nid_postfix(&sem, 5000000); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto posted = std::chrono::steady_clock::now();
    Require(sem_post_nid_postfix(&sem) == 0, "post must release a relative-timeout waiter");
    waiter.join();
    Require(waitResult == 0, "relative-timeout waiter must succeed after a post before its timeout");
    Require(std::chrono::steady_clock::now() - posted < std::chrono::seconds(4), "relative-timeout waiter waited for its timeout");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "relative-timeout waiter must consume the posted count");
    Require(sem_destroy_nid_postfix(&sem) == 0, "relative-timeout semaphore destroy must succeed");
}

static void CheckNullAndInvalidArguments() {
    std::uintptr_t sem = 0;
    int value = -1;
    const auto badSem = reinterpret_cast<void*>(std::uintptr_t{0x10});
    const auto badValue = reinterpret_cast<int*>(std::uintptr_t{0x10});
    const auto badTimespec = reinterpret_cast<const KernelTimespec*>(std::uintptr_t{0x10});

    *__error_nid_postfix() = 0;
    Require(sem_init_nid_postfix(nullptr, 0, 1) == -1 && *__error_nid_postfix() == 22, "sem_init(nullptr) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_init_nid_postfix(badSem, 0, 1) == -1 && *__error_nid_postfix() == 14, "sem_init(badSem) must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(sem_destroy_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 22, "sem_destroy(nullptr) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_destroy_nid_postfix(badSem) == -1 && *__error_nid_postfix() == 22, "sem_destroy(badSem) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_wait_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 22, "sem_wait(nullptr) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_wait_nid_postfix(badSem) == -1 && *__error_nid_postfix() == 22, "sem_wait(badSem) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 22, "sem_trywait(nullptr) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_trywait_nid_postfix(badSem) == -1 && *__error_nid_postfix() == 22, "sem_trywait(badSem) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_post_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 22, "sem_post(nullptr) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_post_nid_postfix(badSem) == -1 && *__error_nid_postfix() == 22, "sem_post(badSem) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(nullptr, &value) == -1 && *__error_nid_postfix() == 22, "sem_getvalue(nullptr, val) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(badSem, &value) == -1 && *__error_nid_postfix() == 22, "sem_getvalue(badSem, val) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_reltimedwait_np_nid_postfix(nullptr, 1000) == -1 && *__error_nid_postfix() == 22, "sem_reltimedwait_np(nullptr) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_reltimedwait_np_nid_postfix(badSem, 1000) == -1 && *__error_nid_postfix() == 22, "sem_reltimedwait_np(badSem) must fail with EINVAL");

    KernelTimespec soon{100, 0};
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(nullptr, &soon) == -1 && *__error_nid_postfix() == 22, "sem_timedwait(nullptr, abstime) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(badSem, &soon) == -1 && *__error_nid_postfix() == 22, "sem_timedwait(badSem, abstime) must fail with EINVAL");

    Require(sem_init_nid_postfix(&sem, 0, 0) == 0, "test semaphore must initialize");
    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(&sem, nullptr) == -1 && *__error_nid_postfix() == 22, "sem_getvalue(sem, nullptr) must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(&sem, badValue) == -1 && *__error_nid_postfix() == 14, "sem_getvalue(sem, badValue) must fail with EFAULT");

    KernelTimespec negativeNanoseconds{100, -1};
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&sem, &negativeNanoseconds) == -1 && *__error_nid_postfix() == 22, "negative nanoseconds must fail with EINVAL");
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&sem, badTimespec) == -1 && *__error_nid_postfix() == 14, "sem_timedwait(sem, badTimespec) must fail with EFAULT");

    auto* readOnlyMem = static_cast<char*>(mmap_nid_postfix(nullptr, 16384, 1, 0x1002, -1, 0));
    Require(readOnlyMem != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)), "mmap readOnlyMem must succeed");
    *__error_nid_postfix() = 0;
    Require(sem_init_nid_postfix(readOnlyMem, 0, 1) == -1 && *__error_nid_postfix() == 14, "sem_init(readOnlyMem) must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(sem_getvalue_nid_postfix(&sem, reinterpret_cast<int*>(readOnlyMem)) == -1 && *__error_nid_postfix() == 14, "sem_getvalue(sem, readOnlyMem) must fail with EFAULT");
    munmap_nid_postfix(readOnlyMem, 16384);

    auto* semPage = static_cast<void**>(mmap_nid_postfix(nullptr, 4096, 3, 0x1002, -1, 0));
    Require(semPage != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)), "mmap semPage must succeed");
    Require(sem_init_nid_postfix(semPage, 0, 1) == 0, "sem_init on semPage must succeed");
    Require(mprotect_nid_postfix(semPage, 4096, 1) == 0, "mprotect semPage to read-only must succeed");
    *__error_nid_postfix() = 0;
    Require(sem_destroy_nid_postfix(semPage) == -1 && *__error_nid_postfix() == 14, "sem_destroy on read-only page must fail with EFAULT");
    int readVal = -1;
    Require(sem_getvalue_nid_postfix(semPage, &readVal) == 0 && readVal == 1, "semaphore must be preserved after failed destroy");
    Require(mprotect_nid_postfix(semPage, 4096, 3) == 0, "mprotect semPage to read-write must succeed");
    Require(sem_destroy_nid_postfix(semPage) == 0, "sem_destroy on writable page must succeed");
    munmap_nid_postfix(semPage, 4096);

    Require(sem_destroy_nid_postfix(&sem) == 0, "test semaphore destroy must succeed");
}

int main(int argc, char** argv) {
    constexpr unsigned int maximum = 0x7fffffffu;
    std::uintptr_t sem = 0;
    int value = -1;
    if (argc > 1 && std::strcmp(argv[1], "init") == 0) {
        for (const unsigned int initial : {maximum + 1, 0xffffffffu}) {
            sem = 0x1234;
            *__error_nid_postfix() = 0;
            const int result = sem_init_nid_postfix(&sem, 0, initial);
            std::fprintf(stderr, "sem_init(%u): result=%d errno=%d\n", initial, result, *__error_nid_postfix());
            Require(result == -1 && *__error_nid_postfix() == 22, "initial count above maximum must fail with EINVAL");
            Require(sem == 0x1234, "failed initialization changed semaphore storage");
        }
        return 0;
    }
    Require(sem_init_nid_postfix(&sem, 0, maximum) == 0, "maximum count must initialize");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum), "maximum count must be readable");
    *__error_nid_postfix() = 0;
    const int result = sem_post_nid_postfix(&sem);
    std::fprintf(stderr, "sem_post(maximum): result=%d errno=%d\n", result, *__error_nid_postfix());
    Require(result == -1 && *__error_nid_postfix() == 84, "post at maximum must fail with guest EOVERFLOW");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum), "overflow changed count");
    Require(sem_trywait_nid_postfix(&sem) == 0, "wait at maximum must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum - 1), "wait must decrement");
    Require(sem_post_nid_postfix(&sem) == 0, "post below maximum must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum), "post must reach maximum");
    Require(sem_post_nid_postfix(&sem) == -1 && *__error_nid_postfix() == 84, "repeated overflow must fail");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == static_cast<int>(maximum), "repeated overflow changed count");
    Require(sem_destroy_nid_postfix(&sem) == 0, "destroy must succeed");
    Require(sem_init_nid_postfix(&sem, 0, 0) == 0, "zero count must initialize");
    Require(sem_trywait_nid_postfix(&sem) == -1 && *__error_nid_postfix() == 35, "empty semaphore must report EAGAIN");
    Require(sem_post_nid_postfix(&sem) == 0, "normal post must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 1, "normal post must increment");
    Require(sem_trywait_nid_postfix(&sem) == 0, "normal wait must succeed");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "normal wait must decrement");
    Require(sem_destroy_nid_postfix(&sem) == 0, "normal destroy must succeed");

    KernelTimespec invalidTimeout{0, 1000000000};
    Require(sem_init_nid_postfix(&sem, 0, 1) == 0, "available semaphore must initialize");
    Require(sem_timedwait_nid_postfix(&sem, nullptr) == 0, "available semaphore must ignore null timeout");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "null-timeout wait must consume the available count");
    Require(sem_destroy_nid_postfix(&sem) == 0, "null-timeout semaphore destroy must succeed");

    Require(sem_init_nid_postfix(&sem, 0, 1) == 0, "invalid-timeout semaphore must initialize");
    Require(sem_timedwait_nid_postfix(&sem, &invalidTimeout) == 0, "available semaphore must ignore invalid timeout");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "invalid-timeout wait must consume the available count");
    Require(sem_destroy_nid_postfix(&sem) == 0, "invalid-timeout semaphore destroy must succeed");

    Require(sem_init_nid_postfix(&sem, 0, 0) == 0, "blocked semaphore must initialize");
    std::atomic<bool> entered = false;
    std::atomic<bool> completed = false;
    int waitResult = -2;
    std::thread waiter([&] {
        entered.store(true, std::memory_order_release);
        waitResult = sem_timedwait_nid_postfix(&sem, nullptr);
        completed.store(true, std::memory_order_release);
    });
    while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Require(!completed.load(std::memory_order_acquire), "blocked null-timeout wait must remain blocked");
    Require(sem_post_nid_postfix(&sem) == 0, "post must release a null-timeout waiter");
    waiter.join();
    Require(waitResult == 0, "null-timeout waiter must succeed after a post");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "null-timeout waiter must consume the posted count");

    Require(sem_init_nid_postfix(&sem, 0, 0) == 0, "invalid-timeout blocked semaphore must initialize");
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&sem, &invalidTimeout) == -1 && *__error_nid_postfix() == 22, "blocked invalid-timeout wait must report EINVAL");
    KernelTimespec expiredTimeout{0, 0};
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&sem, &expiredTimeout) == -1 && *__error_nid_postfix() == 60, "blocked expired-timeout wait must report ETIMEDOUT");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 0, "failed timed waits must preserve the count");
    Require(sem_destroy_nid_postfix(&sem) == 0, "blocked semaphore destroy must succeed");
    CheckFutureDeadline();
    CheckRelativeTimeout();
    CheckRetire();
    CheckNullAndInvalidArguments();

    Require(sem_init_nid_postfix(&sem, 0, 1) == 0, "renamed semaphore must initialize");
    Require(sem_setname_nid_postfix(&sem, "semaphore") == 0, "first setname must succeed");
    *__error_nid_postfix() = 0;
    Require(sem_setname_nid_postfix(&sem, "other") == -1 && *__error_nid_postfix() == 35, "second setname must report EAGAIN");
    *__error_nid_postfix() = 0;
    Require(sem_setname_nid_postfix(&sem, "") == -1 && *__error_nid_postfix() == 35, "second setname with an empty name must report EAGAIN");
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0 && value == 1, "setname must preserve the count");
    bool rejected = false;
    try { sem_setname_nid_postfix(&sem, nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "null name must throw");
    rejected = false;
    try { sem_setname_nid_postfix(nullptr, "semaphore"); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "null semaphore must throw");
    Require(sem_trywait_nid_postfix(&sem) == 0, "renamed semaphore must stay usable");
    Require(sem_destroy_nid_postfix(&sem) == 0, "renamed semaphore destroy must succeed");
    *__error_nid_postfix() = 0;
    Require(sem_setname_nid_postfix(&sem, "semaphore") == -1 && *__error_nid_postfix() == 35, "destroyed semaphore must report EAGAIN like a zeroed one");
    Require(sem_init_nid_postfix(&sem, 0, 0) == 0, "reinitialized semaphore must initialize");
    Require(sem_setname_nid_postfix(&sem, "again") == 0, "a reinitialized semaphore can be named again");
    Require(sem_destroy_nid_postfix(&sem) == 0, "reinitialized semaphore destroy must succeed");
}
