#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <cstdlib>

static constexpr int POSIX_EINVAL = 22;

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
    Require(pthread_cond_init_nid_postfix(&cond, &attr) == 0);
    Require(pthread_cond_signal_nid_postfix(&cond) == 0);
    Require(pthread_cond_broadcast_nid_postfix(&cond) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
}
