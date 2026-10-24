#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sched_getparam_nid_postfix(int pid, KernelSchedParam* param);
int APS5_VABI sched_getscheduler_nid_postfix(int pid);
int APS5_VABI sched_setparam_nid_postfix(int pid, const KernelSchedParam* param);
int APS5_VABI pthread_getschedparam_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param);
int APS5_VABI pthread_setschedparam_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI getpid_nid_postfix(void);
int* APS5_VABI __error_nid_postfix();
}

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

template <typename Call>
static bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

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
    for (const int priority : {255, 768, 895, 896, 959, 960, 0, -1}) {
        const KernelSchedParam outside{priority};
        REQUIRE(Throws([&] { sched_setparam_nid_postfix(0, &outside); }));
    }
    REQUIRE(sched_getparam_nid_postfix(0, &reported) == 0 && reported.sched_priority == 300);
    const KernelSchedParam upperBand{900};
    REQUIRE(pthread_setschedparam_nid_postfix(scePthreadSelf(), SCHED_RR_POLICY, &upperBand) == 0);
    REQUIRE(Throws([&] { sched_getparam_nid_postfix(0, &reported); }));
    REQUIRE(sched_getscheduler_nid_postfix(0) == SCHED_RR_POLICY);
    REQUIRE(pthread_setschedparam_nid_postfix(scePthreadSelf(), SCHED_RR_POLICY, &lowered) == 0);
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
    REQUIRE(Throws([&] { sched_getparam_nid_postfix(self, &param); }));
    REQUIRE(Throws([&] { sched_getscheduler_nid_postfix(self); }));
    REQUIRE(Throws([&] { sched_setparam_nid_postfix(self, &valid); }));
    *__error_nid_postfix() = 0;
    REQUIRE(FailsWith(sched_getparam_nid_postfix(0, nullptr), GUEST_EFAULT));
    *__error_nid_postfix() = 0;
    REQUIRE(FailsWith(sched_getparam_nid_postfix(self + 1, nullptr), GUEST_ESRCH));
    *__error_nid_postfix() = 0;
    REQUIRE(FailsWith(sched_setparam_nid_postfix(0, nullptr), GUEST_EFAULT));
    *__error_nid_postfix() = 0;
    REQUIRE(FailsWith(sched_setparam_nid_postfix(self + 1, nullptr), GUEST_EFAULT));
}

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
    const KernelSchedParam set{700};
    REQUIRE(sched_setparam_nid_postfix(0, &set) == 0);
    REQUIRE(sched_getparam_nid_postfix(0, &param) == 0);
    worker.after = param.sched_priority;
    worker.policy = sched_getscheduler_nid_postfix(0);
    return nullptr;
}

static void CheckOtherThread() {
    KernelSchedParam mainBefore{};
    REQUIRE(sched_getparam_nid_postfix(0, &mainBefore) == 0);
    Worker worker;
    Pthread thread = nullptr;
    REQUIRE(scePthreadCreate(&thread, nullptr, WorkerEntry, &worker, "sched") == 0);
    REQUIRE(scePthreadJoin(thread, nullptr) == 0);
    REQUIRE(worker.after == 700 && (worker.policy == SCHED_FIFO_POLICY || worker.policy == SCHED_RR_POLICY));
    KernelSchedParam mainAfter{};
    REQUIRE(sched_getparam_nid_postfix(0, &mainAfter) == 0 && mainAfter.sched_priority == mainBefore.sched_priority);
}

int main() {
    CheckCallingThread();
    CheckTargets();
    CheckOtherThread();
    return 0;
}
