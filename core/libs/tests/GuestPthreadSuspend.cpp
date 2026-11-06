#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_EINVAL = 22;
static constexpr int ROUNDS = 40;
static constexpr int GUEST_STEPS = 20000;
static constexpr auto STOPPED_WINDOW = std::chrono::milliseconds(10);
static constexpr auto PROGRESS_LIMIT = std::chrono::seconds(5);
static void* const FINISHED = reinterpret_cast<void*>(std::uintptr_t{7});
static void* const CANCELED = reinterpret_cast<void*>(std::uintptr_t{1});

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

struct Worker {
    std::atomic<std::uint64_t> progress{0};
    std::atomic<bool> stop{false};
};

static void* APS5_VABI Record(void* opaque) {
    static_cast<Worker*>(opaque)->progress.fetch_add(1);
    return FINISHED;
}

static void* APS5_VABI Spin(void* opaque) {
    auto* worker = static_cast<Worker*>(opaque);
    while (!worker->stop.load()) worker->progress.fetch_add(1);
    return FINISHED;
}

static void* APS5_VABI Nap(void* opaque) {
    auto* worker = static_cast<Worker*>(opaque);
    while (!worker->stop.load()) {
        sceKernelUsleep_nid_postfix(200);
        worker->progress.fetch_add(1);
    }
    return FINISHED;
}

static void* APS5_VABI Rename(void* opaque) {
    auto* worker = static_cast<Worker*>(opaque);
    while (!worker->stop.load()) {
        Require(scePthreadRename(scePthreadSelf(), (worker->progress.load() & 1) != 0 ? "rename-a" : "rename-b") == SCE_OK, "scePthreadRename failed in the worker");
        for (int step = 0; step < GUEST_STEPS && !worker->stop.load(); ++step) worker->progress.fetch_add(1);
    }
    return FINISHED;
}

static void WaitForProgress(const Worker& worker, std::uint64_t after, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + PROGRESS_LIMIT;
    while (worker.progress.load() <= after) {
        Require(std::chrono::steady_clock::now() < deadline, message);
        std::this_thread::yield();
    }
}

static void ReportsErrors() {
    Require(pthread_suspend_np_nid_postfix(nullptr) == GUEST_EINVAL, "pthread_suspend_np accepted a null thread");
    Require(pthread_suspend_np_nid_postfix(scePthreadSelf()) == GUEST_EDEADLK, "pthread_suspend_np suspended the calling thread");
    Worker worker;
    Pthread joined = nullptr;
    Require(scePthreadCreate(&joined, nullptr, Record, &worker, "joined") == SCE_OK, "scePthreadCreate failed");
    Require(scePthreadJoin(joined, nullptr) == SCE_OK, "scePthreadJoin failed");
    Require(pthread_suspend_np_nid_postfix(joined) == GUEST_ESRCH, "pthread_suspend_np found a joined thread");
}

static void KeepsCreatedSuspendedThreadParked() {
    Worker worker;
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0, "pthread_attr_init failed");
    Require(pthread_attr_setcreatesuspend_np_nid_postfix(&attr) == 0, "pthread_attr_setcreatesuspend_np failed");
    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, &attr, Record, &worker) == 0, "pthread_create of a suspended thread failed");
    Require(pthread_attr_destroy_nid_postfix(&attr) == 0, "pthread_attr_destroy failed");
    Require(pthread_suspend_np_nid_postfix(thread) == 0, "pthread_suspend_np of a thread created suspended failed");
    std::this_thread::sleep_for(STOPPED_WINDOW);
    Require(worker.progress.load() == 0, "a thread created suspended ran after pthread_suspend_np");
    Require(pthread_resume_np_nid_postfix(thread) == 0, "pthread_resume_np failed");
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK && result == FINISHED && worker.progress.load() == 1, "the resumed thread did not run once");
}

