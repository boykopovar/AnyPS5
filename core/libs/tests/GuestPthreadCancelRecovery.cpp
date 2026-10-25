#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int CANCEL_ENABLE = 0;
static constexpr int CANCEL_DISABLE = 1;
static constexpr int CANCEL_DEFERRED = 0;
static constexpr int KERNEL_EINVAL = static_cast<int>(0x80020016);
static constexpr KernelUseconds WAIT_MICROSECONDS = 60000000;
static void* const CANCELED = reinterpret_cast<void*>(std::uintptr_t{1});

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), location.line());
    std::abort();
}

static PthreadSem sem = nullptr;
static void* posixSem = nullptr;
static PthreadMutex mutex = nullptr;
static PthreadCond cond = nullptr;
static PthreadKey key = -1;
static Pthread target = nullptr;
static std::atomic<bool> ready{false}, release{false}, targetRelease{false};
static std::atomic<int> events{0};
static std::array<int, 4> order{};
static bool usePosix = false, useTimed = false, disabled = false;
static bool conditionReleased = false, returned = false;
static int stage = 0, normalResult = 0, sentinel = 0;
static void* joinResult = &sentinel;

static void Signal(std::atomic<bool>& flag) {
    flag.store(true);
    flag.notify_all();
}

static void Gate() {
    Signal(ready);
    release.wait(false);
}

static Pthread Start(PthreadEntry entry) {
    ready = false;
    release = false;
    returned = false;
    events = 0;
    order = {};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, entry, nullptr, nullptr) == 0);
    ready.wait(false);
    return thread;
}

static void Join(Pthread thread, void* expected) {
    void* result = &sentinel;
    Require(scePthreadJoin(thread, &result) == 0);
    Require(result == expected);
}

static void APS5_VABI Record(void* value) {
    const auto index = events.fetch_add(1);
    Require(index < static_cast<int>(order.size()));
    order[index] = static_cast<int>(reinterpret_cast<std::uintptr_t>(value));
}

static void APS5_VABI CleanupWithPoints(void*) {
    Record(reinterpret_cast<void*>(std::uintptr_t{2}));
    scePthreadTestcancel();

    Require(sceKernelUsleep_nid_postfix(1) == 0);
    Record(reinterpret_cast<void*>(std::uintptr_t{3}));
}

static void APS5_VABI DestroySpecific(void* value) {
    Require(value == &normalResult);
    Record(reinterpret_cast<void*>(std::uintptr_t{4}));
}

static void WaitForToken() {
    if (usePosix) {
        Require((useTimed ? sem_reltimedwait_np_nid_postfix(&posixSem, WAIT_MICROSECONDS)
                          : sem_wait_nid_postfix(&posixSem)) == 0);
    } else {
        Require((useTimed ? scePthreadSemTimedwait(&sem, WAIT_MICROSECONDS) : scePthreadSemWait(&sem)) == 0);
    }
}

static int TokenCount() {
    int count = -1;
    Require((usePosix ? sem_getvalue_nid_postfix(&posixSem, &count) : scePthreadSemGetvalue(&sem, &count)) == 0);
    return count;
}

static void* APS5_VABI PendingToken(void*) {
    std::uintptr_t first[8], second[8];
    Require(scePthreadSetspecific(key, &normalResult) == 0);
    __pthread_cleanup_push_imp_nid_postfix(Record, reinterpret_cast<void*>(std::uintptr_t{1}), first);
    __pthread_cleanup_push_imp_nid_postfix(CleanupWithPoints, nullptr, second);

    Gate();
    WaitForToken();
    returned = true;
    return nullptr;
}

static void CheckDefaults() {
    int old = -1;
    Require(scePthreadSetcancelstate(CANCEL_ENABLE, &old) == 0 && old == CANCEL_ENABLE);
    Require(scePthreadSetcanceltype(CANCEL_DEFERRED, &old) == 0 && old == CANCEL_DEFERRED);
    scePthreadTestcancel();

}

static void* APS5_VABI ConsumeToken(void*) {
    CheckDefaults();
    WaitForToken();
    return &normalResult;
}

