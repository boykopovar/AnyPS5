#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int SCE_OK = 0;
constexpr int POSIX_EINVAL = 22;
constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestPthreadCondattrInitNullBounds() {
    Require(pthread_condattr_init_nid_postfix(nullptr) == POSIX_EINVAL);

    bool threw = false;
    try {
        scePthreadCondattrInit(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == SCE_OK);
    Require(attr != nullptr);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == SCE_OK);
    Require(attr == nullptr);
}

void TestPthreadCondattrDestroyNullBounds() {
    Require(pthread_condattr_destroy_nid_postfix(nullptr) == POSIX_EINVAL);

    PthreadCondattr unset = nullptr;
    Require(pthread_condattr_destroy_nid_postfix(&unset) == POSIX_EINVAL);

    Require(scePthreadCondattrDestroy(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadCondattrDestroy(&unset) == SCE_KERNEL_ERROR_EINVAL);

    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == SCE_OK);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == SCE_OK);
    Require(attr == nullptr);
    Require(pthread_condattr_destroy_nid_postfix(&attr) == POSIX_EINVAL);
    Require(scePthreadCondattrDestroy(&attr) == SCE_KERNEL_ERROR_EINVAL);
}

void TestPthreadCondTimedwaitNullBounds() {
    PthreadCond cond = nullptr;
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, nullptr) == POSIX_EINVAL);

    const KernelTimespec negativeSec{-1, 0};
    const KernelTimespec negativeNsec{0, -1};
    const KernelTimespec overflowNsec{0, 1000000000};
    const KernelTimespec extremeNsec{0, 2000000000};

    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &negativeSec) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &negativeNsec) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &overflowNsec) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &extremeNsec) == POSIX_EINVAL);

    Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &negativeSec) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &negativeSec) == POSIX_EINVAL);

    bool threwCondNull = false;
    try {
        scePthreadCondTimedwait(nullptr, &mutex, 1000);
    } catch (const std::invalid_argument&) {
        threwCondNull = true;
    }
    Require(threwCondNull);

    bool threwMutexNull = false;
    try {
        scePthreadCondTimedwait(&cond, nullptr, 1000);
    } catch (const std::invalid_argument&) {
        threwMutexNull = true;
    }
    Require(threwMutexNull);

    Require(scePthreadCondDestroy(&cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}

}

int main() {
    TestPthreadCondattrInitNullBounds();
    TestPthreadCondattrDestroyNullBounds();
    TestPthreadCondTimedwaitNullBounds();
    return 0;
}