static void SuspendsRunningThread(PthreadEntry entry, const char* name, bool readNameWhileSuspended) {
    Worker worker;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, entry, &worker, name) == SCE_OK, "scePthreadCreate failed");
    WaitForProgress(worker, 0, "the worker never started");
    for (int round = 0; round < ROUNDS; ++round) {
        Require(pthread_suspend_np_nid_postfix(thread) == 0, "pthread_suspend_np of a running thread failed");
        const std::uint64_t stopped = worker.progress.load();
        if (readNameWhileSuspended) {
            char current[32]{};
            Require(scePthreadGetname(thread, current) == SCE_OK, "scePthreadGetname failed while the worker was suspended");
        }
        std::this_thread::sleep_for(STOPPED_WINDOW);
        Require(worker.progress.load() == stopped, "a suspended thread kept running");
        Require(pthread_suspend_np_nid_postfix(thread) == 0, "suspending an already suspended thread failed");
        Require(pthread_resume_np_nid_postfix(thread) == 0, "pthread_resume_np failed");
        WaitForProgress(worker, stopped, "a resumed thread did not continue");
    }
    worker.stop.store(true);
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK && result == FINISHED, "joining the worker failed");
}

struct Locker {
    PthreadMutex mutex = nullptr;
    std::atomic<int> stage{0};
};

static void* APS5_VABI LockAfterRelease(void* opaque) {
    auto* locker = static_cast<Locker*>(opaque);
    locker->stage.store(1);
    Require(scePthreadMutexLock(&locker->mutex) == SCE_OK, "scePthreadMutexLock failed in the worker");
    locker->stage.store(2);
    Require(scePthreadMutexUnlock(&locker->mutex) == SCE_OK, "scePthreadMutexUnlock failed in the worker");
    return FINISHED;
}

static void* APS5_VABI SpinUntilCanceled(void* opaque) {
    auto* worker = static_cast<Worker*>(opaque);
    for (;;) {
        worker->progress.fetch_add(1);
        scePthreadTestcancel();
    }
}

static void KeepsBlockedThreadAfterItsWaitEnds() {
    Locker locker;
    Require(scePthreadMutexInit(&locker.mutex, nullptr, "suspend") == SCE_OK, "scePthreadMutexInit failed");
    Require(scePthreadMutexLock(&locker.mutex) == SCE_OK, "scePthreadMutexLock failed");
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, LockAfterRelease, &locker, "locker") == SCE_OK, "scePthreadCreate failed");
    while (locker.stage.load() == 0) std::this_thread::yield();
    std::this_thread::sleep_for(STOPPED_WINDOW);
    Require(pthread_suspend_np_nid_postfix(thread) == 0, "pthread_suspend_np of a thread blocked on a mutex failed");
    Require(scePthreadMutexUnlock(&locker.mutex) == SCE_OK, "scePthreadMutexUnlock failed");
    std::this_thread::sleep_for(STOPPED_WINDOW);
    Require(locker.stage.load() == 1, "a suspended thread took the mutex released while it was suspended");
    Require(pthread_resume_np_nid_postfix(thread) == 0, "pthread_resume_np failed");
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK && result == FINISHED && locker.stage.load() == 2, "the resumed thread did not take the mutex");
    Require(scePthreadMutexDestroy(&locker.mutex) == SCE_OK, "scePthreadMutexDestroy failed");
}

struct Churn {
    std::atomic<std::uint64_t> threads{0};
    std::atomic<bool> stop{false};
};

static void* APS5_VABI Finish(void*) {
    return FINISHED;
}

static void* APS5_VABI CreateAndJoin(void* opaque) {
    auto* churn = static_cast<Churn*>(opaque);
    while (!churn->stop.load()) {
        Pthread child = nullptr;
        Require(scePthreadCreate(&child, nullptr, Finish, nullptr, "churn-child") == SCE_OK, "scePthreadCreate failed in the churn");
        Require(scePthreadJoin(child, nullptr) == SCE_OK, "scePthreadJoin failed in the churn");
        churn->threads.fetch_add(1);
    }
    return FINISHED;
}

