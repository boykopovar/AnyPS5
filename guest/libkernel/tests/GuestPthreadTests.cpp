#include <cstdint>
#include <cstdlib>
#include <exception>

struct PthreadMutexPrivate;
struct PthreadMutexattrPrivate;

using PthreadMutex = PthreadMutexPrivate*;
using PthreadMutexattr = PthreadMutexattrPrivate*;

struct KernelTimespec {
    std::int64_t tv_sec;
    std::int64_t tv_nsec;
};

static constexpr int POSIX_EINVAL = 22;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TCallable>
static bool CatchesError(TCallable&& callable, int expectedPosix, int expectedSce) {
    try {
        const int result = callable();
        return result == expectedPosix || result == expectedSce;
    } catch (const std::exception&) {
        return true;
    }
}

extern "C" {
int pthread_mutex_init(PthreadMutex* mutex, const PthreadMutexattr* attr);
int pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr);
int pthread_mutex_lock_nid_postfix(PthreadMutex* mutex);
int pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex);
int pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex);
int pthread_mutex_timedlock(PthreadMutex* mutex, const KernelTimespec* abstime);
int pthread_mutex_timedlock_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime);
int pthread_mutexattr_init(PthreadMutexattr* attr);
int pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr);
int pthread_mutexattr_destroy(PthreadMutexattr* attr);
int pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr);
int pthread_mutexattr_getprotocol(const PthreadMutexattr* attr, int* protocol);
int pthread_mutexattr_getprotocol_nid_postfix(const PthreadMutexattr* attr, int* protocol);
int pthread_mutexattr_setprotocol(PthreadMutexattr* attr, int protocol);
int pthread_mutexattr_setprotocol_nid_postfix(PthreadMutexattr* attr, int protocol);
int scePthreadMutexattrGetprotocol(const PthreadMutexattr* attr, int* protocol);
int scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
}

static void TestTimedlockNegativeTimespec() {
    PthreadMutex mutex = nullptr;
    Require(CatchesError([&] { return pthread_mutex_init_nid_postfix(&mutex, nullptr); }, 0, 0));
    Require(CatchesError([&] { return pthread_mutex_lock_nid_postfix(&mutex); }, 0, 0));

    const KernelTimespec negativeSec{-1, 0};
    const KernelTimespec negativeNsec{0, -1};
    const KernelTimespec overflowNsec{0, 1000000000};
    const KernelTimespec bothNegative{-5, -100};

    Require(CatchesError([&] { return pthread_mutex_timedlock_nid_postfix(&mutex, &negativeSec); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return pthread_mutex_timedlock_nid_postfix(&mutex, &negativeNsec); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return pthread_mutex_timedlock_nid_postfix(&mutex, &overflowNsec); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return pthread_mutex_timedlock_nid_postfix(&mutex, &bothNegative); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return pthread_mutex_timedlock_nid_postfix(nullptr, &negativeSec); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return pthread_mutex_timedlock_nid_postfix(nullptr, &negativeNsec); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));

    Require(CatchesError([&] { return pthread_mutex_unlock_nid_postfix(&mutex); }, 0, 0));
    Require(CatchesError([&] { return pthread_mutex_destroy_nid_postfix(&mutex); }, 0, 0));
}

static void TestMutexattrProtocolNull() {
    int protocol = -1;
    PthreadMutexattr attr = nullptr;

    Require(CatchesError([&] { return pthread_mutexattr_getprotocol_nid_postfix(nullptr, &protocol); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return pthread_mutexattr_getprotocol_nid_postfix(&attr, &protocol); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return scePthreadMutexattrGetprotocol(nullptr, &protocol); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return scePthreadMutexattrGetprotocol(&attr, &protocol); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));

    Require(CatchesError([&] { return pthread_mutexattr_init_nid_postfix(&attr); }, 0, 0));
    Require(CatchesError([&] { return pthread_mutexattr_getprotocol_nid_postfix(&attr, nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return scePthreadMutexattrGetprotocol(&attr, nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));

    Require(CatchesError([&] { return pthread_mutexattr_setprotocol_nid_postfix(nullptr, 0); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return scePthreadMutexattrSetprotocol(nullptr, 0); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));

    PthreadMutexattr uninitAttr = nullptr;
    Require(CatchesError([&] { return pthread_mutexattr_setprotocol_nid_postfix(&uninitAttr, 0); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesError([&] { return scePthreadMutexattrSetprotocol(&uninitAttr, 0); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));

    Require(CatchesError([&] { return pthread_mutexattr_destroy_nid_postfix(&attr); }, 0, 0));
}

int main() {
    TestTimedlockNegativeTimespec();
    TestMutexattrProtocolNull();
    return 0;
}
