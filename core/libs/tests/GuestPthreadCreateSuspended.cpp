#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadCancel(Pthread thread);
void APS5_VABI scePthreadTestcancel();
int APS5_VABI pthread_attr_init_nid_postfix(PthreadAttr* attr);
int APS5_VABI pthread_attr_destroy_nid_postfix(PthreadAttr* attr);
int APS5_VABI pthread_attr_setdetachstate_nid_postfix(PthreadAttr* attr, int state);
int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI pthread_create_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg);
int APS5_VABI pthread_join_nid_postfix(Pthread thread, void** retval);
int APS5_VABI pthread_attr_setcreatesuspend_np_nid_postfix(PthreadAttr* attr);
int APS5_VABI pthread_resume_np_nid_postfix(Pthread thread);
void APS5_VABI pthread_resume_all_np_nid_postfix(void);
int APS5_VABI pthread_multi_np_nid_postfix(void);
int APS5_VABI scePthreadAttrSetcreatesuspend(PthreadAttr* attr);
int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler);
int APS5_VABI sceKernelRemoveExceptionHandler(int signum);
int APS5_VABI sceKernelRaiseException(Pthread thread, int signum);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_SIGUSR1 = 30;
static constexpr int GUEST_CREATE_DETACHED = 1;
static constexpr int DETACHED_ROUNDS = 200;
static constexpr auto SUSPENDED_WINDOW = std::chrono::milliseconds(50);
static void* const CANCELED = reinterpret_cast<void*>(std::uintptr_t{1});
static void* const FINISHED = reinterpret_cast<void*>(std::uintptr_t{7});

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

struct Started {
    std::atomic<bool> ran{false};
    std::atomic<bool> returned{false};
};

static std::atomic<bool> release{false};
static std::atomic<bool> stopResuming{false};
static std::atomic<int> raised{0};
static std::atomic<Pthread> raisedOn{nullptr};

static void* APS5_VABI Record(void* opaque) {
    static_cast<Started*>(opaque)->ran.store(true);
    return FINISHED;
}

static void* APS5_VABI CancelAfterResume(void* opaque) {
    auto* started = static_cast<Started*>(opaque);
    started->ran.store(true);
    scePthreadTestcancel();
    started->returned.store(true);
    return FINISHED;
}

static void* APS5_VABI CancelInResumeAll(void* opaque) {
    auto* started = static_cast<Started*>(opaque);
    started->ran.store(true);
    Require(scePthreadCancel(scePthreadSelf()) == SCE_OK, "scePthreadCancel of the calling thread failed");
    pthread_resume_all_np_nid_postfix();
    started->returned.store(true);
    return FINISHED;
}

static void* APS5_VABI ResumeAllUntilStopped(void*) {
    while (!stopResuming.load()) pthread_resume_all_np_nid_postfix();
    return FINISHED;
}

static void* APS5_VABI WaitForRelease(void*) {
    while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return FINISHED;
}

static void APS5_VABI Handler(int, void*) {
    raisedOn.store(scePthreadSelf());
    raised.fetch_add(1);
}

static void CreateSuspendedPosix(Pthread* thread, PthreadEntry entry, void* arg) {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0, "pthread_attr_init failed");
    Require(pthread_attr_setcreatesuspend_np_nid_postfix(&attr) == 0, "pthread_attr_setcreatesuspend_np failed");
    Require(pthread_create_nid_postfix(thread, &attr, entry, arg) == 0, "pthread_create of a suspended thread failed");
    Require(pthread_attr_destroy_nid_postfix(&attr) == 0, "pthread_attr_destroy failed");
}

static void CreateSuspendedSce(Pthread* thread, PthreadEntry entry, void* arg) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK, "scePthreadAttrInit failed");
    Require(scePthreadAttrSetcreatesuspend(&attr) == SCE_OK, "scePthreadAttrSetcreatesuspend failed");
    Require(scePthreadCreate(thread, &attr, entry, arg, "suspended") == SCE_OK, "scePthreadCreate of a suspended thread failed");
    Require(scePthreadAttrDestroy(&attr) == SCE_OK, "scePthreadAttrDestroy failed");
}

