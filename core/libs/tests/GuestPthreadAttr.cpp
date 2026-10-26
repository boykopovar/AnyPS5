#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int EXPLICIT_SCHED = 0;
static constexpr int INHERIT_SCHED = 4;
static constexpr int SCOPE_PROCESS = 0;
static constexpr int SCOPE_SYSTEM = 2;
static constexpr int DETACH_JOINABLE = 0;
static constexpr int CREATION_PRIORITY = 256;
static constexpr int UPDATED_PRIORITY = 767;
static constexpr KernelCpumask CREATION_AFFINITY = 0x3;
static constexpr KernelCpumask UPDATED_AFFINITY = 0x1000;
static constexpr std::size_t STACK_SIZE = 2u << 20;
static constexpr std::size_t MIN_STACK_SIZE = 16384;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ENOTSUP = 45;
static constexpr int DETACH_DETACHED = 1;
static constexpr int DETACHED_THREADS = 32;

static void Require(bool value) { if (!value) std::abort(); }

struct ReportedAttributes {
    int priority = 0;
    KernelCpumask affinity = 0;
    std::size_t stackSize = 0;
    int detachState = -1;
};

static ReportedAttributes Query(Pthread thread) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(thread, &attr) == SCE_OK);
    ReportedAttributes reported;
    KernelSchedParam param{};
    Require(scePthreadAttrGetschedparam(&attr, &param) == SCE_OK);
    reported.priority = param.sched_priority;
    Require(scePthreadAttrGetaffinity(&attr, &reported.affinity) == SCE_OK);
    Require(scePthreadAttrGetstacksize(&attr, &reported.stackSize) == SCE_OK);
    Require(scePthreadAttrGetdetachstate(&attr, &reported.detachState) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    return reported;
}

static void CheckStackSizeLimit() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    std::size_t stackSize = 0;
    Require(scePthreadAttrSetstacksize(&attr, MIN_STACK_SIZE) == SCE_OK);
    Require(scePthreadAttrGetstacksize(&attr, &stackSize) == SCE_OK && stackSize == MIN_STACK_SIZE);
    Require(scePthreadAttrSetstacksize(&attr, MIN_STACK_SIZE - 1) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetstacksize(&attr, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_attr_setstacksize_nid_postfix(&attr, MIN_STACK_SIZE - 1) == GUEST_EINVAL);
    Require(scePthreadAttrGetstacksize(&attr, &stackSize) == SCE_OK && stackSize == MIN_STACK_SIZE);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
}

static void CheckSoloSched() {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_setsolosched_np_nid_postfix(nullptr, 1) == GUEST_EINVAL);
    Require(pthread_attr_setsolosched_np_nid_postfix(&attr, 1) == GUEST_EINVAL);
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    int soloSched = -1;
    Require(scePthreadAttrGetsolosched(&attr, &soloSched) == SCE_OK && soloSched == 0);
    Require(pthread_attr_setsolosched_np_nid_postfix(&attr, 16) == 0);
    Require(scePthreadAttrGetsolosched(&attr, &soloSched) == SCE_OK && soloSched == 16);
    constexpr int GuestEopnotsupp = 45;
    constexpr int SceKernelErrorEopnotsupp = static_cast<int>(0x8002002Du);
    for (int rejected : {-1, 1, 2, 4, 8, 15, 17, 32, 100, 0x7fffffff}) {
        Require(pthread_attr_setsolosched_np_nid_postfix(&attr, rejected) == GuestEopnotsupp);
        Require(scePthreadAttrSetsolosched(&attr, rejected) == SceKernelErrorEopnotsupp);
        Require(scePthreadAttrGetsolosched(&attr, &soloSched) == SCE_OK && soloSched == 16);
    }
    Require(pthread_attr_setsolosched_np_nid_postfix(&attr, 0) == 0);
    Require(scePthreadAttrGetsolosched(&attr, &soloSched) == SCE_OK && soloSched == 0);
    PthreadAttr unset = nullptr;
    Require(scePthreadAttrSetsolosched(&unset, 16) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetsolosched(nullptr, 16) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetsolosched(&unset, &soloSched) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(pthread_attr_setsolosched_np_nid_postfix(&attr, 1) == GUEST_EINVAL);
}

