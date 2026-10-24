#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <thread>

extern "C" {
int APS5_VABI sem_init_nid_postfix(void*, int, unsigned int);
int APS5_VABI sem_destroy_nid_postfix(void*);
int APS5_VABI sem_post_nid_postfix(void*);
int APS5_VABI sem_trywait_nid_postfix(void*);
int APS5_VABI sem_timedwait_nid_postfix(void*, const KernelTimespec*);
int APS5_VABI sem_getvalue_nid_postfix(void*, int*);
int* APS5_VABI __error_nid_postfix();
}

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
    CheckRetire();
}