static void ReportsErrors() {
    Require(pthread_attr_setcreatesuspend_np_nid_postfix(nullptr) == GUEST_EINVAL, "pthread_attr_setcreatesuspend_np accepted a null attribute");
    Require(scePthreadAttrSetcreatesuspend(nullptr) == SCE_KERNEL_ERROR_EINVAL, "scePthreadAttrSetcreatesuspend accepted a null attribute");
    PthreadAttr destroyed = nullptr;
    Require(scePthreadAttrInit(&destroyed) == SCE_OK, "scePthreadAttrInit failed");
    Require(scePthreadAttrDestroy(&destroyed) == SCE_OK, "scePthreadAttrDestroy failed");
    Require(pthread_attr_setcreatesuspend_np_nid_postfix(&destroyed) == GUEST_EINVAL, "pthread_attr_setcreatesuspend_np accepted a destroyed attribute");
    Require(scePthreadAttrSetcreatesuspend(&destroyed) == SCE_KERNEL_ERROR_EINVAL, "scePthreadAttrSetcreatesuspend accepted a destroyed attribute");
    Require(pthread_resume_np_nid_postfix(nullptr) == GUEST_EINVAL, "pthread_resume_np accepted a null thread");
    Pthread joined = nullptr;
    Started started;
    Require(scePthreadCreate(&joined, nullptr, Record, &started, "joined") == SCE_OK, "scePthreadCreate failed");
    Require(scePthreadJoin(joined, nullptr) == SCE_OK, "scePthreadJoin failed");
    Require(pthread_resume_np_nid_postfix(joined) == GUEST_ESRCH, "pthread_resume_np found a joined thread");
}

static void ResumesOneThread(bool sce) {
    Started started;
    Pthread thread = nullptr;
    if (sce) CreateSuspendedSce(&thread, Record, &started);
    else CreateSuspendedPosix(&thread, Record, &started);
    std::this_thread::sleep_for(SUSPENDED_WINDOW);
    Require(!started.ran.load(), "a thread created suspended ran before it was resumed");
    Require(pthread_resume_np_nid_postfix(thread) == 0, "resuming a suspended thread failed");
    void* result = nullptr;
    Require((sce ? scePthreadJoin(thread, &result) : pthread_join_nid_postfix(thread, &result)) == 0, "joining a resumed thread failed");
    Require(result == FINISHED && started.ran.load(), "a resumed thread did not run its start routine");
}

static void ResumesRunningAndCallingThreads() {
    Require(pthread_resume_np_nid_postfix(scePthreadSelf()) == 0, "pthread_resume_np of the calling thread failed");
    release.store(false);
    Pthread running = nullptr;
    Require(scePthreadCreate(&running, nullptr, WaitForRelease, nullptr, "running") == SCE_OK, "scePthreadCreate failed");
    Require(pthread_resume_np_nid_postfix(running) == 0, "pthread_resume_np of a running thread failed");
    release.store(true);
    void* result = nullptr;
    Require(scePthreadJoin(running, &result) == SCE_OK && result == FINISHED, "joining a running thread failed");
}

static void ResumesAllThreads(void (*resumeAll)()) {
    Started first;
    Started second;
    Pthread firstThread = nullptr;
    Pthread secondThread = nullptr;
    CreateSuspendedPosix(&firstThread, Record, &first);
    CreateSuspendedSce(&secondThread, Record, &second);
    std::this_thread::sleep_for(SUSPENDED_WINDOW);
    Require(!first.ran.load() && !second.ran.load(), "a thread created suspended ran before it was resumed");
    resumeAll();
    void* result = nullptr;
    Require(scePthreadJoin(firstThread, &result) == SCE_OK && result == FINISHED && first.ran.load(), "resuming all threads left the first one suspended");
    Require(scePthreadJoin(secondThread, &result) == SCE_OK && result == FINISHED && second.ran.load(), "resuming all threads left the second one suspended");
}