static void PendingTokenRecovery() {
    Require((usePosix ? sem_init_nid_postfix(&posixSem, 0, 1) : scePthreadSemInit(&sem, 0, 1, nullptr)) == 0);
    const auto thread = Start(PendingToken);
    Require(scePthreadCancel(thread) == 0);
    Require(scePthreadCancel(thread) == 0);
    Require(events == 0 && TokenCount() == 1);
    Signal(release);
    Join(thread, CANCELED);
    Require(!returned && events == 4);
    Require((order == std::array<int, 4>{2, 3, 1, 4}));
    Require(TokenCount() == 1);
    Pthread consumer = nullptr;
    Require(scePthreadCreate(&consumer, nullptr, ConsumeToken, nullptr, nullptr) == 0);
    Join(consumer, &normalResult);
    Require(TokenCount() == 0);
    Require((usePosix ? sem_destroy_nid_postfix(&posixSem) : scePthreadSemDestroy(&sem)) == 0);
}

static void APS5_VABI Unlock(void*) {
    Require(scePthreadMutexUnlock(&mutex) == 0);
    Record(reinterpret_cast<void*>(std::uintptr_t{1}));
}

static void* APS5_VABI ConditionWaiter(void*) {
    CheckDefaults();
    if (disabled) Require(scePthreadSetcancelstate(CANCEL_DISABLE, nullptr) == 0);
    Require(scePthreadMutexLock(&mutex) == 0);
    std::uintptr_t info[8];
    __pthread_cleanup_push_imp_nid_postfix(Unlock, nullptr, info);

    Signal(ready);
    while (!conditionReleased) Require(scePthreadCondTimedwait(&cond, &mutex, WAIT_MICROSECONDS) == 0);
    Require(scePthreadMutexUnlock(&mutex) == 0);
    __pthread_cleanup_pop_imp_nid_postfix(0);

    if (disabled) {
        stage = 1;
        scePthreadTestcancel();

        int old = -1;
        Require(scePthreadSetcancelstate(CANCEL_ENABLE, &old) == 0 && old == CANCEL_DISABLE);
        stage = 2;
        scePthreadTestcancel();

    }
    returned = true;
    return &normalResult;
}

static void ConditionRecovery() {
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == 0);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0);
    conditionReleased = false;
    stage = 0;
    const auto thread = Start(ConditionWaiter);
    Require(scePthreadMutexLock(&mutex) == 0);
    Require(scePthreadCancel(thread) == 0);
    Require(events == 0);
    if (disabled) {
        conditionReleased = true;
        Require(scePthreadCondSignal(&cond) == 0);
    }
    Require(scePthreadMutexUnlock(&mutex) == 0);
    Join(thread, CANCELED);
    Require(!returned);
    Require(disabled ? stage == 2 && events == 0 : events == 1 && order[0] == 1);
    disabled = false;
    conditionReleased = false;
    const auto next = Start(ConditionWaiter);
    Require(scePthreadMutexLock(&mutex) == 0);
    conditionReleased = true;
    Require(scePthreadCondSignal(&cond) == 0);
    Require(scePthreadMutexUnlock(&mutex) == 0);
    Join(next, &normalResult);
    Require(returned && events == 0);
    Require(scePthreadCondDestroy(&cond) == 0);
    Require(scePthreadMutexDestroy(&mutex) == 0);
}

enum class ConditionWaitMode { SceTimed, Sce, PosixTimed, Posix };

struct SignaledCondition {
    PthreadMutex mutex = nullptr;
    PthreadCond condition = nullptr;
    PthreadCond* slot = &condition;
    ConditionWaitMode mode = ConditionWaitMode::SceTimed;
    int lockCount = 1;
    bool cancellationDisabled = false;
    bool released = false;
    std::atomic<int> ready{0};
    std::array<int, 2> results{-1, -1};
    std::array<bool, 2> cleaned{};
};

struct SignaledConditionWorker {
    SignaledCondition* context;
    int index;
};

static void APS5_VABI UnlockSignaledCondition(void* value) {
    auto& worker = *static_cast<SignaledConditionWorker*>(value);
    auto& context = *worker.context;
    for (int level = 0; level < context.lockCount; ++level) Require(scePthreadMutexUnlock(&context.mutex) == 0);
    Require(scePthreadMutexUnlock(&context.mutex) == static_cast<int>(0x80020001));
    context.cleaned[worker.index] = true;
}