static void SuspendsWhileThreadsComeAndGo() {
    Churn churn;
    Pthread churner = nullptr;
    Require(scePthreadCreate(&churner, nullptr, CreateAndJoin, &churn, "churner") == SCE_OK, "scePthreadCreate failed");
    SuspendsRunningThread(Spin, "spin-churn", false);
    churn.stop.store(true);
    Require(scePthreadJoin(churner, nullptr) == SCE_OK, "joining the churner failed");
    Require(churn.threads.load() > 0, "no thread started and exited during the suspends");
}

static void DefersCancelUntilResume() {
    Worker worker;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, SpinUntilCanceled, &worker, "cancel") == SCE_OK, "scePthreadCreate failed");
    WaitForProgress(worker, 0, "the worker never started");
    Require(pthread_suspend_np_nid_postfix(thread) == 0, "pthread_suspend_np failed");
    const std::uint64_t stopped = worker.progress.load();
    Require(scePthreadCancel(thread) == SCE_OK, "scePthreadCancel of a suspended thread failed");
    std::this_thread::sleep_for(STOPPED_WINDOW);
    Require(worker.progress.load() == stopped, "a cancel woke a suspended thread");
    Require(pthread_resume_np_nid_postfix(thread) == 0, "pthread_resume_np failed");
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK && result == CANCELED, "a cancel requested while suspended did not take effect after resume");
}

static void SuspendsAdoptedThread() {
    Worker worker;
    std::atomic<Pthread> adopted{nullptr};
    std::thread host([&] {
        adopted.store(scePthreadSelf());
        while (!worker.stop.load()) worker.progress.fetch_add(1);
    });
    while (adopted.load() == nullptr) std::this_thread::yield();
    WaitForProgress(worker, 0, "the adopted thread never started");
    for (int round = 0; round < ROUNDS; ++round) {
        Require(pthread_suspend_np_nid_postfix(adopted.load()) == 0, "pthread_suspend_np of an adopted thread failed");
        const std::uint64_t stopped = worker.progress.load();
        std::this_thread::sleep_for(STOPPED_WINDOW);
        Require(worker.progress.load() == stopped, "a suspended adopted thread kept running");
        Require(pthread_resume_np_nid_postfix(adopted.load()) == 0, "pthread_resume_np of an adopted thread failed");
        WaitForProgress(worker, stopped, "a resumed adopted thread did not continue");
    }
    worker.stop.store(true);
    host.join();
}

static void SuspendsAdoptedThreadThatExits() {
    constexpr int EXIT_ROUNDS = 200;
    constexpr auto HOST_CALL = std::chrono::milliseconds(2);
    for (int round = 0; round < EXIT_ROUNDS; ++round) {
        std::atomic<Pthread> adopted{nullptr};
        std::thread host([&] {
            adopted.store(scePthreadSelf());
            std::this_thread::sleep_for(HOST_CALL);
        });
        while (adopted.load() == nullptr) std::this_thread::yield();
        const int result = pthread_suspend_np_nid_postfix(adopted.load());
        Require(result == 0 || result == GUEST_ESRCH, "pthread_suspend_np of an exiting adopted thread failed");
        if (result == 0) {
            const int resumed = pthread_resume_np_nid_postfix(adopted.load());
            Require(resumed == 0 || resumed == GUEST_ESRCH, "pthread_resume_np of an exiting adopted thread failed");
        }
        host.join();
    }
}

int main() {
    ReportsErrors();
    KeepsCreatedSuspendedThreadParked();
    SuspendsRunningThread(Spin, "spin", false);
    SuspendsRunningThread(Nap, "nap", false);
    SuspendsRunningThread(Rename, "rename", true);
    KeepsBlockedThreadAfterItsWaitEnds();
    SuspendsWhileThreadsComeAndGo();
    DefersCancelUntilResume();
    SuspendsAdoptedThread();
    SuspendsAdoptedThreadThatExits();
    std::puts("guest_pthread_suspend: ok");
    return 0;
}
