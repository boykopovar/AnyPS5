#include "SceTypes.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int SCE_OK = 0;
constexpr int POSIX_EINVAL = 22;
constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);

constexpr std::array<KernelTimespec, 8> NEGATIVE_NSEC_TIMESPECS = {
    KernelTimespec{0, -1},
    KernelTimespec{0, -2},
    KernelTimespec{1, -100},
    KernelTimespec{10, -1000000},
    KernelTimespec{0, -1000000000},
    KernelTimespec{5, -9223372036854775807LL - 1},
    KernelTimespec{-1, -1},
    KernelTimespec{-10, -500}
};

constexpr std::array<KernelTimespec, 10> OVERFLOW_NSEC_TIMESPECS = {
    KernelTimespec{0, 1000000000},
    KernelTimespec{1, 1000000000},
    KernelTimespec{0, 1000000001},
    KernelTimespec{1, 1000000500},
    KernelTimespec{0, 2000000000},
    KernelTimespec{5, 5000000000LL},
    KernelTimespec{0, 9999999999LL},
    KernelTimespec{0, 9223372036854775807LL},
    KernelTimespec{-1, 1000000000},
    KernelTimespec{-5, 2000000000}
};

constexpr std::array<KernelTimespec, 6> NEGATIVE_SEC_TIMESPECS = {
    KernelTimespec{-1, 0},
    KernelTimespec{-2, 500000000},
    KernelTimespec{-10, 0},
    KernelTimespec{-99999, 100},
    KernelTimespec{-9223372036854775807LL - 1, 0},
    KernelTimespec{-9223372036854775807LL - 1, 999999999}
};

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestTimedwaitNegativeNanoseconds() {
    PthreadCond cond = nullptr;
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    for (const auto& ts : NEGATIVE_NSEC_TIMESPECS) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, &ts) == POSIX_EINVAL);
    }

    PthreadCond unsetCond = nullptr;
    PthreadMutex unsetMutex = nullptr;
    for (const auto& ts : NEGATIVE_NSEC_TIMESPECS) {
        Require(pthread_cond_timedwait_nid_postfix(&unsetCond, &mutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(&cond, &unsetMutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(&unsetCond, &unsetMutex, &ts) == POSIX_EINVAL);
    }

    Require(scePthreadCondDestroy(&cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}

void TestTimedwaitOverflowNanoseconds() {
    PthreadCond cond = nullptr;
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    for (const auto& ts : OVERFLOW_NSEC_TIMESPECS) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, &ts) == POSIX_EINVAL);
    }

    PthreadCond unsetCond = nullptr;
    PthreadMutex unsetMutex = nullptr;
    for (const auto& ts : OVERFLOW_NSEC_TIMESPECS) {
        Require(pthread_cond_timedwait_nid_postfix(&unsetCond, &mutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(&cond, &unsetMutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(&unsetCond, &unsetMutex, &ts) == POSIX_EINVAL);
    }

    Require(scePthreadCondDestroy(&cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}

void TestTimedwaitNegativeSeconds() {
    PthreadCond cond = nullptr;
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    for (const auto& ts : NEGATIVE_SEC_TIMESPECS) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &ts) == POSIX_EINVAL);
        Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, &ts) == POSIX_EINVAL);
    }

    Require(scePthreadCondDestroy(&cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}

void TestTimedwaitNullTimespec() {
    PthreadCond cond = nullptr;
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, nullptr) == POSIX_EINVAL);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, nullptr) == POSIX_EINVAL);

    PthreadCond unsetCond = nullptr;
    Require(pthread_cond_timedwait_nid_postfix(&unsetCond, &mutex, nullptr) == POSIX_EINVAL);

    Require(scePthreadCondDestroy(&cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}

void TestTimedwaitUninitializedAndDestroyedCond() {
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == SCE_OK);

    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondDestroy(&cond) == SCE_OK);

    const KernelTimespec validTimespec{1, 0};
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &validTimespec) == POSIX_EINVAL);

    for (const auto& ts : NEGATIVE_NSEC_TIMESPECS) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == POSIX_EINVAL);
    }
    for (const auto& ts : OVERFLOW_NSEC_TIMESPECS) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == POSIX_EINVAL);
    }
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, nullptr) == POSIX_EINVAL);

    Require(scePthreadCondTimedwait(&cond, &mutex, 1000) == SCE_KERNEL_ERROR_EINVAL);

    bool threw = false;
    try {
        pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &validTimespec);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        scePthreadCondTimedwait(nullptr, &mutex, 1000);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        scePthreadCondTimedwait(&cond, nullptr, 1000);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}

}

int main() {
    TestTimedwaitNegativeNanoseconds();
    TestTimedwaitOverflowNanoseconds();
    TestTimedwaitNegativeSeconds();
    TestTimedwaitNullTimespec();
    TestTimedwaitUninitializedAndDestroyedCond();
    return 0;
}
