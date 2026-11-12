#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdlib>
#include <thread>
#include <vector>

static constexpr int POSIX_EFAULT = 14;
static constexpr int POSIX_EINVAL = 22;
static constexpr int POSIX_ETIMEDOUT = 60;

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    PthreadCond cond = nullptr;
    PthreadMutex mutex = nullptr;
    PthreadCondattr attr = nullptr;
    const KernelTimespec abstime{100, 0};

    Require(pthread_cond_init_nid_postfix(nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_destroy_nid_postfix(nullptr) == POSIX_EINVAL);
    Require(pthread_cond_signal_nid_postfix(nullptr) == POSIX_EINVAL);
    Require(pthread_cond_broadcast_nid_postfix(nullptr) == POSIX_EINVAL);
    Require(pthread_cond_wait_nid_postfix(nullptr, &mutex) == POSIX_EINVAL);
    Require(pthread_cond_wait_nid_postfix(&cond, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_wait_nid_postfix(nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &abstime) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &abstime) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_reltimedwait_np_nid_postfix(nullptr, &mutex, 1000) == POSIX_EINVAL);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, nullptr, 1000) == POSIX_EINVAL);
    Require(pthread_cond_reltimedwait_np_nid_postfix(nullptr, nullptr, 1000) == POSIX_EINVAL);
    Require(pthread_cond_signalto_np_nid_postfix(nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_signalto_np_nid_postfix(&cond, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_signalto_np_nid_postfix(nullptr, reinterpret_cast<Pthread>(1)) == POSIX_EINVAL);
    Require(pthread_condattr_init_nid_postfix(nullptr) == POSIX_EINVAL);
    Require(pthread_condattr_destroy_nid_postfix(nullptr) == POSIX_EINVAL);

    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    int pshared = -1;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == 0);
    Require(pthread_condattr_setpshared_nid_postfix(&attr, 0) == 0);
    Require(pthread_condattr_setpshared_nid_postfix(&attr, 1) == POSIX_EINVAL);
    Require(pthread_condattr_getpshared_nid_postfix(&attr, reinterpret_cast<int*>(0x10)) == POSIX_EFAULT);

    Require(pthread_cond_init_nid_postfix(&cond, &attr) == 0);
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    const KernelTimespec badSec{-1, 0};
    const KernelTimespec badNsecNeg{0, -1};
    const KernelTimespec badNsecHigh{0, 1000000000};
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &badSec) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &badNsecNeg) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &badNsecHigh) == POSIX_EINVAL);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &mutex, 1000) == POSIX_ETIMEDOUT);

    bool ready = false;
    bool waiting = false;
    std::thread waiter([&] {
        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        waiting = true;
        Require(pthread_cond_signal_nid_postfix(&cond) == 0);
        while (!ready) {
            Require(pthread_cond_wait_nid_postfix(&cond, &mutex) == 0);
        }
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    });
    while (!waiting) {
        Require(pthread_cond_wait_nid_postfix(&cond, &mutex) == 0);
    }
    ready = true;
    Require(pthread_cond_signal_nid_postfix(&cond) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    waiter.join();

    std::atomic<int> arrived{0};
    bool releaseAll = false;
    std::vector<std::thread> waiters;
    for (int i = 0; i < 3; ++i) {
        waiters.emplace_back([&] {
            Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
            arrived.fetch_add(1, std::memory_order_release);
            Require(pthread_cond_broadcast_nid_postfix(&cond) == 0);
            while (!releaseAll) {
                Require(pthread_cond_wait_nid_postfix(&cond, &mutex) == 0);
            }
            Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
        });
    }
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    while (arrived.load(std::memory_order_acquire) < 3) {
        Require(pthread_cond_wait_nid_postfix(&cond, &mutex) == 0);
    }
    releaseAll = true;
    Require(pthread_cond_broadcast_nid_postfix(&cond) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    for (auto& thread : waiters) thread.join();

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
}
