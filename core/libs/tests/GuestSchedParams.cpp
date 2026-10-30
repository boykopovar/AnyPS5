#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>

static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EFAULT = 14;
static constexpr int SCHED_FIFO_POLICY = 1;
static constexpr int SCHED_RR_POLICY = 3;

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "line %d failed, errno=%d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define REQUIRE(condition) Require(condition, __LINE__)

static bool FailsWith(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

static void CheckCallingThread() {
    int policy = -1;
    KernelSchedParam expected{};
    REQUIRE(pthread_getschedparam_nid_postfix(scePthreadSelf(), &policy, &expected) == 0);
    REQUIRE(sched_getscheduler_nid_postfix(0) == policy);
    KernelSchedParam reported{-1};
    REQUIRE(sched_getparam_nid_postfix(0, &reported) == 0 && reported.sched_priority == expected.sched_priority);

    for (const int priority : {256, 767, 500}) {
        const KernelSchedParam set{priority};
        REQUIRE(sched_setparam_nid_postfix(0, &set) == 0);
        REQUIRE(sched_getparam_nid_postfix(0, &reported) == 0 && reported.sched_priority == priority);
        int current = -1;
        KernelSchedParam threadParam{};
        REQUIRE(pthread_getschedparam_nid_postfix(scePthreadSelf(), &current, &threadParam) == 0);
        REQUIRE(current == policy && threadParam.sched_priority == priority);
        REQUIRE(sched_getscheduler_nid_postfix(0) == policy);
    }
    const KernelSchedParam roundRobin{600};
    REQUIRE(pthread_setschedparam_nid_postfix(scePthreadSelf(), SCHED_RR_POLICY, &roundRobin) == 0);
    REQUIRE(sched_getscheduler_nid_postfix(0) == SCHED_RR_POLICY);
    const KernelSchedParam lowered{300};
    REQUIRE(sched_setparam_nid_postfix(0, &lowered) == 0 && sched_getscheduler_nid_postfix(0) == SCHED_RR_POLICY);
    REQUIRE(sched_getparam_nid_postfix(0, &reported) == 0 && reported.sched_priority == 300);
    REQUIRE(pthread_setschedparam_nid_postfix(scePthreadSelf(), policy, &expected) == 0);
}

static void CheckTargets() {
    KernelSchedParam param{-1};
    const KernelSchedParam valid{500};
    const int self = getpid_nid_postfix();
    for (const int pid : {self + 1, -1, 99999}) {
        *__error_nid_postfix() = 0;
        REQUIRE(FailsWith(sched_getparam_nid_postfix(pid, &param), GUEST_ESRCH) && param.sched_priority == -1);
        *__error_nid_postfix() = 0;
        REQUIRE(FailsWith(sched_getscheduler_nid_postfix(pid), GUEST_ESRCH));
        *__error_nid_postfix() = 0;
        REQUIRE(FailsWith(sched_setparam_nid_postfix(pid, &valid), GUEST_ESRCH));
    }
    *__error_nid_postfix() = 0;
    REQUIRE(FailsWith(sched_getparam_nid_postfix(0, nullptr), GUEST_EFAULT));
    *__error_nid_postfix() = 0;
    REQUIRE(FailsWith(sched_getparam_nid_postfix(self + 1, nullptr), GUEST_ESRCH));
    *__error_nid_postfix() = 0;
    REQUIRE(FailsWith(sched_setparam_nid_postfix(0, nullptr), GUEST_EFAULT));
    *__error_nid_postfix() = 0;
    REQUIRE(FailsWith(sched_setparam_nid_postfix(self + 1, nullptr), GUEST_EFAULT));
}

static constexpr int WorkerPriority = 700;

struct Worker {
    int before = -1;
    int after = -1;
    int policy = -1;
};

static void* APS5_VABI WorkerEntry(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    KernelSchedParam param{};
    REQUIRE(sched_getparam_nid_postfix(0, &param) == 0);
    worker.before = param.sched_priority;
    const KernelSchedParam set{WorkerPriority};
    REQUIRE(sched_setparam_nid_postfix(0, &set) == 0);
    REQUIRE(sched_getparam_nid_postfix(0, &param) == 0);
    worker.after = param.sched_priority;
    worker.policy = sched_getscheduler_nid_postfix(0);
    return nullptr;
}

static void CheckOtherThread() {
    constexpr int callerPriority = 400;
    KernelSchedParam original{};
    REQUIRE(sched_getparam_nid_postfix(0, &original) == 0);
    const KernelSchedParam callerParam{callerPriority};
    REQUIRE(sched_setparam_nid_postfix(0, &callerParam) == 0);
    Worker worker;
    Pthread thread = nullptr;
    REQUIRE(scePthreadCreate(&thread, nullptr, WorkerEntry, &worker, "sched") == 0);
    REQUIRE(scePthreadJoin(thread, nullptr) == 0);
    REQUIRE(worker.after == WorkerPriority && (worker.policy == SCHED_FIFO_POLICY || worker.policy == SCHED_RR_POLICY));
    KernelSchedParam callerAfter{};
    REQUIRE(sched_getparam_nid_postfix(0, &callerAfter) == 0 && callerAfter.sched_priority == callerPriority);
    REQUIRE(sched_setparam_nid_postfix(0, &original) == 0);
}

static void CheckOutOfRange() {
    KernelSchedParam original{};
    REQUIRE(sched_getparam_nid_postfix(0, &original) == 0);
    const int policy = sched_getscheduler_nid_postfix(0);
    for (const int priority : {900, 959}) {
        REQUIRE(scePthreadSetprio(scePthreadSelf(), priority) == 0);
        KernelSchedParam reported{-1};
        REQUIRE(sched_getparam_nid_postfix(0, &reported) == 0 && reported.sched_priority == priority);
        REQUIRE(sched_getscheduler_nid_postfix(0) == policy);
    }
    const KernelSchedParam inRange{700};
    REQUIRE(sched_setparam_nid_postfix(0, &inRange) == 0);
    for (const int priority : {255, 768}) {
        const KernelSchedParam rejected{priority};
        *__error_nid_postfix() = 0;
        REQUIRE(FailsWith(sched_setparam_nid_postfix(0, &rejected), GUEST_EPERM));
        KernelSchedParam reported{-1};
        REQUIRE(sched_getparam_nid_postfix(0, &reported) == 0 && reported.sched_priority == 700);
    }
    REQUIRE(sched_setparam_nid_postfix(0, &original) == 0);
}

#ifndef __APPLE__
static void CheckOwnPid() {
    KernelSchedParam original{};
    REQUIRE(sched_getparam_nid_postfix(0, &original) == 0);
    const KernelSchedParam mainParam{600};
    REQUIRE(sched_setparam_nid_postfix(0, &mainParam) == 0);
    const int self = getpid_nid_postfix();
    const int policy = sched_getscheduler_nid_postfix(0);
    KernelSchedParam reported{-1};
    REQUIRE(sched_getparam_nid_postfix(self, &reported) == 0 && reported.sched_priority == 600);
    REQUIRE(sched_getscheduler_nid_postfix(self) == policy);
    REQUIRE(sched_setparam_nid_postfix(self, &original) == 0);
}
#endif

int main() {
    CheckCallingThread();
    CheckTargets();
    CheckOtherThread();
    CheckOutOfRange();
#ifndef __APPLE__
    CheckOwnPid();
#endif
    return 0;
}
