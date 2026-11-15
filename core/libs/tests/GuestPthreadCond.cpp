#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

static constexpr int POSIX_EFAULT = 14;
static constexpr int POSIX_EINVAL = 22;
static constexpr int SCE_EFAULT = static_cast<int>(0x8002000e);

static void ClobberErrno(std::uintptr_t, std::size_t) { errno = 99; }

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
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_cond_signal_nid_postfix(&cond) == 0);
    Require(pthread_cond_broadcast_nid_postfix(&cond) == 0);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);

    errno = 71;
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, reinterpret_cast<const KernelTimespec*>(0x10)) == POSIX_EINVAL);
    Require(errno == 71);
    const KernelTimespec negativeAbstime{-1, 0};
    errno = 71;
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &negativeAbstime) == POSIX_EINVAL);
    Require(errno == 71);

    KernelClockid clockId = 99;
    errno = 71;
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == 0 && errno == 71);
    errno = 71;
    Require(pthread_condattr_getclock_nid_postfix(&attr, reinterpret_cast<KernelClockid*>(0x10)) == POSIX_EFAULT && errno == 71);

    int pshared = 99;
    errno = 71;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == 0 && errno == 71);
    errno = 71;
    Require(pthread_condattr_getpshared_nid_postfix(&attr, reinterpret_cast<int*>(0x10)) == POSIX_EFAULT && errno == 71);

    pshared = 99;
    errno = 71;
    Require(scePthreadCondattrGetpshared(&attr, &pshared) == 0 && pshared == 0 && errno == 71);
    errno = 71;
    Require(scePthreadCondattrGetpshared(&attr, reinterpret_cast<int*>(0x10)) == SCE_EFAULT && errno == 71);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
}