static void CheckSchedArguments() {
    constexpr int SchedFifo = 1;
    constexpr int SchedOther = 2;
    constexpr int SchedRr = 3;
    PthreadAttr attr = nullptr;
    PthreadAttr unset = nullptr;
    KernelSchedParam param{CREATION_PRIORITY};
    Require(pthread_attr_setschedparam_nid_postfix(nullptr, &param) == GUEST_EINVAL);
    Require(pthread_attr_setschedparam_nid_postfix(&unset, &param) == GUEST_EINVAL);
    Require(pthread_attr_setschedpolicy_nid_postfix(&unset, SchedRr) == GUEST_EINVAL);
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(pthread_attr_setschedparam_nid_postfix(&attr, nullptr) == GUEST_ENOTSUP);
    for (const int policy : {SchedFifo, SchedRr}) {
        Require(pthread_attr_setschedpolicy_nid_postfix(&attr, policy) == 0);
        Require(pthread_attr_setschedparam_nid_postfix(&attr, &param) == 0);
        for (const int rejected : {0, 1, CREATION_PRIORITY - 1, UPDATED_PRIORITY + 1, 1000}) {
            KernelSchedParam outside{rejected};
            Require(pthread_attr_setschedparam_nid_postfix(&attr, &outside) == GUEST_ENOTSUP);
            KernelSchedParam reported{-1};
            Require(scePthreadAttrGetschedparam(&attr, &reported) == SCE_OK && reported.sched_priority == CREATION_PRIORITY);
        }
        KernelSchedParam highest{UPDATED_PRIORITY};
        Require(pthread_attr_setschedparam_nid_postfix(&attr, &highest) == 0);
        Require(pthread_attr_setschedparam_nid_postfix(&attr, &param) == 0);
    }
    Require(pthread_attr_setschedpolicy_nid_postfix(&attr, SchedOther) == 0);
    KernelSchedParam other{1000};
    Require(pthread_attr_setschedparam_nid_postfix(&attr, &other) == 0);
    int policy = -1;
    for (const int rejected : {-1, 0, 4, 100}) {
        Require(pthread_attr_setschedpolicy_nid_postfix(&attr, rejected) == GUEST_ENOTSUP);
        Require(pthread_attr_getschedpolicy_nid_postfix(&attr, &policy) == 0 && policy == SchedOther);
    }
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
}

static void* APS5_VABI Worker(void* arg) {
    static_cast<std::future<void>*>(arg)->get();
    return nullptr;
}

static void CheckSchedulingAttributes() {
    int inherit = -1;
    int scope = -1;
    Require(pthread_attr_getinheritsched_nid_postfix(nullptr, &inherit) == GUEST_EINVAL);
    Require(pthread_attr_setinheritsched_nid_postfix(nullptr, EXPLICIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_getscope_nid_postfix(nullptr, &scope) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(nullptr, SCOPE_SYSTEM) == GUEST_EINVAL);

    PthreadAttr attr = nullptr;
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == GUEST_EINVAL);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, EXPLICIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_SYSTEM) == GUEST_EINVAL);

    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == INHERIT_SCHED);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, 1) == GUEST_ENOTSUP);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, -1) == GUEST_ENOTSUP);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, EXPLICIT_SCHED) == 0);
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == EXPLICIT_SCHED);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, INHERIT_SCHED) == 0);
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == INHERIT_SCHED);

    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(pthread_attr_setscope_nid_postfix(&attr, 1) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, 3) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, -1) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_PROCESS) == 0);
    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == 0 && scope == SCOPE_PROCESS);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_SYSTEM) == 0);
    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);

    Require(pthread_attr_getscope_nid_postfix(&attr, nullptr) == GUEST_EINVAL);

    bool threw = false;
    try {
        pthread_attr_getinheritsched_nid_postfix(&attr, nullptr);

    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);

    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == GUEST_EINVAL);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, EXPLICIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_SYSTEM) == GUEST_EINVAL);
}

