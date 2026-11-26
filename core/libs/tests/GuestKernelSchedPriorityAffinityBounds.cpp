#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEsrch = 3;
constexpr int PosixEdeadlk = 11;
constexpr int PosixEacces = 13;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixErange = 34;
constexpr int PosixEnotsup = 45;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::abort();
    }
}

void TestThreadPriorityBounds() {
    int prio = 0;
    Require(scePthreadGetprio(nullptr, &prio) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadGetprio(scePthreadSelf(), nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadGetprio(scePthreadSelf(), &prio) == 0);
    const int originalPrio = prio;

    Require(scePthreadSetprio(nullptr, 500) == SCE_KERNEL_ERROR_EINVAL);
    for (const int invalidPrio : {-1, 0, 255, 768, 895, 960, 1023, 4096}) {
        Require(scePthreadSetprio(scePthreadSelf(), invalidPrio) == SCE_KERNEL_ERROR_EINVAL);
    }

    for (const int validPrio : {256, 500, 767, 896, 920, 959}) {
        Require(scePthreadSetprio(scePthreadSelf(), validPrio) == 0);
        int current = 0;
        Require(scePthreadGetprio(scePthreadSelf(), &current) == 0);
        Require(current == validPrio);
    }
    Require(scePthreadSetprio(scePthreadSelf(), originalPrio) == 0);

    Require(pthread_getprio_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEinval);
    Require(pthread_getprio_nid_postfix(scePthreadSelf()) == originalPrio);

    Require(pthread_setprio_nid_postfix(nullptr, 500) == PosixEinval);
    Require(pthread_setprio_nid_postfix(scePthreadSelf(), 200) == PosixEinval);
    Require(pthread_setprio_nid_postfix(scePthreadSelf(), 600) == 0);
    Require(pthread_getprio_nid_postfix(scePthreadSelf()) == 600);
    Require(pthread_setprio_nid_postfix(scePthreadSelf(), originalPrio) == 0);
}

void TestSchedulingPolicyBounds() {
    int policy = 0;
    KernelSchedParam param{};
    Require(scePthreadGetschedparam(nullptr, &policy, &param) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadGetschedparam(scePthreadSelf(), nullptr, &param) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadGetschedparam(scePthreadSelf(), &policy, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadGetschedparam(scePthreadSelf(), &policy, &param) == 0);
    const int originalPolicy = policy;
    const KernelSchedParam originalParam = param;

    Require(scePthreadSetschedparam(scePthreadSelf(), 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadSetschedparam(nullptr, 1, &param) == SCE_KERNEL_ERROR_EINVAL);

    const KernelSchedParam invalidParam{100};
    Require(scePthreadSetschedparam(scePthreadSelf(), 1, &invalidParam) == SCE_KERNEL_ERROR_EINVAL);
    for (const int invalidPolicy : {-1, 0, 2, 4, 99}) {
        Require(scePthreadSetschedparam(scePthreadSelf(), invalidPolicy, &originalParam) == SCE_KERNEL_ERROR_EINVAL);
    }

    const KernelSchedParam roundRobinParam{650};
    Require(scePthreadSetschedparam(scePthreadSelf(), 3, &roundRobinParam) == 0);
    int currentPolicy = 0;
    KernelSchedParam currentParam{};
    Require(scePthreadGetschedparam(scePthreadSelf(), &currentPolicy, &currentParam) == 0);
    Require(currentPolicy == 3);
    Require(currentParam.sched_priority == 650);

    Require(scePthreadSetschedparam(scePthreadSelf(), originalPolicy, &originalParam) == 0);

    Require(pthread_getschedparam_nid_postfix(nullptr, &policy, &param) == PosixEsrch);
    Require(pthread_getschedparam_nid_postfix(scePthreadSelf(), nullptr, &param) == PosixEinval);
    Require(pthread_getschedparam_nid_postfix(scePthreadSelf(), &policy, nullptr) == PosixEinval);
    Require(pthread_getschedparam_nid_postfix(scePthreadSelf(), &policy, &param) == 0);

    Require(pthread_setschedparam_nid_postfix(nullptr, 1, &param) == PosixEinval);
    Require(pthread_setschedparam_nid_postfix(scePthreadSelf(), 1, nullptr) == PosixEinval);
    Require(pthread_setschedparam_nid_postfix(scePthreadSelf(), 2, &param) == PosixEinval);
    Require(pthread_setschedparam_nid_postfix(scePthreadSelf(), originalPolicy, &originalParam) == 0);
}

void TestProcessPriorityAndNice() {
    const int selfPid = getpid_nid_postfix();

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(-1, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(3, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(0, 999999) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    Require(getpriority_nid_postfix(0, 0) == 0);
    Require(getpriority_nid_postfix(0, selfPid) == 0);
    Require(getpriority_nid_postfix(1, 0) == 0);
    Require(getpriority_nid_postfix(1, selfPid) == 0);
    Require(getpriority_nid_postfix(2, 0) == 0);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(-1, 0, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(3, 0, 0) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(0, 999999, 0) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(0, 0, -1) == -1);
    Require(*__error_nid_postfix() == PosixEacces);

    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(0, selfPid, -20) == -1);
    Require(*__error_nid_postfix() == PosixEacces);

    Require(setpriority_nid_postfix(0, 0, 0) == 0);
    Require(setpriority_nid_postfix(0, selfPid, 5) == 0);
}

void TestRtPriorityAndYield() {
    GuestRtPrio rtprio{};
    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(99, 0, &rtprio) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(0, 0, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(0, 999999, &rtprio) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    Require(rtprio_thread_nid_postfix(0, 0, &rtprio) == 0);
    Require(rtprio.type == 2 || rtprio.type == 3 || rtprio.type == 10);

    GuestRtPrio setRealtime{};
    setRealtime.type = 2;
    setRealtime.prio = 10;
    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(1, 0, &setRealtime) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    GuestRtPrio setIdle{};
    setIdle.type = 4;
    setIdle.prio = 0;
    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(1, 0, &setIdle) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    Require(sched_yield_nid_postfix() == 0);
    scePthreadYield();

    Require(pthread_setconcurrency_nid_postfix(-1) == PosixEinval);
    Require(pthread_setconcurrency_nid_postfix(0) == 0);
    Require(pthread_getconcurrency_nid_postfix() == 0);
    Require(pthread_setconcurrency_nid_postfix(4) == 0);
    Require(pthread_getconcurrency_nid_postfix() == 4);
    Require(pthread_setconcurrency_nid_postfix(0) == 0);
}

void TestSchedParamBounds() {
    for (const int policy : {1, 3}) {
        Require(sched_get_priority_min_nid_postfix(policy) == 256);
        Require(sched_get_priority_max_nid_postfix(policy) == 767);
    }
    for (const int invalidPolicy : {-1, 0, 2, 4, 99}) {
        *__error_nid_postfix() = 0;
        Require(sched_get_priority_min_nid_postfix(invalidPolicy) == -1);
        Require(*__error_nid_postfix() == PosixEinval);
        *__error_nid_postfix() = 0;
        Require(sched_get_priority_max_nid_postfix(invalidPolicy) == -1);
        Require(*__error_nid_postfix() == PosixEinval);
    }

    const int selfPid = getpid_nid_postfix();
    Require(sched_getscheduler_nid_postfix(0) >= 1);
    Require(sched_getscheduler_nid_postfix(selfPid) >= 1);
    *__error_nid_postfix() = 0;
    Require(sched_getscheduler_nid_postfix(-1) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);
    *__error_nid_postfix() = 0;
    Require(sched_getscheduler_nid_postfix(999999) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    KernelSchedParam currentParam{};
    Require(sched_getparam_nid_postfix(0, &currentParam) == 0);
    Require(currentParam.sched_priority >= 256 && currentParam.sched_priority <= 767);
    *__error_nid_postfix() = 0;
    Require(sched_getparam_nid_postfix(0, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);
    *__error_nid_postfix() = 0;
    Require(sched_getparam_nid_postfix(999999, &currentParam) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    *__error_nid_postfix() = 0;
    Require(sched_setparam_nid_postfix(0, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);
    *__error_nid_postfix() = 0;
    Require(sched_setparam_nid_postfix(999999, &currentParam) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    for (const int outOfRange : {100, 255, 768, 1000}) {
        const KernelSchedParam invalid{outOfRange};
        *__error_nid_postfix() = 0;
        Require(sched_setparam_nid_postfix(0, &invalid) == -1);
        Require(*__error_nid_postfix() == PosixEperm);
    }

    const KernelSchedParam updatedParam{520};
    Require(sched_setparam_nid_postfix(0, &updatedParam) == 0);
    KernelSchedParam verifyParam{};
    Require(sched_getparam_nid_postfix(0, &verifyParam) == 0);
    Require(verifyParam.sched_priority == 520);
    Require(sched_setparam_nid_postfix(0, &currentParam) == 0);
}

void TestPthreadAttrSchedulingBounds() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);

    Require(scePthreadAttrSetschedpolicy(&attr, 1) == 0);
    Require(scePthreadAttrSetschedpolicy(&attr, 3) == 0);
    for (const int unsupportedPolicy : {-1, 0, 2, 4, 99}) {
        Require(scePthreadAttrSetschedpolicy(&attr, unsupportedPolicy) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    }

    int policy = 0;
    Require(scePthreadAttrGetschedpolicy(&attr, &policy) == 0);
    Require(policy == 3);

    Require(scePthreadAttrSetinheritsched(&attr, 0) == 0);
    Require(scePthreadAttrSetinheritsched(&attr, 4) == 0);
    for (const int unsupportedInherit : {-1, 1, 2, 3, 5, 99}) {
        Require(scePthreadAttrSetinheritsched(&attr, unsupportedInherit) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    }

    int inherit = 0;
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == 0);
    Require(inherit == 4);

    Require(scePthreadAttrSetsolosched(&attr, 0) == 0);
    Require(scePthreadAttrSetsolosched(&attr, 16) == 0);
    for (const int unsupportedSolo : {-1, 1, 8, 32, 99}) {
        Require(scePthreadAttrSetsolosched(&attr, unsupportedSolo) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    }

    int solo = 0;
    Require(scePthreadAttrGetsolosched(&attr, &solo) == 0);
    Require(solo == 16);

    Require(scePthreadAttrSetschedparam(&attr, nullptr) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    Require(scePthreadAttrGetschedparam(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    Require(pthread_attr_setinheritsched_nid_postfix(&attr, 99) == PosixEnotsup);
    Require(pthread_attr_setschedpolicy_nid_postfix(&attr, 99) == PosixEnotsup);
    Require(pthread_attr_setschedparam_nid_postfix(&attr, nullptr) == PosixEnotsup);

    const KernelSchedParam outOfRange{150};
    Require(pthread_attr_setschedparam_nid_postfix(&attr, &outOfRange) == PosixEnotsup);

    const KernelSchedParam valid{620};
    Require(pthread_attr_setschedparam_nid_postfix(&attr, &valid) == 0);
    KernelSchedParam readBack{};
    Require(scePthreadAttrGetschedparam(&attr, &readBack) == 0);
    Require(readBack.sched_priority == 620);
    readBack = {};
    Require(pthread_attr_getschedparam_nid_postfix(&attr, &readBack) == 0);
    Require(readBack.sched_priority == 620);
    Require(pthread_attr_getschedparam_nid_postfix(&attr, reinterpret_cast<KernelSchedParam*>(0x10)) == PosixEfault);

    Require(scePthreadAttrDestroy(&attr) == 0);
}

void TestCpuAffinityMaskBoundaries() {
    KernelCpumask mask = 0;
    Require(scePthreadGetaffinity(nullptr, &mask) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadGetaffinity(scePthreadSelf(), nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadGetaffinity(scePthreadSelf(), &mask) == 0);
    Require(mask != 0);
    const KernelCpumask originalAffinity = mask;

    Require(scePthreadSetaffinity(nullptr, mask) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadSetaffinity(scePthreadSelf(), 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadSetaffinity(scePthreadSelf(), originalAffinity) == 0);

    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    Require(scePthreadAttrGetaffinity(nullptr, &mask) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetaffinity(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetaffinity(nullptr, 0x1) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetaffinity(&attr, 0x3) == 0);
    Require(scePthreadAttrGetaffinity(&attr, &mask) == 0);
    Require(mask == 0x3);
    Require(scePthreadAttrDestroy(&attr) == 0);

    Require(pthread_getaffinity_np_nid_postfix(nullptr, 8, &mask) == PosixEinval);
    Require(pthread_getaffinity_np_nid_postfix(scePthreadSelf(), 4, &mask) == PosixErange);
    Require(pthread_getaffinity_np_nid_postfix(scePthreadSelf(), 24, &mask) == PosixErange);
    Require(pthread_getaffinity_np_nid_postfix(scePthreadSelf(), 8, nullptr) == PosixEfault);
    Require(pthread_getaffinity_np_nid_postfix(scePthreadSelf(), 8, &mask) == 0);
    Require(mask == originalAffinity);

    std::array<std::uint8_t, 16> buf16{};
    Require(pthread_getaffinity_np_nid_postfix(scePthreadSelf(), 16, buf16.data()) == 0);

    Require(pthread_setaffinity_np_nid_postfix(nullptr, 8, &mask) == PosixEinval);
    Require(pthread_setaffinity_np_nid_postfix(scePthreadSelf(), 4, &mask) == PosixErange);
    Require(pthread_setaffinity_np_nid_postfix(scePthreadSelf(), 24, &mask) == PosixErange);
    Require(pthread_setaffinity_np_nid_postfix(scePthreadSelf(), 8, nullptr) == PosixEfault);

    std::array<std::uint8_t, 16> dirty{};
    dirty[0] = 1;
    dirty[8] = 1;
    Require(pthread_setaffinity_np_nid_postfix(scePthreadSelf(), 16, dirty.data()) == PosixEinval);

    const std::uint64_t zeroMask = 0;
    Require(pthread_setaffinity_np_nid_postfix(scePthreadSelf(), 8, &zeroMask) == PosixEdeadlk);

    Require(pthread_setaffinity_np_nid_postfix(scePthreadSelf(), 8, &originalAffinity) == 0);
}

void TestCpusetAffinityBoundaries() {
    std::uint64_t mask = 0;
    *__error_nid_postfix() = 0;
    Require(cpuset_getaffinity_nid_postfix(3, 1, -1, 4, &mask) == -1);
    Require(*__error_nid_postfix() == PosixErange);

    *__error_nid_postfix() = 0;
    Require(cpuset_getaffinity_nid_postfix(3, 1, -1, 24, &mask) == -1);
    Require(*__error_nid_postfix() == PosixErange);

    *__error_nid_postfix() = 0;
    Require(cpuset_getaffinity_nid_postfix(3, 1, -1, 8, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(cpuset_getaffinity_nid_postfix(3, 99, -1, 8, &mask) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(cpuset_getaffinity_nid_postfix(99, 1, -1, 8, &mask) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(cpuset_getaffinity_nid_postfix(3, 2, 999999, 8, &mask) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    *__error_nid_postfix() = 0;
    Require(cpuset_getaffinity_nid_postfix(3, 1, 999999, 8, &mask) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);

    std::uint64_t rootMask = 0;
    Require(cpuset_getaffinity_nid_postfix(1, 3, -1, 8, &rootMask) == 0);
    Require(rootMask == 0xffff);

    std::uint64_t availableMask = 0;
    Require(cpuset_getaffinity_nid_postfix(2, 3, -1, 8, &availableMask) == 0);
    Require(availableMask == sceKernelGetAvailableCpumask());

    std::uint64_t threadMask = 0;
    Require(cpuset_getaffinity_nid_postfix(3, 1, -1, 8, &threadMask) == 0);
    Require(threadMask != 0);

    *__error_nid_postfix() = 0;
    Require(cpuset_setaffinity_nid_postfix(3, 1, -1, 4, &threadMask) == -1);
    Require(*__error_nid_postfix() == PosixErange);

    *__error_nid_postfix() = 0;
    Require(cpuset_setaffinity_nid_postfix(3, 1, -1, 24, &threadMask) == -1);
    Require(*__error_nid_postfix() == PosixErange);

    *__error_nid_postfix() = 0;
    Require(cpuset_setaffinity_nid_postfix(3, 1, -1, 8, nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEfault);

    *__error_nid_postfix() = 0;
    Require(cpuset_setaffinity_nid_postfix(3, 99, -1, 8, &threadMask) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    *__error_nid_postfix() = 0;
    Require(cpuset_setaffinity_nid_postfix(99, 1, -1, 8, &threadMask) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    std::array<std::uint8_t, 16> dirtyBuf{};
    dirtyBuf[0] = 1;
    dirtyBuf[8] = 1;
    *__error_nid_postfix() = 0;
    Require(cpuset_setaffinity_nid_postfix(3, 1, -1, 16, dirtyBuf.data()) == -1);
    Require(*__error_nid_postfix() == PosixEinval);

    const std::uint64_t deadlkMask = 0;
    *__error_nid_postfix() = 0;
    Require(cpuset_setaffinity_nid_postfix(3, 1, -1, 8, &deadlkMask) == -1);
    Require(*__error_nid_postfix() == PosixEdeadlk);

    Require(cpuset_setaffinity_nid_postfix(3, 1, -1, 8, &threadMask) == 0);
}

} // namespace

int main() {
    TestThreadPriorityBounds();
    TestSchedulingPolicyBounds();
    TestProcessPriorityAndNice();
    TestRtPriorityAndYield();
    TestSchedParamBounds();
    TestPthreadAttrSchedulingBounds();
    TestCpuAffinityMaskBoundaries();
    TestCpusetAffinityBoundaries();
    return 0;
}
