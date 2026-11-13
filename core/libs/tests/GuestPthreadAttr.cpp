#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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
static constexpr int GUEST_EFAULT = 14;
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
    PthreadAttr unset = nullptr;
    Require(scePthreadAttrSetstacksize(nullptr, MIN_STACK_SIZE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetstacksize(&unset, MIN_STACK_SIZE) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    std::size_t stackSize = 0;
    Require(scePthreadAttrSetstacksize(&attr, MIN_STACK_SIZE) == SCE_OK);
    Require(scePthreadAttrGetstacksize(&attr, &stackSize) == SCE_OK && stackSize == MIN_STACK_SIZE);
    for (const std::size_t rejected : std::array<std::size_t, 6>{0, 1, 512, 4096, 8192, MIN_STACK_SIZE - 1}) {
        Require(scePthreadAttrSetstacksize(&attr, rejected) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetstacksize(&attr, &stackSize) == SCE_OK && stackSize == MIN_STACK_SIZE);
    }
    Require(pthread_attr_setstacksize_nid_postfix(&attr, MIN_STACK_SIZE - 1) == GUEST_EINVAL);
    Require(scePthreadAttrGetstacksize(&attr, &stackSize) == SCE_OK && stackSize == MIN_STACK_SIZE);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(scePthreadAttrSetstacksize(&attr, MIN_STACK_SIZE) == SCE_KERNEL_ERROR_EINVAL);
}

static void CheckGuardSize() {
    PthreadAttr attr = nullptr;
    std::size_t guardSize = 0;
    PthreadAttr unset = nullptr;
    Require(scePthreadAttrGetguardsize(nullptr, &guardSize) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetguardsize(&unset, &guardSize) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetguardsize(nullptr, 4096) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetguardsize(&unset, 4096) == SCE_KERNEL_ERROR_EINVAL);

    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGetguardsize(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetguardsize(&attr, &guardSize) == SCE_OK && guardSize == 4096);

    for (const std::size_t size : std::array<std::size_t, 5>{0, 4096, 8192, 16384, 65536}) {
        Require(scePthreadAttrSetguardsize(&attr, size) == SCE_OK);
        guardSize = 0;
        Require(scePthreadAttrGetguardsize(&attr, &guardSize) == SCE_OK && guardSize == size);
    }

    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(scePthreadAttrGetguardsize(&attr, &guardSize) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetguardsize(&attr, 4096) == SCE_KERNEL_ERROR_EINVAL);
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

static void CheckSchedPolicyAndStackGetters() {
    PthreadAttr unset = nullptr;
    PthreadAttr destroyed = nullptr;
    PthreadAttr live = nullptr;
    Require(scePthreadAttrInit(&destroyed) == SCE_OK);
    Require(scePthreadAttrDestroy(&destroyed) == SCE_OK);
    Require(scePthreadAttrInit(&live) == SCE_OK);
    for (PthreadAttr* invalid : {static_cast<PthreadAttr*>(nullptr), &unset, &destroyed}) {
        int policy = -1;
        KernelSchedParam param{-1};
        void* address = &policy;
        std::size_t size = 7;
        Require(pthread_attr_getschedpolicy_nid_postfix(invalid, &policy) == GUEST_EINVAL && policy == -1);
        Require(pthread_attr_getschedparam_nid_postfix(invalid, &param) == GUEST_EINVAL && param.sched_priority == -1);
        Require(pthread_attr_getstacksize_nid_postfix(invalid, &size) == GUEST_EINVAL && size == 7);
        Require(pthread_attr_getstackaddr_nid_postfix(invalid, &address) == GUEST_EINVAL && address == &policy);
        Require(pthread_attr_getstack_nid_postfix(invalid, &address, &size) == GUEST_EINVAL && address == &policy && size == 7);
    }
    int policy = -1;
    void* address = &policy;
    std::size_t size = 7;
    Require(pthread_attr_getschedpolicy_nid_postfix(&live, nullptr) == GUEST_EINVAL);
    Require(pthread_attr_getstackaddr_nid_postfix(&live, nullptr) == GUEST_EINVAL);
    Require(pthread_attr_getstack_nid_postfix(&live, nullptr, &size) == GUEST_EINVAL && size == 7);
    Require(pthread_attr_getstack_nid_postfix(&live, &address, nullptr) == GUEST_EINVAL && address == &policy);
    Require(scePthreadAttrDestroy(&live) == SCE_OK);
}

static void* APS5_VABI Worker(void* arg) {
    static_cast<std::future<void>*>(arg)->get();
    return nullptr;
}

static void CheckSchedulingAttributes() {
    int inherit = -1;
    int scope = -1;
    PthreadAttr unset = nullptr;
    Require(pthread_attr_getinheritsched_nid_postfix(nullptr, &inherit) == GUEST_EINVAL);
    Require(pthread_attr_setinheritsched_nid_postfix(nullptr, EXPLICIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_getscope_nid_postfix(nullptr, &scope) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(nullptr, SCOPE_SYSTEM) == GUEST_EINVAL);
    Require(scePthreadAttrGetinheritsched(nullptr, &inherit) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetinheritsched(&unset, &inherit) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetinheritsched(nullptr, EXPLICIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetinheritsched(&unset, EXPLICIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);

    PthreadAttr attr = nullptr;
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == GUEST_EINVAL);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, EXPLICIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_SYSTEM) == GUEST_EINVAL);
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetinheritsched(&attr, EXPLICIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);

    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGetinheritsched(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == INHERIT_SCHED);
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == INHERIT_SCHED);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, 1) == GUEST_ENOTSUP);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, -1) == GUEST_ENOTSUP);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, EXPLICIT_SCHED) == 0);
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == EXPLICIT_SCHED);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, INHERIT_SCHED) == 0);
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == 0 && inherit == INHERIT_SCHED);

    Require(scePthreadAttrSetinheritsched(&attr, EXPLICIT_SCHED) == SCE_OK);
    inherit = -1;
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == EXPLICIT_SCHED);
    Require(scePthreadAttrSetinheritsched(&attr, INHERIT_SCHED) == SCE_OK);
    inherit = -1;
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == INHERIT_SCHED);

    for (const int rejected : {-1, 1, 2, 3, 5, 8, 16, 100}) {
        Require(scePthreadAttrSetinheritsched(&attr, rejected) == SCE_KERNEL_ERROR_EOPNOTSUPP);
        inherit = -1;
        Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_OK && inherit == INHERIT_SCHED);
    }

    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(pthread_attr_setscope_nid_postfix(&attr, 1) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, 3) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, -1) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_PROCESS) == 0);
    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == 0 && scope == SCOPE_PROCESS);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_SYSTEM) == 0);
    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == 0 && scope == SCOPE_SYSTEM);

    Require(pthread_attr_getscope_nid_postfix(&attr, nullptr) == GUEST_EINVAL);

    Require(pthread_attr_getinheritsched_nid_postfix(&attr, nullptr) == GUEST_EINVAL);

    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(pthread_attr_getinheritsched_nid_postfix(&attr, &inherit) == GUEST_EINVAL);
    Require(pthread_attr_setinheritsched_nid_postfix(&attr, EXPLICIT_SCHED) == GUEST_EINVAL);
    Require(pthread_attr_getscope_nid_postfix(&attr, &scope) == GUEST_EINVAL);
    Require(pthread_attr_setscope_nid_postfix(&attr, SCOPE_SYSTEM) == GUEST_EINVAL);
    Require(scePthreadAttrGetinheritsched(&attr, &inherit) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetinheritsched(&attr, EXPLICIT_SCHED) == SCE_KERNEL_ERROR_EINVAL);
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
    int reportedInherit = -1;
    Require(pthread_attr_getinheritsched_nid_postfix(&queryAttr, &reportedInherit) == 0 && reportedInherit == EXPLICIT_SCHED);
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
    reportedInherit = -1;
    Require(pthread_attr_getinheritsched_nid_postfix(&queryAttr, &reportedInherit) == 0 && reportedInherit == INHERIT_SCHED);
    scope = -1;
    Require(pthread_attr_getscope_nid_postfix(&queryAttr, &scope) == 0 && scope == SCOPE_SYSTEM);
    Require(pthread_attr_setinheritsched_nid_postfix(&queryAttr, EXPLICIT_SCHED) == 0);
    Require(pthread_attr_get_np_nid_postfix(inheritedThread, &queryAttr) == 0);
    Require(pthread_attr_getscope_nid_postfix(&queryAttr, &scope) == 0 && scope == SCOPE_SYSTEM);
    reportedInherit = -1;
    Require(pthread_attr_getinheritsched_nid_postfix(&queryAttr, &reportedInherit) == 0 && reportedInherit == INHERIT_SCHED);
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

    Pthread adopted = nullptr;
    std::int64_t adoptedId = 0;
    std::thread([&adopted, &adoptedId] {
        adopted = scePthreadSelf();
        adoptedId = pthread_getthreadid_np_nid_postfix();
    }).join();
    Require(adopted != nullptr && ReportedEnded(adopted));
    CheckPosixQuery(adopted, GUEST_ESRCH);
    KernelCpumask adoptedMask = 0;
    *__error_nid_postfix() = 0;
    Require(cpuset_getaffinity_nid_postfix(3, 1, adoptedId, sizeof(adoptedMask), &adoptedMask) == -1 && *__error_nid_postfix() == GUEST_ESRCH);
}

