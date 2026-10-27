#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static bool StackContains(Pthread thread, const void* address, std::uintptr_t* beginOut = nullptr, std::size_t* sizeOut = nullptr) {
    PthreadAttr attr = nullptr;
    if (scePthreadAttrInit(&attr) != 0 || scePthreadAttrGet(thread, &attr) != 0) return false;
    void* stack = nullptr;
    size_t size = 0;
    const bool queried = scePthreadAttrGetstackaddr(&attr, &stack) == 0 && scePthreadAttrGetstacksize(&attr, &size) == 0;
    scePthreadAttrDestroy(&attr);

    const auto begin = reinterpret_cast<std::uintptr_t>(stack);
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    if (beginOut) *beginOut = begin;
    if (sizeOut) *sizeOut = size;
    return queried && stack != nullptr && size != 0 && value >= begin && value - begin < size;
}

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_EINVAL = 22;
static constexpr int MUTEX_TYPE_RECURSIVE = 2;
static constexpr int GuestPthread_Cancel_Enable = 0;
static constexpr int GuestPthread_Cancel_Asynchronous = 2;
static constexpr std::intptr_t WorkerRetval = 0x1234;

static void Require(bool value, const char* check) {
    if (value) return;
    std::fprintf(stderr, "guest_thread_self: require failed: %s\n", check);
    std::fflush(nullptr);
    std::abort();
}

#define RequireCheck(expression) Require(static_cast<bool>(expression), #expression)

#ifdef _WIN32
static constexpr std::uintptr_t GuestPage = 0x4000;
static constexpr int GuestProtNone = 0;
static constexpr int GuestProtReadWrite = 3;

static void ReportedStack(Pthread thread, std::uintptr_t* begin, std::uintptr_t* end) {
    PthreadAttr attr = nullptr;
    RequireCheck(scePthreadAttrInit(&attr) == SCE_OK && scePthreadAttrGet(thread, &attr) == SCE_OK);
    void* stack = nullptr;
    size_t size = 0;
    RequireCheck(scePthreadAttrGetstackaddr(&attr, &stack) == SCE_OK && scePthreadAttrGetstacksize(&attr, &size) == SCE_OK);
    scePthreadAttrDestroy(&attr);

    *begin = reinterpret_cast<std::uintptr_t>(stack);
    *end = *begin + size;
}

static void RequireGuardable(std::uintptr_t begin, std::uintptr_t end) {
    RequireCheck(begin % GuestPage == 0 && end - begin >= GuestPage);
    for (auto cursor = begin; cursor < end;) {
        MEMORY_BASIC_INFORMATION memory{};
        RequireCheck(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory));
        RequireCheck(memory.State == MEM_COMMIT && memory.Protect == PAGE_READWRITE);
        cursor = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    }
    auto* lowest = reinterpret_cast<void*>(begin);
    RequireCheck(mprotect_nid_postfix(lowest, GuestPage, GuestProtReadWrite) == 0);
    RequireCheck(mprotect_nid_postfix(lowest, GuestPage, GuestProtNone) == 0);
    RequireCheck(mprotect_nid_postfix(lowest, GuestPage, GuestProtReadWrite) == 0);
}

static void RequireGuardableStack(Pthread thread) {
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;
    ReportedStack(thread, &begin, &end);
    RequireGuardable(begin, end);
}