static void* APS5_VABI SignaledConditionWaiter(void* value) {
    auto& worker = *static_cast<SignaledConditionWorker*>(value);
    auto& context = *worker.context;
    if (context.cancellationDisabled) Require(scePthreadSetcancelstate(CANCEL_DISABLE, nullptr) == 0);
    for (int level = 0; level < context.lockCount; ++level) Require(scePthreadMutexLock(&context.mutex) == 0);
    std::uintptr_t cleanup[8];
    __pthread_cleanup_push_imp_nid_postfix(UnlockSignaledCondition, &worker, cleanup);

    context.ready.fetch_add(1);
    int result = 0;
    do {
        switch (context.mode) {
        case ConditionWaitMode::SceTimed:
            result = scePthreadCondTimedwait(context.slot, &context.mutex, 2000000);
            break;
        case ConditionWaitMode::Sce:
            result = scePthreadCondWait(context.slot, &context.mutex);
            break;
        case ConditionWaitMode::PosixTimed: {
            KernelTimespec deadline{};
            Require(clock_gettime_nid_postfix(0, &deadline) == 0);
            deadline.tv_sec += 2;
            result = pthread_cond_timedwait_nid_postfix(context.slot, &context.mutex, &deadline);
            break;
        }
        case ConditionWaitMode::Posix:
            result = pthread_cond_wait_nid_postfix(context.slot, &context.mutex);
            break;
        }
    } while (result == 0 && !context.released);
    context.results[worker.index] = result;
    __pthread_cleanup_pop_imp_nid_postfix(1);

    return nullptr;
}

static bool ConditionRegistered(Pthread thread) {
    const std::lock_guard lock(thread->cancelLock);
    return thread->cancelWait != nullptr;
}

static void InitializeSignaledCondition(SignaledCondition& context, bool recursive) {
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == 0);
    Require(scePthreadMutexattrSettype(&attr, recursive ? 2 : 1) == 0);
    Require(scePthreadMutexInit(&context.mutex, &attr, nullptr) == 0);
    Require(scePthreadMutexattrDestroy(&attr) == 0);
    context.lockCount = recursive ? 3 : 1;
    Require(scePthreadCondInit(context.slot, nullptr, nullptr) == 0);
}

static void CanceledConditionSignal(ConditionWaitMode mode, bool recursive) {
    SignaledCondition context;
    context.mode = mode;
    InitializeSignaledCondition(context, recursive);
    std::array<SignaledConditionWorker, 2> workers{{{&context, 0}, {&context, 1}}};
    std::array<Pthread, 2> threads{};
    for (int index = 0; index < 2; ++index) {
        Require(scePthreadCreate(&threads[index], nullptr, SignaledConditionWaiter, &workers[index], nullptr) == 0);
        while (context.ready.load() <= index) std::this_thread::yield();
        Require(scePthreadMutexLock(&context.mutex) == 0);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (!ConditionRegistered(threads[index]) && std::chrono::steady_clock::now() < deadline) {
            Require(scePthreadMutexUnlock(&context.mutex) == 0);
            std::this_thread::yield();
            Require(scePthreadMutexLock(&context.mutex) == 0);
        }
        Require(ConditionRegistered(threads[index]));
        Require(scePthreadMutexUnlock(&context.mutex) == 0);
    }
    Require(scePthreadMutexLock(&context.mutex) == 0);
    context.released = true;
    Require(scePthreadCondSignal(&context.condition) == 0);
    int selected = -1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (selected < 0 && std::chrono::steady_clock::now() < deadline) {
        for (int index = 0; index < 2; ++index) if (!ConditionRegistered(threads[index])) selected = index;
        std::this_thread::yield();
    }
    Require(selected >= 0);
    Require(scePthreadCancel(threads[selected]) == 0);
    Require(scePthreadMutexUnlock(&context.mutex) == 0);
    Join(threads[selected], CANCELED);
    Join(threads[1 - selected], nullptr);
    Require(context.results[selected] == -1 && context.results[1 - selected] == 0);
    Require(context.cleaned[0] && context.cleaned[1]);
    Require(scePthreadCondDestroy(&context.condition) == 0);
    Require(scePthreadMutexDestroy(&context.mutex) == 0);
}