static void CancelsBeforeResume() {
    Started started;
    Pthread thread = nullptr;
    CreateSuspendedSce(&thread, CancelAfterResume, &started);
    Require(scePthreadCancel(thread) == SCE_OK, "scePthreadCancel of a suspended thread failed");
    Require(pthread_resume_np_nid_postfix(thread) == 0, "pthread_resume_np of a canceled thread failed");
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK, "joining a canceled thread failed");
    Require(result == CANCELED && started.ran.load() && !started.returned.load(), "a cancel requested while suspended did not take effect at the first cancellation point");
}

static void CancelsInResumeAll() {
    Started started;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, CancelInResumeAll, &started, "resume_all") == SCE_OK, "scePthreadCreate failed");
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK, "joining a canceled thread failed");
    Require(result == CANCELED && started.ran.load() && !started.returned.load(), "pthread_resume_all_np did not act on a pending cancel");
}

static void ReportsCreateSuspendedInAttrGet(int (APS5_VABI* get)(Pthread, PthreadAttr*)) {
    Started first;
    Started second;
    Pthread suspended = nullptr;
    CreateSuspendedPosix(&suspended, Record, &first);
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0, "pthread_attr_init failed");
    Require(get(suspended, &attr) == 0, "getting the attributes of a suspended thread failed");
    Pthread copy = nullptr;
    Require(pthread_create_nid_postfix(&copy, &attr, Record, &second) == 0, "pthread_create with the attributes of a suspended thread failed");
    std::this_thread::sleep_for(SUSPENDED_WINDOW);
    Require(!second.ran.load(), "a thread created with the attributes of a suspended thread ran before it was resumed");
    Require(pthread_resume_np_nid_postfix(copy) == 0 && pthread_resume_np_nid_postfix(suspended) == 0, "resuming a suspended thread failed");
    void* result = nullptr;
    Require(pthread_join_nid_postfix(copy, &result) == 0 && result == FINISHED && second.ran.load(), "joining a resumed thread failed");
    Require(pthread_join_nid_postfix(suspended, &result) == 0 && result == FINISHED && first.ran.load(), "joining a resumed thread failed");
    release.store(false);
    Pthread running = nullptr;
    Require(scePthreadCreate(&running, nullptr, WaitForRelease, nullptr, "running") == SCE_OK, "scePthreadCreate failed");
    Require(pthread_attr_setcreatesuspend_np_nid_postfix(&attr) == 0, "pthread_attr_setcreatesuspend_np failed");
    Require(get(running, &attr) == 0, "getting the attributes of a running thread failed");
    Started third;
    Pthread plain = nullptr;
    Require(pthread_create_nid_postfix(&plain, &attr, Record, &third) == 0, "pthread_create with the attributes of a running thread failed");
    Require(pthread_join_nid_postfix(plain, &result) == 0 && result == FINISHED && third.ran.load(), "a thread created with the attributes of a running thread did not run");
    release.store(true);
    Require(scePthreadJoin(running, nullptr) == SCE_OK, "joining a running thread failed");
    Require(pthread_attr_destroy_nid_postfix(&attr) == 0, "pthread_attr_destroy failed");
}

