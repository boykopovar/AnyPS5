#include <cstdint>
#include <cstdlib>
#include <exception>

struct PthreadMutexPrivate;
struct PthreadMutexattrPrivate;

using PthreadMutex = PthreadMutexPrivate*;
using PthreadMutexattr = PthreadMutexattrPrivate*;

static constexpr int POSIX_EINVAL = 22;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TCallable>
static bool CatchesNullError(TCallable&& callable, int expectedPosix, int expectedSce) {
    try {
        const int result = callable();
        return result == expectedPosix || result == expectedSce;
    } catch (const std::exception&) {
        return true;
    }
}

extern "C" {
int pthread_mutex_destroy(PthreadMutex* mutex);
int pthread_mutexattr_init(PthreadMutexattr* attr);
int pthread_mutexattr_destroy(PthreadMutexattr* attr);
int pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex);
int pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr);
int pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr);
int scePthreadMutexDestroy(PthreadMutex* mutex);
int scePthreadMutexattrInit(PthreadMutexattr* attr);
int scePthreadMutexattrDestroy(PthreadMutexattr* attr);
}

static void TestPthreadMutexDestroyNull() {
    Require(CatchesNullError([] { return pthread_mutex_destroy_nid_postfix(nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesNullError([] { return scePthreadMutexDestroy(nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
}

static void TestPthreadMutexattrInitNull() {
    Require(CatchesNullError([] { return pthread_mutexattr_init_nid_postfix(nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesNullError([] { return scePthreadMutexattrInit(nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
}

static void TestPthreadMutexattrDestroyNull() {
    Require(CatchesNullError([] { return pthread_mutexattr_destroy_nid_postfix(nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesNullError([] { return scePthreadMutexattrDestroy(nullptr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    PthreadMutexattr nullAttr = nullptr;
    Require(CatchesNullError([&] { return pthread_mutexattr_destroy_nid_postfix(&nullAttr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
    Require(CatchesNullError([&] { return scePthreadMutexattrDestroy(&nullAttr); }, POSIX_EINVAL, SCE_KERNEL_ERROR_EINVAL));
}

int main() {
    TestPthreadMutexDestroyNull();
    TestPthreadMutexattrInitNull();
    TestPthreadMutexattrDestroyNull();
    return 0;
}
