#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

static constexpr int SCE_OK = 0;
static constexpr int ROUNDS = 20;
static constexpr int CALLER_ROUNDS = 30;
static constexpr int SPAWN_ROUNDS = 5;
static constexpr int MAX_CHILDREN = 16;
static constexpr int GUEST_STEPS = 20000;
static constexpr auto STOPPED_WINDOW = std::chrono::milliseconds(10);
static constexpr auto HOST_CALL = std::chrono::milliseconds(50);
static constexpr auto PROGRESS_LIMIT = std::chrono::seconds(5);
static void* const FINISHED = reinterpret_cast<void*>(std::uintptr_t{7});

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

static void* APS5_VABI Record(void* opaque) {
    static_cast<Worker*>(opaque)->progress.fetch_add(1);
    return FINISHED;
}

static void WaitForProgress(const Worker& worker, std::uint64_t after, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + PROGRESS_LIMIT;
    while (worker.progress.load() <= after) {
        Require(std::chrono::steady_clock::now() < deadline, message);
        std::this_thread::yield();
    }
}

static void SpinUntil(const std::atomic<int>& value, int expected) {
    while (value.load() < expected) {
    }
}

static Pthread Start(PthreadEntry entry, Worker& worker, const char* name) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, entry, &worker, name) == SCE_OK, "scePthreadCreate failed");
    return thread;
}

static void Finish(Pthread thread, Worker& worker) {
    worker.stop.store(true);
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK && result == FINISHED, "joining a worker failed");
}

static void SuspendsEveryOtherThread() {
    std::array<Worker, 3> workers;
    const std::array<Pthread, 3> threads{Start(Spin, workers[0], "spin"), Start(Nap, workers[1], "nap"), Start(Rename, workers[2], "rename")};
    for (auto& worker : workers) WaitForProgress(worker, 0, "a worker never started");
    for (int round = 0; round < ROUNDS; ++round) {
        pthread_suspend_all_np_nid_postfix();
        std::array<std::uint64_t, 3> stopped{};
        for (std::size_t index = 0; index < workers.size(); ++index) stopped[index] = workers[index].progress.load();
        std::this_thread::sleep_for(STOPPED_WINDOW);
        for (std::size_t index = 0; index < workers.size(); ++index) Require(workers[index].progress.load() == stopped[index], "a thread kept running after pthread_suspend_all_np");
        pthread_resume_all_np_nid_postfix();
        for (std::size_t index = 0; index < workers.size(); ++index) WaitForProgress(workers[index], stopped[index], "a thread did not continue after pthread_resume_all_np");
    }
    for (std::size_t index = 0; index < workers.size(); ++index) Finish(threads[index], workers[index]);
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

static void SingleAndMultiStopEveryone() {
    Worker spinner;
    const Pthread spin = Start(Spin, spinner, "spin");
    Locker locker;
    Require(scePthreadMutexInit(&locker.mutex, nullptr, "single") == SCE_OK, "scePthreadMutexInit failed");
    Require(scePthreadMutexLock(&locker.mutex) == SCE_OK, "scePthreadMutexLock failed");
    Pthread blocked = nullptr;
    Require(scePthreadCreate(&blocked, nullptr, LockAfterRelease, &locker, "locker") == SCE_OK, "scePthreadCreate failed");
    while (locker.stage.load() == 0) std::this_thread::yield();
    WaitForProgress(spinner, 0, "the spinner never started");
    std::this_thread::sleep_for(STOPPED_WINDOW);
    Require(pthread_single_np_nid_postfix() == 0, "pthread_single_np did not return 0");
    const std::uint64_t stopped = spinner.progress.load();
    Require(scePthreadMutexUnlock(&locker.mutex) == SCE_OK, "scePthreadMutexUnlock failed");
    std::this_thread::sleep_for(STOPPED_WINDOW);
    Require(spinner.progress.load() == stopped, "a thread kept running after pthread_single_np");
    Require(locker.stage.load() == 1, "a suspended thread took the mutex released while it was suspended");
    Require(pthread_multi_np_nid_postfix() == 0, "pthread_multi_np did not return 0");
    WaitForProgress(spinner, stopped, "a thread did not continue after pthread_multi_np");
    void* result = nullptr;
    Require(scePthreadJoin(blocked, &result) == SCE_OK && result == FINISHED && locker.stage.load() == 2, "the blocked thread did not take the mutex after pthread_multi_np");
    Require(scePthreadMutexDestroy(&locker.mutex) == SCE_OK, "scePthreadMutexDestroy failed");
    Finish(spin, spinner);
}

static void KeepsEarlierSuspensions() {
    Worker suspended;
    const Pthread early = Start(Spin, suspended, "early");
    WaitForProgress(suspended, 0, "the worker never started");
    Require(pthread_suspend_np_nid_postfix(early) == 0, "pthread_suspend_np failed");
    Worker created;
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0, "pthread_attr_init failed");
    Require(pthread_attr_setcreatesuspend_np_nid_postfix(&attr) == 0, "pthread_attr_setcreatesuspend_np failed");
    Pthread late = nullptr;
    Require(pthread_create_nid_postfix(&late, &attr, Record, &created) == 0, "pthread_create of a suspended thread failed");
    Require(pthread_attr_destroy_nid_postfix(&attr) == 0, "pthread_attr_destroy failed");
    const std::uint64_t stopped = suspended.progress.load();
    pthread_suspend_all_np_nid_postfix();
    std::this_thread::sleep_for(STOPPED_WINDOW);
    Require(suspended.progress.load() == stopped && created.progress.load() == 0, "a thread suspended earlier ran during pthread_suspend_all_np");
    pthread_resume_all_np_nid_postfix();
    WaitForProgress(suspended, stopped, "a thread suspended earlier did not continue after pthread_resume_all_np");
    void* result = nullptr;
    Require(scePthreadJoin(late, &result) == SCE_OK && result == FINISHED && created.progress.load() == 1, "a thread created suspended did not run after pthread_resume_all_np");
    Finish(early, suspended);
}

