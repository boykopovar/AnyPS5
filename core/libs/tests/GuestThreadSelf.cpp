#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static bool StackContains(Pthread thread, const void* address) {
    PthreadAttr attr = nullptr;
    if (scePthreadAttrInit(&attr) != 0 || scePthreadAttrGet(thread, &attr) != 0) return false;
    void* stack = nullptr;
    size_t size = 0;
    const bool queried = scePthreadAttrGetstackaddr(&attr, &stack) == 0 && scePthreadAttrGetstacksize(&attr, &size) == 0;
    scePthreadAttrDestroy(&attr);

    const auto begin = reinterpret_cast<std::uintptr_t>(stack);
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    return queried && stack != nullptr && size != 0 && value >= begin && value - begin < size;
}

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_EINVAL = 22;
static constexpr int MUTEX_TYPE_RECURSIVE = 2;
static constexpr int GuestPthread_Cancel_Enable = 0;
static constexpr int GuestPthread_Cancel_Asynchronous = 2;
static constexpr std::intptr_t WorkerRetval = 0x1234;

static void Require(bool value) { if (!value) std::abort(); }

#ifdef _WIN32
static constexpr std::uintptr_t GuestPage = 0x4000;
static constexpr int GuestProtNone = 0;
static constexpr int GuestProtReadWrite = 3;

static void ReportedStack(Pthread thread, std::uintptr_t* begin, std::uintptr_t* end) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK && scePthreadAttrGet(thread, &attr) == SCE_OK);
    void* stack = nullptr;
    size_t size = 0;
    Require(scePthreadAttrGetstackaddr(&attr, &stack) == SCE_OK && scePthreadAttrGetstacksize(&attr, &size) == SCE_OK);
    scePthreadAttrDestroy(&attr);

    *begin = reinterpret_cast<std::uintptr_t>(stack);
    *end = *begin + size;
}

static void RequireGuardable(std::uintptr_t begin, std::uintptr_t end) {
    Require(begin % GuestPage == 0 && end - begin >= GuestPage);
    for (auto cursor = begin; cursor < end;) {
        MEMORY_BASIC_INFORMATION memory{};
        Require(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory));
        Require(memory.State == MEM_COMMIT && memory.Protect == PAGE_READWRITE);
        cursor = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    }
    auto* lowest = reinterpret_cast<void*>(begin);
    Require(mprotect_nid_postfix(lowest, GuestPage, GuestProtReadWrite) == 0);
    Require(mprotect_nid_postfix(lowest, GuestPage, GuestProtNone) == 0);
    Require(mprotect_nid_postfix(lowest, GuestPage, GuestProtReadWrite) == 0);
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
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
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
    Require(begin == low && end == high);
    return 0;
}

static void RunHostThread(LPTHREAD_START_ROUTINE routine, SIZE_T committed) {
    const HANDLE thread = CreateThread(nullptr, committed, routine, nullptr, 0, nullptr);
    Require(thread != nullptr);
    Require(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    Require(CloseHandle(thread) != 0);
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
    context.workerStackReported = StackContains(context.selfFromWorker, &local);
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
    Require(mainSelf != nullptr);
    Require(scePthreadSelf() == mainSelf);
    const std::int64_t mainTid = ThreadIdField(mainSelf);
    Require(mainTid != 0);
    Require(scePthreadGetthreadid() == static_cast<int>(mainTid));
    Require(pthread_getthreadid_np_nid_postfix() == static_cast<int>(mainTid));
    int local = 0;
    Require(StackContains(mainSelf, &local));
#ifdef _WIN32
    RequireGuardableStack(mainSelf);
    RunHostThread(DefaultSizeHostThread, 0);
    RunHostThread(PartlyCommittedHostThread, 0x10000);
#endif
    Require(scePthreadJoin(mainSelf, nullptr) == SCE_KERNEL_ERROR_EDEADLK);
    Require(pthread_join_nid_postfix(mainSelf, nullptr) == GUEST_EDEADLK);
    Require(scePthreadDetach(mainSelf) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadJoin(nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadDetach(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_join_nid_postfix(nullptr, nullptr) == GUEST_EINVAL);
    Require(pthread_detach_nid_postfix(nullptr) == GUEST_EINVAL);

    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrSettype(&attr, MUTEX_TYPE_RECURSIVE) == SCE_OK);
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, &attr, nullptr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);

    Require(scePthreadMutexLock(&mutex) == SCE_OK);

    WorkerContext context;
    context.mutex = &mutex;
    Require(scePthreadCreate(&context.thread, nullptr, Worker, &context, nullptr) == SCE_OK);
    Require(context.thread != nullptr);
    Require(context.thread != mainSelf);

    void* result = nullptr;
    Require(scePthreadJoin(context.thread, &result) == SCE_OK);
    Require(reinterpret_cast<std::intptr_t>(result) == WorkerRetval);
    Require(context.selfFromWorker != nullptr);
    Require(context.selfFromWorker == context.thread);
    Require(context.selfFromWorker != mainSelf);
    Require(context.workerTid != 0 && context.workerTid != mainTid);
    Require(context.workerThreadId == static_cast<int>(context.workerTid));
    Require(context.workerPosixThreadId == context.workerThreadId && context.workerPosixThreadId != static_cast<int>(mainTid));
    Require(context.unlockResult == SCE_KERNEL_ERROR_EPERM);
    Require(context.sceJoinSelf == SCE_KERNEL_ERROR_EDEADLK);
    Require(context.posixJoinSelf == GUEST_EDEADLK);
    Require(context.workerStackReported);
    Require(context.testcancelReturned);

    scePthreadTestcancel();
    pthread_testcancel_nid_postfix();
    Require(scePthreadMutexUnlock(&mutex) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);

    char name[32] = {'x'};
    pthread_set_name_np_nid_postfix(mainSelf, "q2-main");

    Require(scePthreadGetname(mainSelf, name) == SCE_OK && std::strcmp(name, "q2-main") == 0);
    pthread_set_name_np_nid_postfix(mainSelf, nullptr);

    Require(scePthreadGetname(mainSelf, name) == SCE_OK && name[0] == '\0');
    pthread_set_name_np_nid_postfix(nullptr, "ignored");

}