static void CheckThreadScope() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, EXPLICIT_SCHED) == 0);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_PROCESS) == 0);

    std::promise<void> release;
    auto released = release.get_future();
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Worker, &released, nullptr) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    PthreadAttr queryAttr = nullptr;
    Require(scePthreadAttrInit(&queryAttr) == SCE_OK);
    Require(scePthreadAttrGet(thread, &queryAttr) == SCE_OK);
    int scope = -1;
    Require(pthread_attr_getscope_nid_postfix(&queryAttr, &scope) == 0 && scope == SCOPE_PROCESS);

    Require(pthread_attr_get_np_nid_postfix(thread, &queryAttr) == 0);
    Require(pthread_attr_getscope_nid_postfix(&queryAttr, &scope) == 0 && scope == SCOPE_PROCESS);
    Require(scePthreadAttrDestroy(&queryAttr) == SCE_OK);

    release.set_value();
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, INHERIT_SCHED) == 0);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_PROCESS) == 0);

    std::promise<void> releaseInherited;
    auto releasedInherited = releaseInherited.get_future();
    Pthread inheritedThread = nullptr;
    Require(scePthreadCreate(&inheritedThread, &attr, Worker, &releasedInherited, nullptr) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    Require(scePthreadAttrInit(&queryAttr) == SCE_OK);
    Require(scePthreadAttrGet(inheritedThread, &queryAttr) == SCE_OK);
    scope = -1;
    Require(pthread_attr_getscope_nid_postfix(&queryAttr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(pthread_attr_get_np_nid_postfix(inheritedThread, &queryAttr) == 0);
    Require(pthread_attr_getscope_nid_postfix(&queryAttr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(scePthreadAttrDestroy(&queryAttr) == SCE_OK);

    releaseInherited.set_value();
    Require(scePthreadJoin(inheritedThread, nullptr) == SCE_OK);
}

static int QueryResult(Pthread thread, bool posix = false) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetstacksize(&attr, STACK_SIZE) == SCE_OK);
    *__error_nid_postfix() = 12345;
    const int result = posix ? pthread_attr_get_np_nid_postfix(thread, &attr) : scePthreadAttrGet(thread, &attr);
    if (posix) Require(*__error_nid_postfix() == 12345);
    std::size_t stackSize = 0;
    Require(scePthreadAttrGetstacksize(&attr, &stackSize) == SCE_OK);
    Require(result == SCE_OK || stackSize == STACK_SIZE);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    return result;
}

static void CheckPosixQuery(Pthread thread, int expected) {
    const int result = QueryResult(thread, true);
    if (result == expected) return;
    std::fprintf(stderr, "pthread_attr_get_np returned %d, expected %d\n", result, expected);
    std::abort();
}

static bool ReportedEnded(Pthread thread) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (QueryResult(thread) == SCE_OK) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return QueryResult(thread) == SCE_KERNEL_ERROR_ESRCH;
}

struct ExitQuery {
    PthreadKey key = -1;
    int result = -1;
    int posixResult = -1;
};

static void APS5_VABI QuerySelfAtExit(void* value) {
    auto* query = static_cast<ExitQuery*>(value);
    query->result = QueryResult(scePthreadSelf());
    query->posixResult = QueryResult(scePthreadSelf(), true);
}

static void* APS5_VABI SetExitQuery(void* arg) {
    auto* query = static_cast<ExitQuery*>(arg);
    Require(scePthreadSetspecific(query->key, query) == SCE_OK);
    return nullptr;
}

static void* APS5_VABI Finish(void*) {
    return nullptr;
}

static void CheckEndedThreads() {
    Require(QueryResult(scePthreadSelf()) == SCE_OK);
    CheckPosixQuery(scePthreadSelf(), 0);
    CheckPosixQuery(nullptr, GUEST_EINVAL);

    Pthread joined = nullptr;
    Require(scePthreadCreate(&joined, nullptr, Finish, nullptr, nullptr) == SCE_OK);
    Require(scePthreadJoin(joined, nullptr) == SCE_OK);
    Require(QueryResult(joined) == SCE_KERNEL_ERROR_ESRCH);
    CheckPosixQuery(joined, GUEST_ESRCH);
    PthreadAttr unset = nullptr;
    Require(scePthreadAttrGet(joined, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGet(joined, &unset) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_attr_get_np_nid_postfix(joined, nullptr) == GUEST_EINVAL);
    Require(pthread_attr_get_np_nid_postfix(joined, &unset) == GUEST_EINVAL);

    ExitQuery exitQuery;
    Require(scePthreadKeyCreate(&exitQuery.key, reinterpret_cast<pthread_key_destructor_func_t>(QuerySelfAtExit)) == SCE_OK);
    Pthread finished = nullptr;
    Require(scePthreadCreate(&finished, nullptr, SetExitQuery, &exitQuery, nullptr) == SCE_OK);
    Require(ReportedEnded(finished));
    CheckPosixQuery(finished, GUEST_ESRCH);
    Require(scePthreadJoin(finished, nullptr) == SCE_OK);
    Require(exitQuery.result == SCE_OK && exitQuery.posixResult == 0);
    Require(QueryResult(finished) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadKeyDelete(exitQuery.key) == SCE_OK);

    PthreadAttr detachedAttr = nullptr;
    Require(scePthreadAttrInit(&detachedAttr) == SCE_OK);
    Require(scePthreadAttrSetdetachstate(&detachedAttr, DETACH_DETACHED) == SCE_OK);
    for (int index = 0; index < DETACHED_THREADS; ++index) {
        Pthread detached = nullptr;
        Require(scePthreadCreate(&detached, &detachedAttr, Finish, nullptr, nullptr) == SCE_OK);
        Require(ReportedEnded(detached));
        CheckPosixQuery(detached, GUEST_ESRCH);
    }
    Require(scePthreadAttrDestroy(&detachedAttr) == SCE_OK);
}

int main() {
    CheckStackSizeLimit();
    CheckSoloSched();
    CheckSchedArguments();
    CheckSchedulingAttributes();
    CheckThreadScope();
    CheckEndedThreads();
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetinheritsched(&attr, EXPLICIT_SCHED) == SCE_OK);
    const KernelSchedParam requested{CREATION_PRIORITY};
    Require(scePthreadAttrSetschedparam(&attr, &requested) == SCE_OK);
    Require(scePthreadAttrSetaffinity(&attr, CREATION_AFFINITY) == SCE_OK);
    Require(scePthreadAttrSetstacksize(&attr, STACK_SIZE) == SCE_OK);

    std::promise<void> release;
    auto released = release.get_future();
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Worker, &released, nullptr) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    auto reported = Query(thread);
    Require(reported.priority == CREATION_PRIORITY);
    Require(reported.affinity == CREATION_AFFINITY);
    Require(reported.stackSize == STACK_SIZE);
    Require(reported.detachState == DETACH_JOINABLE);

    Require(scePthreadSetprio(thread, UPDATED_PRIORITY) == SCE_OK);
    Require(scePthreadSetaffinity(thread, UPDATED_AFFINITY) == SCE_OK);
    reported = Query(thread);
    Require(reported.priority == UPDATED_PRIORITY);
    Require(reported.affinity == UPDATED_AFFINITY);
    Require(reported.stackSize == STACK_SIZE);
    Require(reported.detachState == DETACH_JOINABLE);

    PthreadAttr destroyed = nullptr;
    Require(scePthreadAttrGet(thread, &destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGet(thread, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(nullptr, &attr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);

    release.set_value();
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    PthreadAttr stackAttr = nullptr;
    char stackMarker;
    void* reportedAddress = nullptr;
    std::size_t reportedSize = 0;
    Require(pthread_attr_setstack_nid_postfix(nullptr, &stackMarker, STACK_SIZE) == 22);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, &stackMarker, STACK_SIZE) == 22);
    Require(pthread_attr_getstackaddr_nid_postfix(nullptr, &reportedAddress) == 22);
    Require(pthread_attr_getstackaddr_nid_postfix(&stackAttr, &reportedAddress) == 22);
    Require(scePthreadAttrInit(&stackAttr) == SCE_OK);
    Require(pthread_attr_getstackaddr_nid_postfix(&stackAttr, nullptr) == 22);
    Require(pthread_attr_getstackaddr_nid_postfix(&stackAttr, &reportedAddress) == 0 && reportedAddress == nullptr);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, &stackMarker, STACK_SIZE) == 0);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, nullptr, STACK_SIZE) == 22);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, &stackMarker, 16383) == 22);
    Require(pthread_attr_getstack_nid_postfix(&stackAttr, &reportedAddress, &reportedSize) == 0);
    Require(reportedAddress == &stackMarker && reportedSize == STACK_SIZE);
    reportedAddress = nullptr;
    Require(pthread_attr_getstackaddr_nid_postfix(&stackAttr, &reportedAddress) == 0 && reportedAddress == &stackMarker);
    Require(scePthreadAttrDestroy(&stackAttr) == SCE_OK);
    Require(pthread_attr_setstack_nid_postfix(&stackAttr, &stackMarker, STACK_SIZE) == 22);
    Require(pthread_attr_getstackaddr_nid_postfix(&stackAttr, &reportedAddress) == 22);
}
