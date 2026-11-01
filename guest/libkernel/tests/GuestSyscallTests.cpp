#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

constexpr int ErrorInvalid = 22;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSemTimedwaitNegativeTimeoutOnBlockedSem() {
    std::uintptr_t sem = 0;
    Require(sem_init_nid_postfix(&sem, 0, 0) == 0);

    const KernelTimespec negativeNsecs[] = {
        {0, -1},
        {0, -2},
        {0, -1000},
        {0, -1000000000LL},
        {0, std::numeric_limits<std::int64_t>::min()},
        {1, -1},
        {-1, -1},
        {-10, -500},
        {100, -1000000LL},
    };

    for (const auto& ts : negativeNsecs) {
        *__error_nid_postfix() = 0;
        Require(sem_timedwait_nid_postfix(&sem, &ts) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);
    }

    const KernelTimespec overflowNsecs[] = {
        {0, 1000000000LL},
        {0, 1000000001LL},
        {0, 2000000000LL},
        {0, std::numeric_limits<std::int64_t>::max()},
        {1, 1000000000LL},
        {-1, 1000000000LL},
        {100, 5000000000LL},
    };

    for (const auto& ts : overflowNsecs) {
        *__error_nid_postfix() = 0;
        Require(sem_timedwait_nid_postfix(&sem, &ts) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);
    }

    int value = -1;
    Require(sem_getvalue_nid_postfix(&sem, &value) == 0);
    Require(value == 0);

    Require(sem_destroy_nid_postfix(&sem) == 0);
}

void TestSemTimedwaitNegativeTimeoutOnInvalidSem() {
    const KernelTimespec negativeTs{0, -1};
    const KernelTimespec overflowTs{0, 1000000000LL};
    const KernelTimespec validTs{0, 0};

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(nullptr, &negativeTs) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(nullptr, &overflowTs) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(nullptr, &validTs) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(nullptr, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    std::uintptr_t uninitSem = 0;
    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&uninitSem, &negativeTs) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&uninitSem, &overflowTs) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&uninitSem, &validTs) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    *__error_nid_postfix() = 0;
    Require(sem_timedwait_nid_postfix(&uninitSem, nullptr) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);
}

void TestKsemTimedwaitNegativeTimeoutOnBlockedKsem() {
    std::intptr_t ksemId = -1;
    Require(ksem_init_nid_postfix(&ksemId, 0) == 0);
    Require(ksemId >= 0);

    const KernelTimespec negativeNsecs[] = {
        {0, -1},
        {0, -2},
        {0, -1000},
        {0, -1000000000LL},
        {0, std::numeric_limits<std::int64_t>::min()},
        {1, -1},
        {-1, -1},
        {-10, -500},
        {100, -1000000LL},
    };

    for (const auto& ts : negativeNsecs) {
        *__error_nid_postfix() = 0;
        Require(ksem_timedwait_nid_postfix(ksemId, &ts) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);
    }

    const KernelTimespec overflowNsecs[] = {
        {0, 1000000000LL},
        {0, 1000000001LL},
        {0, 2000000000LL},
        {0, std::numeric_limits<std::int64_t>::max()},
        {1, 1000000000LL},
        {-1, 1000000000LL},
        {100, 5000000000LL},
    };

    for (const auto& ts : overflowNsecs) {
        *__error_nid_postfix() = 0;
        Require(ksem_timedwait_nid_postfix(ksemId, &ts) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);
    }

    int value = -1;
    Require(ksem_getvalue_nid_postfix(ksemId, &value) == 0);
    Require(value == 0);

    Require(ksem_destroy_nid_postfix(ksemId) == 0);
}

void TestKsemTimedwaitNegativeTimeoutOnAvailableKsem() {
    std::intptr_t ksemId = -1;
    Require(ksem_init_nid_postfix(&ksemId, 2) == 0);
    Require(ksemId >= 0);

    const KernelTimespec negativeTs{0, -1};
    *__error_nid_postfix() = 0;
    Require(ksem_timedwait_nid_postfix(ksemId, &negativeTs) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    const KernelTimespec overflowTs{0, 1000000000LL};
    *__error_nid_postfix() = 0;
    Require(ksem_timedwait_nid_postfix(ksemId, &overflowTs) == -1);
    Require(*__error_nid_postfix() == ErrorInvalid);

    int value = -1;
    Require(ksem_getvalue_nid_postfix(ksemId, &value) == 0);
    Require(value == 2);

    Require(ksem_destroy_nid_postfix(ksemId) == 0);
}

void TestKsemTimedwaitNegativeTimeoutOnInvalidDescriptors() {
    const KernelTimespec negativeTs{0, -1};
    const KernelTimespec overflowTs{0, 1000000000LL};
    const KernelTimespec validTs{0, 0};

    for (const std::intptr_t badDescriptor : {-9999, -100, -1, 1024, 65535}) {
        *__error_nid_postfix() = 0;
        Require(ksem_timedwait_nid_postfix(badDescriptor, &negativeTs) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);

        *__error_nid_postfix() = 0;
        Require(ksem_timedwait_nid_postfix(badDescriptor, &overflowTs) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);

        *__error_nid_postfix() = 0;
        Require(ksem_timedwait_nid_postfix(badDescriptor, &validTs) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);

        *__error_nid_postfix() = 0;
        Require(ksem_timedwait_nid_postfix(badDescriptor, nullptr) == -1);
        Require(*__error_nid_postfix() == ErrorInvalid);
    }
}

}

int main() {
    TestSemTimedwaitNegativeTimeoutOnBlockedSem();
    TestSemTimedwaitNegativeTimeoutOnInvalidSem();
    TestKsemTimedwaitNegativeTimeoutOnBlockedKsem();
    TestKsemTimedwaitNegativeTimeoutOnAvailableKsem();
    TestKsemTimedwaitNegativeTimeoutOnInvalidDescriptors();
    return 0;
}