static DWORD WINAPI DefaultSizeHostThread(void*) {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    RequireCheck(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    RequireGuardable(reinterpret_cast<std::uintptr_t>(start), reinterpret_cast<std::uintptr_t>(end));
    RequireGuardableStack(scePthreadSelf());
    return 0;
}

static DWORD WINAPI PartlyCommittedHostThread(void*) {
    ULONG_PTR low = 0;
    ULONG_PTR high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;
    ReportedStack(scePthreadSelf(), &begin, &end);
    RequireCheck(begin == low && end == high);
    return 0;
}

static void RunHostThread(LPTHREAD_START_ROUTINE routine, SIZE_T committed) {
    const HANDLE thread = CreateThread(nullptr, committed, routine, nullptr, 0, nullptr);
    RequireCheck(thread != nullptr);
    RequireCheck(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    RequireCheck(CloseHandle(thread) != 0);
}
#endif

static std::int64_t ThreadIdField(Pthread thread) {
    std::int64_t tid = 0;
    std::memcpy(&tid, static_cast<const void*>(thread), sizeof(tid));
    return tid;
}

struct WorkerContext {
    Pthread thread = nullptr;
    Pthread selfFromWorker = nullptr;
    std::int64_t workerTid = 0;
    int workerThreadId = 0;
    int workerPosixThreadId = 0;
    bool workerStackReported = false;
    std::uintptr_t workerLocal = 0;
    std::uintptr_t workerStackBegin = 0;
    std::size_t workerStackBytes = 0;
    PthreadMutex* mutex = nullptr;
    int unlockResult = 0;
    bool testcancelReturned = false;
    int sceJoinSelf = 0;
    int posixJoinSelf = 0;
};

static void* APS5_VABI Worker(void* arg) {
    auto& context = *static_cast<WorkerContext*>(arg);
    context.selfFromWorker = scePthreadSelf();
    context.workerTid = ThreadIdField(context.selfFromWorker);
    context.workerThreadId = scePthreadGetthreadid();
    context.workerPosixThreadId = pthread_getthreadid_np_nid_postfix();
    int local = 0;
    context.workerLocal = reinterpret_cast<std::uintptr_t>(&local);
    context.workerStackReported = StackContains(context.selfFromWorker, &local, &context.workerStackBegin, &context.workerStackBytes);
    context.unlockResult = scePthreadMutexUnlock(context.mutex);
    context.sceJoinSelf = scePthreadJoin(context.selfFromWorker, nullptr);
    context.posixJoinSelf = pthread_join_nid_postfix(context.selfFromWorker, nullptr);
    int oldState = -1;
    int oldType = -1;
    context.testcancelReturned = scePthreadSetcancelstate(GuestPthread_Cancel_Enable, &oldState) == SCE_OK &&
        scePthreadSetcanceltype(GuestPthread_Cancel_Asynchronous, &oldType) == SCE_OK;
    scePthreadTestcancel();
    pthread_testcancel_nid_postfix();

    context.testcancelReturned = context.testcancelReturned && oldState == GuestPthread_Cancel_Enable;
    scePthreadExit(reinterpret_cast<void*>(WorkerRetval));

    return nullptr;
}

int main() {
    const Pthread mainSelf = scePthreadSelf();
    RequireCheck(mainSelf != nullptr);
    RequireCheck(scePthreadSelf() == mainSelf);
    const std::int64_t mainTid = ThreadIdField(mainSelf);
    RequireCheck(mainTid != 0);
    RequireCheck(scePthreadGetthreadid() == static_cast<int>(mainTid));
    RequireCheck(pthread_getthreadid_np_nid_postfix() == static_cast<int>(mainTid));
    int local = 0;
    RequireCheck(StackContains(mainSelf, &local));
#ifdef _WIN32
    RequireGuardableStack(mainSelf);
    RunHostThread(DefaultSizeHostThread, 0);
    RunHostThread(PartlyCommittedHostThread, 0x10000);
#endif
    RequireCheck(scePthreadJoin(mainSelf, nullptr) == SCE_KERNEL_ERROR_EDEADLK);
    RequireCheck(pthread_join_nid_postfix(mainSelf, nullptr) == GUEST_EDEADLK);
    RequireCheck(scePthreadDetach(mainSelf) == SCE_KERNEL_ERROR_EINVAL);
    RequireCheck(scePthreadJoin(nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    RequireCheck(scePthreadDetach(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    RequireCheck(pthread_join_nid_postfix(nullptr, nullptr) == GUEST_EINVAL);
    RequireCheck(pthread_detach_nid_postfix(nullptr) == GUEST_EINVAL);

    PthreadMutexattr attr = nullptr;
    RequireCheck(scePthreadMutexattrInit(&attr) == SCE_OK);
    RequireCheck(scePthreadMutexattrSettype(&attr, MUTEX_TYPE_RECURSIVE) == SCE_OK);
    PthreadMutex mutex = nullptr;
    RequireCheck(scePthreadMutexInit(&mutex, &attr, nullptr) == SCE_OK);
    RequireCheck(scePthreadMutexattrDestroy(&attr) == SCE_OK);

    RequireCheck(scePthreadMutexLock(&mutex) == SCE_OK);

    WorkerContext context;
    context.mutex = &mutex;
    RequireCheck(scePthreadCreate(&context.thread, nullptr, Worker, &context, nullptr) == SCE_OK);
    RequireCheck(context.thread != nullptr);
    RequireCheck(context.thread != mainSelf);

    void* result = nullptr;
    RequireCheck(scePthreadJoin(context.thread, &result) == SCE_OK);
    std::fprintf(stderr,
        "guest_thread_self: after join result=0x%llx self=%p thread=%p main=%p tid=%lld workerTid=%lld workerThreadId=%d posixTid=%d unlock=0x%x sceJoin=0x%x posixJoin=%d stack=%d local=0x%llx begin=0x%llx size=0x%zx testcancel=%d\n",
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(result)),
        static_cast<void*>(context.selfFromWorker),
        static_cast<void*>(context.thread),
        static_cast<void*>(mainSelf),
        static_cast<long long>(mainTid),
        static_cast<long long>(context.workerTid),
        context.workerThreadId,
        context.workerPosixThreadId,
        static_cast<unsigned>(context.unlockResult),
        static_cast<unsigned>(context.sceJoinSelf),
        context.posixJoinSelf,
        context.workerStackReported ? 1 : 0,
        static_cast<unsigned long long>(context.workerLocal),
        static_cast<unsigned long long>(context.workerStackBegin),
        context.workerStackBytes,
        context.testcancelReturned ? 1 : 0);
    std::fflush(nullptr);
    RequireCheck(reinterpret_cast<std::intptr_t>(result) == WorkerRetval);
    RequireCheck(context.selfFromWorker != nullptr);
    RequireCheck(context.selfFromWorker == context.thread);
    RequireCheck(context.selfFromWorker != mainSelf);
    RequireCheck(context.workerTid != 0 && context.workerTid != mainTid);
    RequireCheck(context.workerThreadId == static_cast<int>(context.workerTid));
    RequireCheck(context.workerPosixThreadId == context.workerThreadId && context.workerPosixThreadId != static_cast<int>(mainTid));
    RequireCheck(context.unlockResult == SCE_KERNEL_ERROR_EPERM);
    RequireCheck(context.sceJoinSelf == SCE_KERNEL_ERROR_EDEADLK);
    RequireCheck(context.posixJoinSelf == GUEST_EDEADLK);
    RequireCheck(context.workerStackReported);
    RequireCheck(context.testcancelReturned);

    scePthreadTestcancel();
    pthread_testcancel_nid_postfix();
    RequireCheck(scePthreadMutexUnlock(&mutex) == SCE_OK);
    RequireCheck(scePthreadMutexDestroy(&mutex) == SCE_OK);

    char name[32] = {'x'};
    pthread_set_name_np_nid_postfix(mainSelf, "q2-main");

    RequireCheck(scePthreadGetname(mainSelf, name) == SCE_OK && std::strcmp(name, "q2-main") == 0);
    pthread_set_name_np_nid_postfix(mainSelf, nullptr);

    RequireCheck(scePthreadGetname(mainSelf, name) == SCE_OK && name[0] == '\0');
    pthread_set_name_np_nid_postfix(nullptr, "ignored");

}
