#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
void APS5_VABI scePthreadExit(void* retval);
int APS5_VABI scePthreadCancel(Pthread thread);
int APS5_VABI scePthreadSetcancelstate(int state, int* old_state);
int APS5_VABI scePthreadSetcanceltype(int type, int* old_type);
void APS5_VABI scePthreadTestcancel();
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondDestroy(PthreadCond* cond);
int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI scePthreadSemInit(PthreadSem* sem, int flag, unsigned int value, const char* name);
int APS5_VABI scePthreadSemDestroy(PthreadSem* sem);
int APS5_VABI scePthreadSemWait(PthreadSem* sem);
int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds);
int APS5_VABI pthread_cancel_nid_postfix(Pthread thread);
int APS5_VABI pthread_setcanceltype_nid_postfix(int type, int* old_type);
int APS5_VABI sem_init_nid_postfix(void* sem, int pshared, unsigned int value);
int APS5_VABI sem_destroy_nid_postfix(void* sem);
int APS5_VABI sem_wait_nid_postfix(void* sem);
void APS5_VABI __pthread_cleanup_push_imp_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info);
void APS5_VABI __pthread_cleanup_pop_imp_nid_postfix(int execute);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ESRCH = static_cast<int>(0x80020003);
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EINVAL = 22;
static constexpr int CANCEL_ENABLE = 0;
static constexpr int CANCEL_DISABLE = 1;
static constexpr int CANCEL_DEFERRED = 0;
static constexpr int CANCEL_ASYNCHRONOUS = 2;
static void* const CANCELED = reinterpret_cast<void*>(std::uintptr_t{1});

static void Require(bool value) { if (!value) std::abort(); }

struct Shared {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    PthreadSem sem = nullptr;
    void* posixSem = nullptr;
    Pthread target = nullptr;
    std::atomic<bool> ready{false};
    std::atomic<bool> release{false};
    std::atomic<bool> returned{false};
    std::atomic<bool> survived{false};
    std::atomic<int> cleanups{0};
    int order[2] = {0, 0};
};

static Shared shared;

static void APS5_VABI UnlockMutex(void*) {
    Require(scePthreadMutexUnlock(&shared.mutex) == SCE_OK);
    shared.cleanups.fetch_add(1);
}

static void APS5_VABI RecordOrder(void* arg) {
    shared.order[shared.cleanups.fetch_add(1)] = static_cast<int>(reinterpret_cast<std::intptr_t>(arg));
}

static void* APS5_VABI CondWaiter(void*) {
    std::uintptr_t info[8];
    Require(scePthreadMutexLock(&shared.mutex) == SCE_OK);
    __pthread_cleanup_push_imp_nid_postfix(UnlockMutex, nullptr, info);
    shared.ready.store(true);
    for (;;) Require(scePthreadCondWait(&shared.cond, &shared.mutex) == SCE_OK);
}

static void* APS5_VABI SemWaiter(void*) {
    shared.ready.store(true);
    scePthreadSemWait(&shared.sem);
    shared.returned.store(true);
    return nullptr;
}

static void* APS5_VABI PosixSemWaiter(void*) {
    shared.ready.store(true);
    sem_wait_nid_postfix(&shared.posixSem);
    shared.returned.store(true);
    return nullptr;
}

