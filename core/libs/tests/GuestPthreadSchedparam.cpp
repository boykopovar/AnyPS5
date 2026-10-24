#include "SceTypes.hpp"
#include <cstdlib>
#include <cstdint>
#include <future>
#include <initializer_list>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf(void);
int APS5_VABI scePthreadGetschedparam(Pthread thread, int* policy, KernelSchedParam* param);
int APS5_VABI scePthreadSetschedparam(Pthread thread, int policy, const KernelSchedParam* param);
int APS5_VABI pthread_getschedparam_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param);
int APS5_VABI pthread_getprio_nid_postfix(Pthread thread);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI pthread_setschedparam_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetschedpolicy(const PthreadAttr* attr, int* policy);
int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCE_KERNEL_ERROR_ESRCH = static_cast<int>(0x80020003);
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_SCHED_FIFO = 1;
static constexpr int GUEST_SCHED_OTHER = 2;
static constexpr int GUEST_SCHED_RR = 3;
static constexpr int EXPLICIT_SCHED = 0;
static constexpr int INHERIT_SCHED = 4;

static void Require(bool value) { if (!value) std::abort(); }

static void RequireAttributes(Pthread thread, int policy, int priority) {
    *__error_nid_postfix() = 1234;
    Require(pthread_getprio_nid_postfix(thread) == priority);
    Require(*__error_nid_postfix() == 1234);
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    for (auto query : {scePthreadAttrGet, pthread_attr_get_np_nid_postfix}) {
        for (int previous : {GUEST_SCHED_FIFO, GUEST_SCHED_RR}) {
            Require(scePthreadAttrSetschedpolicy(&attr, previous) == SCE_OK);
            Require(query(thread, &attr) == SCE_OK);
            int current = -1;
            KernelSchedParam param{};
            Require(scePthreadAttrGetschedpolicy(&attr, &current) == SCE_OK);
            Require(scePthreadAttrGetschedparam(&attr, &param) == SCE_OK);
            Require(current == policy && param.sched_priority == priority);
        }
    }
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
}

static void RequireSched(int policy, int priority) {
    int current = -1;
    KernelSchedParam param{};
    Require(scePthreadGetschedparam(scePthreadSelf(), &current, &param) == SCE_OK);
    Require(current == policy && param.sched_priority == priority);
    current = -1;
    param.sched_priority = -1;
    Require(pthread_getschedparam_nid_postfix(scePthreadSelf(), &current, &param) == 0);
    Require(current == policy && param.sched_priority == priority);
    RequireAttributes(scePthreadSelf(), policy, priority);
}

static int SceSet(int policy, int priority) {
    const KernelSchedParam param{priority};
    return scePthreadSetschedparam(scePthreadSelf(), policy, &param);
}

static int PosixSet(int policy, int priority) {
    const KernelSchedParam param{priority};
    return pthread_setschedparam_nid_postfix(scePthreadSelf(), policy, &param);
}