static void ResumesDetachedThreadsWhileTheyExit() {
    stopResuming.store(false);
    Pthread resumer = nullptr;
    Require(scePthreadCreate(&resumer, nullptr, ResumeAllUntilStopped, nullptr, "resumer") == SCE_OK, "scePthreadCreate failed");
    for (int round = 0; round < DETACHED_ROUNDS; ++round) {
        Started started;
        PthreadAttr attr = nullptr;
        Require(pthread_attr_init_nid_postfix(&attr) == 0, "pthread_attr_init failed");
        Require(pthread_attr_setcreatesuspend_np_nid_postfix(&attr) == 0, "pthread_attr_setcreatesuspend_np failed");
        Require(pthread_attr_setdetachstate_nid_postfix(&attr, GUEST_CREATE_DETACHED) == 0, "pthread_attr_setdetachstate failed");
        Pthread thread = nullptr;
        Require(pthread_create_nid_postfix(&thread, &attr, Record, &started) == 0, "pthread_create of a detached suspended thread failed");
        Require(pthread_attr_destroy_nid_postfix(&attr) == 0, "pthread_attr_destroy failed");
        int result = 0;
        while ((result = pthread_resume_np_nid_postfix(thread)) == 0) std::this_thread::yield();
        Require(result == GUEST_ESRCH && started.ran.load(), "a detached thread resumed while it exited did not run its start routine");
    }
    stopResuming.store(true);
    Require(scePthreadJoin(resumer, nullptr) == SCE_OK, "joining the resuming thread failed");
}

#ifdef _WIN32
static void RefusesRaiseWhileSuspended() {
    Require(sceKernelInstallExceptionHandler(GUEST_SIGUSR1, reinterpret_cast<void*>(&Handler)) == SCE_OK, "sceKernelInstallExceptionHandler failed");
    Started started;
    Pthread thread = nullptr;
    CreateSuspendedSce(&thread, Record, &started);
    bool refused = false;
    try {
        sceKernelRaiseException(thread, GUEST_SIGUSR1);
    } catch (const std::runtime_error& error) {
        refused = std::string(error.what()).find("suspended") != std::string::npos;
    }
    Require(refused, "sceKernelRaiseException did not refuse a thread still suspended at creation");
    Require(pthread_resume_np_nid_postfix(thread) == 0, "pthread_resume_np failed");
    Require(scePthreadJoin(thread, nullptr) == SCE_OK && started.ran.load(), "joining a resumed thread failed");
    Require(raised.load() == 0, "the handler ran for a refused raise");
    Require(sceKernelRemoveExceptionHandler(GUEST_SIGUSR1) == SCE_OK, "sceKernelRemoveExceptionHandler failed");
}
#else
static void DefersRaiseUntilResume() {
    Require(sceKernelInstallExceptionHandler(GUEST_SIGUSR1, reinterpret_cast<void*>(&Handler)) == SCE_OK, "sceKernelInstallExceptionHandler failed");
    Started started;
    Pthread thread = nullptr;
    CreateSuspendedSce(&thread, Record, &started);
    Require(sceKernelRaiseException(thread, GUEST_SIGUSR1) == SCE_OK, "sceKernelRaiseException on a suspended thread failed");
    std::this_thread::sleep_for(SUSPENDED_WINDOW);
    Require(raised.load() == 0, "the handler ran on a thread still suspended at creation");
    Require(pthread_resume_np_nid_postfix(thread) == 0, "pthread_resume_np failed");
    Require(scePthreadJoin(thread, nullptr) == SCE_OK && started.ran.load(), "joining a resumed thread failed");
    Require(raised.load() == 1 && raisedOn.load() == thread, "the handler did not run on the thread once it was resumed");
    Require(sceKernelRemoveExceptionHandler(GUEST_SIGUSR1) == SCE_OK, "sceKernelRemoveExceptionHandler failed");
}
#endif

int main() {
    ReportsErrors();
    ResumesOneThread(false);
    ResumesOneThread(true);
    ResumesRunningAndCallingThreads();
    ResumesAllThreads([] { Require(pthread_multi_np_nid_postfix() == 0, "pthread_multi_np failed"); });
    ResumesAllThreads([] { pthread_resume_all_np_nid_postfix(); });
    CancelsBeforeResume();
    CancelsInResumeAll();
    ReportsCreateSuspendedInAttrGet(pthread_attr_get_np_nid_postfix);
    ReportsCreateSuspendedInAttrGet(scePthreadAttrGet);
    ResumesDetachedThreadsWhileTheyExit();
#ifdef _WIN32
    RefusesRaiseWhileSuspended();
#else
    DefersRaiseUntilResume();
#endif
}
