#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadCancel(Pthread thread);
int APS5_VABI scePthreadSetcancelstate(int state, int* oldState);
int APS5_VABI scePthreadSetcanceltype(int type, int* oldType);
void APS5_VABI scePthreadTestcancel();
int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor);
int APS5_VABI scePthreadKeyDelete(PthreadKey key);
int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondDestroy(PthreadCond* cond);
int APS5_VABI scePthreadCondSignal(PthreadCond* cond);
int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec);
int APS5_VABI scePthreadSemInit(PthreadSem* sem, int flag, unsigned int value, const char* name);
int APS5_VABI scePthreadSemDestroy(PthreadSem* sem);
int APS5_VABI scePthreadSemWait(PthreadSem* sem);
int APS5_VABI scePthreadSemTimedwait(PthreadSem* sem, KernelUseconds usec);
int APS5_VABI scePthreadSemGetvalue(PthreadSem* sem, int* value);
int APS5_VABI sem_init_nid_postfix(void* sem, int pshared, unsigned int value);
int APS5_VABI sem_destroy_nid_postfix(void* sem);
int APS5_VABI sem_wait_nid_postfix(void* sem);
int APS5_VABI sem_reltimedwait_np_nid_postfix(void* sem, std::uint32_t usec);
int APS5_VABI sem_getvalue_nid_postfix(void* sem, int* value);
int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds);
void APS5_VABI __pthread_cleanup_push_imp_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info);
void APS5_VABI __pthread_cleanup_pop_imp_nid_postfix(int execute);
}

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
    disabled = true;
    ConditionRecovery();
    JoinRecovery();
}