struct Callers {
    std::atomic<int> holding{0};
    std::atomic<int> done{0};
};

static void* APS5_VABI SuspendAllRepeatedly(void* opaque) {
    auto* callers = static_cast<Callers*>(opaque);
    for (int round = 0; round < CALLER_ROUNDS; ++round) {
        pthread_suspend_all_np_nid_postfix();
        Require(callers->holding.fetch_add(1) == 0, "two callers ran with every other thread suspended");
        for (int step = 0; step < GUEST_STEPS; ++step) {
        }
        callers->holding.fetch_sub(1);
        pthread_resume_all_np_nid_postfix();
    }
    callers->done.fetch_add(1);
    return FINISHED;
}

static void SerialisesConcurrentCallers() {
    Worker spinner;
    const Pthread spin = Start(Spin, spinner, "spin");
    WaitForProgress(spinner, 0, "the spinner never started");
    Callers callers;
    Pthread first = nullptr;
    Pthread second = nullptr;
    Require(scePthreadCreate(&first, nullptr, SuspendAllRepeatedly, &callers, "caller-a") == SCE_OK, "scePthreadCreate failed");
    Require(scePthreadCreate(&second, nullptr, SuspendAllRepeatedly, &callers, "caller-b") == SCE_OK, "scePthreadCreate failed");
    SpinUntil(callers.done, 2);
    Require(scePthreadJoin(first, nullptr) == SCE_OK && scePthreadJoin(second, nullptr) == SCE_OK, "joining the callers failed");
    const std::uint64_t after = spinner.progress.load();
    WaitForProgress(spinner, after, "the spinner did not continue after both callers finished");
    Finish(spin, spinner);
}

struct Spawner {
    Worker children;
    std::atomic<int> created{0};
    std::atomic<bool> stop{false};
    std::vector<Pthread> threads;
};

static void* APS5_VABI SpawnChildren(void* opaque) {
    auto* spawner = static_cast<Spawner*>(opaque);
    while (!spawner->stop.load() && spawner->created.load() < MAX_CHILDREN) {
        Pthread child = nullptr;
        Require(scePthreadCreate(&child, nullptr, Spin, &spawner->children, "child") == SCE_OK, "scePthreadCreate failed in the spawner");
        spawner->threads.push_back(child);
        spawner->created.fetch_add(1);
    }
    return FINISHED;
}

static void StopsThreadsCreatedMeanwhile() {
    Spawner spawner;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, SpawnChildren, &spawner, "spawner") == SCE_OK, "scePthreadCreate failed");
    for (int round = 0; round < SPAWN_ROUNDS; ++round) {
        pthread_suspend_all_np_nid_postfix();
        const std::uint64_t stopped = spawner.children.progress.load();
        const int created = spawner.created.load();
        std::this_thread::sleep_for(STOPPED_WINDOW);
        Require(spawner.children.progress.load() == stopped && spawner.created.load() == created, "a thread created during pthread_suspend_all_np kept running");
        pthread_resume_all_np_nid_postfix();
    }
    spawner.stop.store(true);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK, "joining the spawner failed");
    spawner.children.stop.store(true);
    for (const Pthread child : spawner.threads) Require(scePthreadJoin(child, nullptr) == SCE_OK, "joining a child failed");
}