static void CheckGetprioOfTheCallingThread() {
    const Pthread self = scePthreadSelf();
    int original = 0;
    Require(scePthreadGetprio(self, &original) == SCE_OK);
    int priority = 0;
    Require(scePthreadSetprio(self, UPDATED_PRIORITY) == SCE_OK);
    Require(scePthreadGetprio(self, &priority) == SCE_OK && priority == UPDATED_PRIORITY);
    Require(scePthreadSetprio(self, CREATION_PRIORITY) == SCE_OK);
    Require(scePthreadGetprio(self, &priority) == SCE_OK && priority == CREATION_PRIORITY);
    Require(scePthreadGetprio(self, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadGetprio(nullptr, &priority) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadSetprio(self, original) == SCE_OK);
    Require(scePthreadGetprio(self, &priority) == SCE_OK && priority == original);
}

static void CheckGetprioOfEndedThreads() {
    constexpr int UNTOUCHED = -77;
    int priority = UNTOUCHED;
    Pthread finished = nullptr;
    Require(scePthreadCreate(&finished, nullptr, Finish, nullptr, nullptr) == SCE_OK);
    Require(ReportedEnded(finished));
    Require(scePthreadGetprio(finished, &priority) == SCE_KERNEL_ERROR_ESRCH && priority == UNTOUCHED);
    Require(scePthreadJoin(finished, nullptr) == SCE_OK);
    Require(scePthreadGetprio(finished, &priority) == SCE_KERNEL_ERROR_ESRCH && priority == UNTOUCHED);
}

static void CheckSetStackSizeOnUnsetAttributes() {
    PthreadAttr unset = nullptr;
    PthreadAttr destroyed = nullptr;
    Require(scePthreadAttrInit(&destroyed) == SCE_OK);
    Require(scePthreadAttrDestroy(&destroyed) == SCE_OK);
    Require(pthread_attr_setstacksize_nid_postfix(nullptr, STACK_SIZE) == GUEST_EINVAL);
    Require(pthread_attr_setstacksize_nid_postfix(&unset, STACK_SIZE) == GUEST_EINVAL);
    Require(pthread_attr_setstacksize_nid_postfix(&destroyed, STACK_SIZE) == GUEST_EINVAL);
}

class GuardPage {
public:
    GuardPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        memory = static_cast<char*>(mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
        Require(mprotect(memory + page, page, PROT_NONE) == 0);
#endif
    }

    ~GuardPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 2 * page);
#endif
    }

    GuardPage(const GuardPage&) = delete;
    GuardPage& operator=(const GuardPage&) = delete;

    void* Unreadable() const { return memory + page; }