static void* APS5_VABI Idler(void*) {
    while (!shared.release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return nullptr;
}

static void* APS5_VABI Joiner(void*) {
    shared.ready.store(true);
    scePthreadJoin(shared.target, nullptr);
    shared.returned.store(true);
    return nullptr;
}

static void* APS5_VABI Sleeper(void*) {
    shared.ready.store(true);
    for (;;) sceKernelUsleep_nid_postfix(1000);
}

static void* APS5_VABI Disabled(void*) {
    int old = -1;
    Require(scePthreadSetcancelstate(CANCEL_DISABLE, &old) == SCE_OK && old == CANCEL_ENABLE);
    shared.ready.store(true);
    while (!shared.release.load()) {
        sceKernelUsleep_nid_postfix(1000);
        scePthreadTestcancel();
    }
    shared.survived.store(true);
    Require(scePthreadSetcancelstate(CANCEL_ENABLE, &old) == SCE_OK && old == CANCEL_DISABLE);
    scePthreadTestcancel();
    shared.returned.store(true);
    return nullptr;
}

static void* APS5_VABI Asynchronous(void*) {
    int old = -1;
    Require(scePthreadSetcancelstate(CANCEL_DISABLE, nullptr) == SCE_OK);
    Require(scePthreadSetcanceltype(CANCEL_ASYNCHRONOUS, &old) == SCE_OK && old == CANCEL_DEFERRED);
    shared.ready.store(true);
    while (!shared.release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    scePthreadSetcancelstate(CANCEL_ENABLE, nullptr);
    shared.returned.store(true);
    return nullptr;
}

static void* APS5_VABI Handlers(void*) {
    std::uintptr_t first[8];
    std::uintptr_t second[8];
    __pthread_cleanup_push_imp_nid_postfix(RecordOrder, reinterpret_cast<void*>(std::intptr_t{1}), first);
    __pthread_cleanup_push_imp_nid_postfix(RecordOrder, reinterpret_cast<void*>(std::intptr_t{2}), second);
    __pthread_cleanup_pop_imp_nid_postfix(0);
    __pthread_cleanup_push_imp_nid_postfix(RecordOrder, reinterpret_cast<void*>(std::intptr_t{3}), second);
    __pthread_cleanup_pop_imp_nid_postfix(1);
    scePthreadExit(reinterpret_cast<void*>(std::intptr_t{7}));
    return nullptr;
}

static Pthread Start(PthreadEntry entry) {
    shared.ready.store(false);
    shared.release.store(false);
    shared.returned.store(false);
    shared.cleanups.store(0);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, entry, nullptr, nullptr) == SCE_OK);
    while (!shared.ready.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    return thread;
}

static void JoinCanceled(Pthread thread) {
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK);
    Require(result == CANCELED);
    Require(!shared.returned.load());
}

int main() {
    Require(scePthreadCancel(nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(pthread_cancel_nid_postfix(nullptr) == GUEST_ESRCH);
    Require(scePthreadSetcancelstate(7, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadSetcanceltype(1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_setcanceltype_nid_postfix(1, nullptr) == GUEST_EINVAL);

    Require(scePthreadMutexInit(&shared.mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&shared.cond, nullptr, nullptr) == SCE_OK);
    Require(scePthreadSemInit(&shared.sem, 0, 0, nullptr) == SCE_OK);
    Require(sem_init_nid_postfix(&shared.posixSem, 0, 0) == 0);

    Pthread thread = Start(CondWaiter);
    Require(scePthreadMutexLock(&shared.mutex) == SCE_OK);
    Require(scePthreadCancel(thread) == SCE_OK);
    Require(shared.cleanups.load() == 0);
    Require(scePthreadMutexUnlock(&shared.mutex) == SCE_OK);
    JoinCanceled(thread);
    Require(shared.cleanups.load() == 1);
    Require(scePthreadMutexLock(&shared.mutex) == SCE_OK);
    Require(scePthreadMutexUnlock(&shared.mutex) == SCE_OK);

    thread = Start(SemWaiter);
    Require(scePthreadCancel(thread) == SCE_OK);
    JoinCanceled(thread);

    thread = Start(PosixSemWaiter);
    Require(pthread_cancel_nid_postfix(thread) == 0);
    JoinCanceled(thread);

    Require(scePthreadCreate(&shared.target, nullptr, Idler, nullptr, nullptr) == SCE_OK);
    thread = Start(Joiner);
    Require(scePthreadCancel(thread) == SCE_OK);
    JoinCanceled(thread);
    shared.release.store(true);
    Require(scePthreadJoin(shared.target, nullptr) == SCE_OK);

    thread = Start(Sleeper);
    Require(scePthreadCancel(thread) == SCE_OK);
    JoinCanceled(thread);

    shared.survived.store(false);
    thread = Start(Disabled);
    Require(scePthreadCancel(thread) == SCE_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    shared.release.store(true);
    JoinCanceled(thread);
    Require(shared.survived.load());

    thread = Start(Asynchronous);
    Require(scePthreadCancel(thread) == SCE_OK);
    shared.release.store(true);
    JoinCanceled(thread);

    shared.ready.store(true);
    shared.cleanups.store(0);
    void* result = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Handlers, nullptr, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, &result) == SCE_OK);
    Require(reinterpret_cast<std::intptr_t>(result) == 7);
    Require(shared.cleanups.load() == 2 && shared.order[0] == 3 && shared.order[1] == 1);

    Require(sem_destroy_nid_postfix(&shared.posixSem) == 0);
    Require(scePthreadSemDestroy(&shared.sem) == SCE_OK);
    Require(scePthreadCondDestroy(&shared.cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&shared.mutex) == SCE_OK);
}