struct Canceler {
    Worker* spinner = nullptr;
    std::atomic<int> state{0};
    std::atomic<bool> stoppedAtExit{false};
};

static void APS5_VABI ResumeAtExit(void* opaque) {
    auto* canceler = static_cast<Canceler*>(opaque);
    const std::uint64_t stopped = canceler->spinner->progress.load();
    std::this_thread::sleep_for(STOPPED_WINDOW);
    canceler->stoppedAtExit.store(canceler->spinner->progress.load() == stopped);
    pthread_resume_all_np_nid_postfix();
    canceler->state.store(2);
}

static void* APS5_VABI CancelThenSuspendAll(void* opaque) {
    auto* canceler = static_cast<Canceler*>(opaque);
    pthread_cleanup_push_nid_postfix(ResumeAtExit, canceler);
    Require(scePthreadCancel(scePthreadSelf()) == SCE_OK, "scePthreadCancel of the caller failed");
    canceler->state.store(1);
    pthread_suspend_all_np_nid_postfix();
    canceler->state.store(3);
    pthread_cleanup_pop_nid_postfix(0);
    return FINISHED;
}

static void DefersCancelUntilEveryoneIsSuspended() {
    Worker spinner;
    const Pthread spin = Start(Spin, spinner, "spin");
    WaitForProgress(spinner, 0, "the spinner never started");
    Canceler canceler;
    canceler.spinner = &spinner;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, CancelThenSuspendAll, &canceler, "canceler") == SCE_OK, "scePthreadCreate failed");
    SpinUntil(canceler.state, 2);
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK && result == GUEST_CANCELED, "a cancel pending during pthread_suspend_all_np did not take effect when it returned");
    Require(canceler.stoppedAtExit.load(), "the other threads were not all suspended when the cancel took effect");
    WaitForProgress(spinner, spinner.progress.load(), "the spinner did not continue after the cleanup handler resumed it");
    Finish(spin, spinner);
}

struct Exemption {
    Pthread holder = nullptr;
    std::atomic<int> holderState{0};
    std::atomic<int> suspenderState{0};
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> holderProgress{0};
};

static void* APS5_VABI HoldSlot(void* opaque) {
    auto* exemption = static_cast<Exemption*>(opaque);
    exemption->holderState.store(1);
    SpinUntil(exemption->suspenderState, 1);
    std::this_thread::sleep_for(HOST_CALL);
    pthread_suspend_all_np_nid_postfix();
    exemption->holderState.store(2);
    pthread_resume_all_np_nid_postfix();
    while (!exemption->stop.load()) exemption->holderProgress.fetch_add(1);
    return FINISHED;
}

static void* APS5_VABI SuspendHolder(void* opaque) {
    auto* exemption = static_cast<Exemption*>(opaque);
    SpinUntil(exemption->holderState, 1);
    exemption->suspenderState.store(1);
    Require(pthread_suspend_np_nid_postfix(exemption->holder) == 0, "pthread_suspend_np of the suspend-all caller failed");
    exemption->suspenderState.store(2);
    return FINISHED;
}

static void SuspendsCallerOnlyOutsideSuspendAll() {
    Exemption exemption;
    Require(scePthreadCreate(&exemption.holder, nullptr, HoldSlot, &exemption, "holder") == SCE_OK, "scePthreadCreate failed");
    Pthread suspender = nullptr;
    Require(scePthreadCreate(&suspender, nullptr, SuspendHolder, &exemption, "suspender") == SCE_OK, "scePthreadCreate failed");
    SpinUntil(exemption.suspenderState, 2);
    const std::uint64_t stopped = exemption.holderProgress.load();
    std::this_thread::sleep_for(STOPPED_WINDOW);
    Require(exemption.holderProgress.load() == stopped, "the suspend-all caller kept running after pthread_suspend_np returned");
    pthread_resume_all_np_nid_postfix();
    SpinUntil(exemption.holderState, 2);
    exemption.stop.store(true);
    Require(scePthreadJoin(exemption.holder, nullptr) == SCE_OK && scePthreadJoin(suspender, nullptr) == SCE_OK, "joining the threads failed");
}

int main() {
    SuspendsEveryOtherThread();
    SingleAndMultiStopEveryone();
    KeepsEarlierSuspensions();
    SerialisesConcurrentCallers();
    StopsThreadsCreatedMeanwhile();
    DefersCancelUntilEveryoneIsSuspended();
    SuspendsCallerOnlyOutsideSuspendAll();
    std::puts("guest_pthread_suspend_all: ok");
    return 0;
}