private:
    char* memory = nullptr;
    std::size_t page = 0;
};

static void CheckBadPointers() {
    GuardPage guard;
    void* const bad[] = {reinterpret_cast<void*>(0x10), guard.Unreadable()};
    for (void* pointer : bad) {
        auto* attrs = static_cast<PthreadAttr*>(pointer);
        auto* ints = static_cast<int*>(pointer);
        auto* sizes = static_cast<std::size_t*>(pointer);
        auto* params = static_cast<KernelSchedParam*>(pointer);
        int value = -1;
        std::size_t size = 1;
        KernelSchedParam param{};
        Require(pthread_attr_getdetachstate_nid_postfix(attrs, &value) == GUEST_EFAULT && value == -1);
        Require(pthread_attr_getguardsize_nid_postfix(attrs, &size) == GUEST_EFAULT && size == 1);
        Require(pthread_attr_getinheritsched_nid_postfix(attrs, &value) == GUEST_EFAULT && value == -1);
        Require(pthread_attr_getschedparam_nid_postfix(attrs, &param) == GUEST_EFAULT);
        Require(pthread_attr_getschedpolicy_nid_postfix(attrs, &value) == GUEST_EFAULT && value == -1);
        Require(pthread_attr_getscope_nid_postfix(attrs, &value) == GUEST_EFAULT && value == -1);
        Require(pthread_attr_getstacksize_nid_postfix(attrs, &size) == GUEST_EFAULT && size == 1);
        Require(pthread_attr_setdetachstate_nid_postfix(attrs, 0) == GUEST_EFAULT);
        Require(pthread_attr_setguardsize_nid_postfix(attrs, 0) == GUEST_EFAULT);
        Require(pthread_attr_setinheritsched_nid_postfix(attrs, EXPLICIT_SCHED) == GUEST_EFAULT);
        Require(pthread_attr_setschedparam_nid_postfix(attrs, &param) == GUEST_EFAULT);
        Require(pthread_attr_setschedpolicy_nid_postfix(attrs, 1) == GUEST_EFAULT);
        Require(pthread_attr_setscope_nid_postfix(attrs, SCOPE_SYSTEM) == GUEST_EFAULT);
        Require(pthread_attr_setstacksize_nid_postfix(attrs, STACK_SIZE) == GUEST_EFAULT);

        PthreadAttr attr = nullptr;
        Require(pthread_attr_init_nid_postfix(&attr) == 0);
        Require(pthread_attr_getdetachstate_nid_postfix(&attr, ints) == GUEST_EFAULT);
        Require(pthread_attr_getguardsize_nid_postfix(&attr, sizes) == GUEST_EFAULT);
        Require(pthread_attr_getinheritsched_nid_postfix(&attr, ints) == GUEST_EFAULT);
        Require(pthread_attr_getschedparam_nid_postfix(&attr, params) == GUEST_EFAULT);
        Require(pthread_attr_getschedpolicy_nid_postfix(&attr, ints) == GUEST_EFAULT);
        Require(pthread_attr_getscope_nid_postfix(&attr, ints) == GUEST_EFAULT);
        Require(pthread_attr_getstacksize_nid_postfix(&attr, sizes) == GUEST_EFAULT);
        Require(pthread_attr_setschedparam_nid_postfix(&attr, params) == GUEST_EFAULT);
        Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    }
}

int main() {
    CheckBadPointers();
    CheckGetprioOfTheCallingThread();
    CheckGetprioOfEndedThreads();
    CheckStackSizeLimit();
    CheckGuardSize();
    CheckSoloSched();
    CheckSchedArguments();
    CheckSchedulingAttributes();
    CheckSchedPolicyAndStackGetters();
    CheckThreadScope();
    CheckEndedThreads();
    CheckSetStackSizeOnUnsetAttributes();
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

    for (const int rejected : {-1, 0, 255, 768, 895, 960, 1023, 131840}) Require(scePthreadSetprio(thread, rejected) == SCE_KERNEL_ERROR_EINVAL);
    for (const int accepted : {256, 900, 959, 65792, -65280}) {
        Require(scePthreadSetprio(thread, accepted) == SCE_OK);
        Require(Query(thread).priority == accepted);
    }
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