static void* APS5_VABI Run(void*) {
    int policy = -1;
    KernelSchedParam param{};
    Require(scePthreadGetschedparam(scePthreadSelf(), &policy, &param) == SCE_OK);
    Require(policy == GUEST_SCHED_FIFO);

    for (int priority : {256, 767, 896, 959}) {
        Require(SceSet(GUEST_SCHED_FIFO, priority) == SCE_OK);
        RequireSched(GUEST_SCHED_FIFO, priority);
    }
    for (int priority : {-1, 0, 255, 768, 895, 960, 1023, 4096}) {
        Require(SceSet(GUEST_SCHED_FIFO, priority) == SCE_KERNEL_ERROR_EINVAL);
        Require(PosixSet(GUEST_SCHED_FIFO, priority) == GUEST_EINVAL);
        RequireSched(GUEST_SCHED_FIFO, 959);
    }

    Require(SceSet(GUEST_SCHED_RR, 300) == SCE_OK);
    RequireSched(GUEST_SCHED_RR, 300);
    for (int other : {-1, 0, GUEST_SCHED_OTHER, 4}) {
        Require(SceSet(other, 700) == SCE_KERNEL_ERROR_EINVAL);
        Require(PosixSet(other, 700) == GUEST_EINVAL);
        RequireSched(GUEST_SCHED_RR, 300);
    }
    Require(PosixSet(GUEST_SCHED_FIFO, 700) == 0);
    RequireSched(GUEST_SCHED_FIFO, 700);
    Require(scePthreadSetschedparam(scePthreadSelf(), GUEST_SCHED_FIFO, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    return nullptr;
}

struct PolicyRequest {
    std::promise<int> ready;
    std::future<void> released;
};

static void* APS5_VABI ReportPolicy(void* arg) {
    auto& request = *static_cast<PolicyRequest*>(arg);
    int policy = -1;
    KernelSchedParam param{};
    Require(scePthreadGetschedparam(scePthreadSelf(), &policy, &param) == SCE_OK);
    RequireAttributes(scePthreadSelf(), policy, param.sched_priority);
    request.ready.set_value(policy);
    request.released.wait();
    return nullptr;
}

static int CreatedPolicy(int inheritSched, int policy) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetinheritsched(&attr, inheritSched) == SCE_OK);
    Require(scePthreadAttrSetschedpolicy(&attr, policy) == SCE_OK);
    std::promise<void> release;
    PolicyRequest request{{}, release.get_future()};
    auto ready = request.ready.get_future();
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, ReportPolicy, &request, "policy") == SCE_OK);
    const int reported = ready.get();
    int current = -1;
    KernelSchedParam param{};
    Require(scePthreadGetschedparam(thread, &current, &param) == SCE_OK);
    Require(current == reported);
    RequireAttributes(thread, reported, param.sched_priority);
    for (int updated : {GUEST_SCHED_RR, GUEST_SCHED_FIFO}) {
        const KernelSchedParam requested{450};
        Require(scePthreadSetschedparam(thread, updated, &requested) == SCE_OK);
        RequireAttributes(thread, updated, requested.sched_priority);
    }
    release.set_value();
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadAttrSetschedpolicy(&attr, GUEST_SCHED_RR) == SCE_OK);
    Require(scePthreadAttrGet(thread, &attr) == SCE_KERNEL_ERROR_ESRCH);
    current = -1;
    Require(scePthreadAttrGetschedpolicy(&attr, &current) == SCE_OK && current == GUEST_SCHED_RR);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    return reported;
}

static void* APS5_VABI ReportPriority(void* arg) {
    int policy = -1;
    KernelSchedParam param{};
    Require(scePthreadGetschedparam(scePthreadSelf(), &policy, &param) == SCE_OK);
    *static_cast<int*>(arg) = param.sched_priority;
    return nullptr;
}

static int CreatedPriority(const PthreadAttr* attr) {
    int reported = -1;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, attr, ReportPriority, &reported, "priority") == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    return reported;
}

static int CreatedPriority(int inheritSched, const KernelSchedParam* param) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetinheritsched(&attr, inheritSched) == SCE_OK);
    Require(scePthreadAttrSetschedpolicy(&attr, GUEST_SCHED_RR) == SCE_OK);
    if (param) Require(scePthreadAttrSetschedparam(&attr, param) == SCE_OK);
    const int reported = CreatedPriority(&attr);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    return reported;
}

static void* APS5_VABI Spawn(void*) {
    const KernelSchedParam param{300};
    for (int priority : {450, 600}) {
        Require(SceSet(GUEST_SCHED_FIFO, priority) == SCE_OK);
        Require(CreatedPriority(INHERIT_SCHED, &param) == priority);
        Require(CreatedPriority(INHERIT_SCHED, nullptr) == priority);
        Require(CreatedPriority(nullptr) == priority);
        Require(CreatedPriority(EXPLICIT_SCHED, &param) == 300);
        Require(CreatedPriority(EXPLICIT_SCHED, nullptr) == 700);
    }
    Require(SceSet(GUEST_SCHED_RR, 300) == SCE_OK);
    Require(CreatedPolicy(INHERIT_SCHED, GUEST_SCHED_FIFO) == GUEST_SCHED_RR);
    return nullptr;
}

int main() {
    *__error_nid_postfix() = 1234;
    Require(pthread_getprio_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(pthread_getprio_nid_postfix(reinterpret_cast<Pthread>(std::uintptr_t{1})) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Run, nullptr, "schedparam") == SCE_OK);
    const Pthread retired = thread;
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    *__error_nid_postfix() = 1234;
    Require(pthread_getprio_nid_postfix(retired) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
    Require(CreatedPolicy(EXPLICIT_SCHED, GUEST_SCHED_RR) == GUEST_SCHED_RR);
    Require(CreatedPolicy(EXPLICIT_SCHED, GUEST_SCHED_FIFO) == GUEST_SCHED_FIFO);
    Require(CreatedPolicy(INHERIT_SCHED, GUEST_SCHED_RR) == GUEST_SCHED_FIFO);
    Require(scePthreadCreate(&thread, nullptr, Spawn, nullptr, "spawn") == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
}