static void DestroyedSignaledCondition(ConditionWaitMode mode, bool recursive, bool cancel, bool cancellationDisabled) {
    SignaledCondition context;
    context.mode = mode;
    context.cancellationDisabled = cancellationDisabled;
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const auto bytes = static_cast<std::size_t>(system.dwPageSize);
    context.slot = static_cast<PthreadCond*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(context.slot != nullptr);
#else
    const auto bytes = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    context.slot = static_cast<PthreadCond*>(mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(context.slot != MAP_FAILED);
#endif
    *context.slot = nullptr;
    InitializeSignaledCondition(context, recursive);
    const std::weak_ptr<TimedWait::Condition> retained = (*context.slot)->_cv;
    SignaledConditionWorker worker{&context, 0};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, SignaledConditionWaiter, &worker, nullptr) == 0);
    while (context.ready.load() == 0) std::this_thread::yield();
    Require(scePthreadMutexLock(&context.mutex) == 0);
    context.released = true;
    Require(scePthreadCondSignal(context.slot) == 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (ConditionRegistered(thread) && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(!ConditionRegistered(thread));
    Require(scePthreadCondDestroy(context.slot) == 0);
    Require(!retained.expired());
#ifdef _WIN32
    Require(VirtualFree(context.slot, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(context.slot, bytes) == 0);
#endif
    if (cancel) Require(scePthreadCancel(thread) == 0);
    Require(scePthreadMutexUnlock(&context.mutex) == 0);
    Join(thread, cancel && !cancellationDisabled ? CANCELED : nullptr);
    Require(context.cleaned[0]);
    Require(context.results[0] == (cancel && !cancellationDisabled ? -1 : 0));
    Require(retained.expired());
    Require(scePthreadMutexDestroy(&context.mutex) == 0);
}

static void* APS5_VABI HeldTarget(void*) {
    CheckDefaults();
    targetRelease.wait(false);
    return &normalResult;
}

static void* APS5_VABI PendingJoin(void*) {
    std::uintptr_t info[8];
    __pthread_cleanup_push_imp_nid_postfix(Record, reinterpret_cast<void*>(std::uintptr_t{1}), info);
    Gate();
    Require(scePthreadJoin(target, &joinResult) == 0);
    returned = true;
    return nullptr;
}

static void JoinRecovery() {
    targetRelease = false;
    joinResult = &sentinel;
    Require(scePthreadCreate(&target, nullptr, HeldTarget, nullptr, nullptr) == 0);
    const auto joiner = Start(PendingJoin);
    Require(scePthreadCancel(joiner) == 0);
    Signal(release);
    Join(joiner, CANCELED);
    Require(!returned && joinResult == &sentinel && events == 1 && order[0] == 1);
    Signal(targetRelease);
    Join(target, &normalResult);
    Require(scePthreadCreate(&target, nullptr, HeldTarget, nullptr, nullptr) == 0);
    Join(target, &normalResult);
}

int main() {
    int old = 17;
    Require(scePthreadSetcancelstate(CANCEL_DISABLE, nullptr) == 0);
    Require(scePthreadSetcancelstate(7, &old) == KERNEL_EINVAL && old == 17);
    Require(scePthreadSetcancelstate(CANCEL_ENABLE, &old) == 0 && old == CANCEL_DISABLE);
    old = 17;
    Require(scePthreadSetcanceltype(1, &old) == KERNEL_EINVAL && old == 17);
    Require(scePthreadSetcanceltype(CANCEL_DEFERRED, &old) == 0 && old == CANCEL_DEFERRED);
    Require(scePthreadKeyCreate(&key, reinterpret_cast<pthread_key_destructor_func_t>(DestroySpecific)) == 0);
    for (bool posix : {false, true}) {
        usePosix = posix;
        for (bool timed : {false, true}) {
            useTimed = timed;
            PendingTokenRecovery();
        }
    }
    Require(scePthreadKeyDelete(key) == 0);
    ConditionRecovery();
    for (const auto mode : {ConditionWaitMode::SceTimed, ConditionWaitMode::Sce, ConditionWaitMode::PosixTimed, ConditionWaitMode::Posix}) {
        for (bool recursive : {false, true}) {
            CanceledConditionSignal(mode, recursive);
            DestroyedSignaledCondition(mode, recursive, true, false);
            DestroyedSignaledCondition(mode, recursive, false, false);
            DestroyedSignaledCondition(mode, recursive, true, true);
        }
    }
    disabled = true;
    ConditionRecovery();
    JoinRecovery();
}
