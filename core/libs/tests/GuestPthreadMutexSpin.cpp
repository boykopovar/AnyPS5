#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int THREADS = 4;
static constexpr int ROUNDS = 2000;
static constexpr int TURNS = 300;

extern "C" std::uint64_t MutexYieldsForTest_nid_no_patch();

static void Require(bool value) { if (!value) std::abort(); }

struct Counts {
    int tries = 0;
    int pauses = 0;
    int yields = 0;
};

static bool Spin(int spins, int yields, int freeOnTry, Counts& counts) {
    return MutexOperations::SpinAcquire(spins, yields,
        [&] { return ++counts.tries == freeOnTry; },
        [&] { ++counts.pauses; },
        [&] { ++counts.yields; });
}

static void CheckSpinAcquire() {
    Counts counts;
    Require(!Spin(5, 3, 0, counts));
    Require(counts.tries == 9 && counts.pauses == 5 && counts.yields == 3);

    counts = {};
    Require(Spin(5, 3, 1, counts));
    Require(counts.tries == 1 && counts.pauses == 0 && counts.yields == 0);

    counts = {};
    Require(Spin(0, 4, 1, counts));
    Require(counts.tries == 1 && counts.pauses == 0 && counts.yields == 0);

    counts = {};
    Require(Spin(5, 3, 4, counts));
    Require(counts.tries == 4 && counts.pauses == 2 && counts.yields == 0);

    counts = {};
    Require(Spin(5, 3, 8, counts));
    Require(counts.tries == 8 && counts.pauses == 5 && counts.yields == 2);

    counts = {};
    Require(Spin(0, 2, 2, counts));
    Require(counts.tries == 2 && counts.pauses == 0 && counts.yields == 1);

    counts = {};
    Require(!Spin(0, 2, 0, counts));
    Require(counts.tries == 3 && counts.pauses == 0 && counts.yields == 2);

    counts = {};
    Require(!Spin(-3, -1, 0, counts));
    Require(counts.tries == 1 && counts.pauses == 0 && counts.yields == 0);
}

struct Shared {
    PthreadMutex mutex = nullptr;
    std::atomic<int> inside{0};
    int total = 0;
    bool overlapped = false;
};

static void* APS5_VABI Contend(void* arg) {
    auto& shared = *static_cast<Shared*>(arg);
    for (int round = 0; round < ROUNDS; ++round) {
        Require(scePthreadMutexLock(&shared.mutex) == SCE_OK);
        if (shared.inside.fetch_add(1) != 0)
            shared.overlapped = true;
        ++shared.total;
        shared.inside.fetch_sub(1);
        Require(scePthreadMutexUnlock(&shared.mutex) == SCE_OK);
    }
    return nullptr;
}

static void CheckContention(PthreadMutexattr* attr, int type, int spins, int yields) {
    Shared shared;
    Require(scePthreadMutexattrSettype(attr, type) == SCE_OK);
    Require(scePthreadMutexInit(&shared.mutex, attr, "spin") == SCE_OK);
    Require(scePthreadMutexSetspinloops(&shared.mutex, spins) == SCE_OK);
    Require(scePthreadMutexSetyieldloops(&shared.mutex, yields) == SCE_OK);
    Pthread threads[THREADS] = {};
    for (auto& thread : threads)
        Require(scePthreadCreate(&thread, nullptr, Contend, &shared, nullptr) == SCE_OK);
    for (auto& thread : threads)
        Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(!shared.overlapped && shared.total == THREADS * ROUNDS);
    Require(scePthreadMutexDestroy(&shared.mutex) == SCE_OK);
}

struct Ring {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<int> inside{0};
    std::atomic<int> nextId{0};
    int turn = 0;
    int total = 0;
    bool recursive = false;
    bool timed = false;
    bool overlapped = false;
};

static void* APS5_VABI TakeTurns(void* arg) {
    auto& ring = *static_cast<Ring*>(arg);
    const int id = ring.nextId.fetch_add(1);
    for (int round = 0; round < TURNS; ++round) {
        Require(scePthreadMutexLock(&ring.mutex) == SCE_OK);
        if (ring.recursive)
            Require(scePthreadMutexLock(&ring.mutex) == SCE_OK);
        while (ring.turn % THREADS != id)
            Require((ring.timed ? scePthreadCondTimedwait(&ring.cond, &ring.mutex, 10000000) : scePthreadCondWait(&ring.cond, &ring.mutex)) == SCE_OK);
        if (ring.inside.fetch_add(1) != 0)
            ring.overlapped = true;
        ++ring.turn;
        ++ring.total;
        ring.inside.fetch_sub(1);
        Require(scePthreadCondBroadcast(&ring.cond) == SCE_OK);
        if (ring.recursive)
            Require(scePthreadMutexUnlock(&ring.mutex) == SCE_OK);
        Require(scePthreadMutexUnlock(&ring.mutex) == SCE_OK);
    }
    return nullptr;
}

static void CheckCondRelock(PthreadMutexattr* attr, int type, int spins, int yields, bool timed) {
    Ring ring;
    ring.recursive = type == 2;
    ring.timed = timed;
    Require(scePthreadMutexattrSettype(attr, type) == SCE_OK);
    Require(scePthreadMutexInit(&ring.mutex, attr, "ring") == SCE_OK);
    Require(scePthreadMutexSetspinloops(&ring.mutex, spins) == SCE_OK);
    Require(scePthreadMutexSetyieldloops(&ring.mutex, yields) == SCE_OK);
    Require(scePthreadCondInit(&ring.cond, nullptr, "ring") == SCE_OK);
    Pthread threads[THREADS] = {};
    for (auto& thread : threads)
        Require(scePthreadCreate(&thread, nullptr, TakeTurns, &ring, nullptr) == SCE_OK);
    for (auto& thread : threads)
        Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(!ring.overlapped && ring.total == THREADS * TURNS && ring.turn == THREADS * TURNS);
    Require(scePthreadCondDestroy(&ring.cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&ring.mutex) == SCE_OK);
}

struct Wake {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<bool> waiting{false};
    bool signaled = false;
    int depth = 1;
};

static void* APS5_VABI WaitForSignal(void* arg) {
    auto& wake = *static_cast<Wake*>(arg);
    for (int level = 0; level < wake.depth; ++level)
        Require(scePthreadMutexLock(&wake.mutex) == SCE_OK);
    wake.waiting.store(true);
    while (!wake.signaled)
        Require(scePthreadCondWait(&wake.cond, &wake.mutex) == SCE_OK);
    for (int level = 0; level < wake.depth; ++level)
        Require(scePthreadMutexUnlock(&wake.mutex) == SCE_OK);
    return nullptr;
}

static void CheckCondRelockYields(PthreadMutexattr* attr, int type, int depth) {
    constexpr std::uint64_t YIELDS = 5;
    Wake wake;
    wake.depth = depth;
    Require(scePthreadMutexattrSettype(attr, type) == SCE_OK);
    Require(scePthreadMutexInit(&wake.mutex, attr, "wake") == SCE_OK);
    Require(scePthreadMutexSetspinloops(&wake.mutex, 0) == SCE_OK);
    Require(scePthreadMutexSetyieldloops(&wake.mutex, static_cast<int>(YIELDS)) == SCE_OK);
    Require(scePthreadCondInit(&wake.cond, nullptr, "wake") == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, WaitForSignal, &wake, nullptr) == SCE_OK);
    while (!wake.waiting.load())
        std::this_thread::yield();
    Require(scePthreadMutexLock(&wake.mutex) == SCE_OK);
    const auto before = MutexYieldsForTest_nid_no_patch();
    wake.signaled = true;
    Require(scePthreadCondSignal(&wake.cond) == SCE_OK);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (MutexYieldsForTest_nid_no_patch() - before < YIELDS && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    Require(scePthreadMutexUnlock(&wake.mutex) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(MutexYieldsForTest_nid_no_patch() - before == YIELDS);
    Require(scePthreadCondDestroy(&wake.cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&wake.mutex) == SCE_OK);
}

static void CheckRecursiveRelock(PthreadMutexattr* attr) {
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexattrSettype(attr, 2) == SCE_OK);
    Require(scePthreadMutexInit(&mutex, attr, "recursive") == SCE_OK);
    Require(scePthreadMutexSetspinloops(&mutex, 2000) == SCE_OK);
    Require(scePthreadMutexSetyieldloops(&mutex, 4) == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_OK);
    Require(scePthreadMutexTimedlock(&mutex, 0) == SCE_OK);
    for (int unlock = 0; unlock < 3; ++unlock)
        Require(scePthreadMutexUnlock(&mutex) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}

int main() {
    CheckSpinAcquire();
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    CheckRecursiveRelock(&attr);
    for (const int type : {1, 2, 3, 4})
        CheckCondRelockYields(&attr, type, 1);
    CheckCondRelockYields(&attr, 2, 2);
    for (const int type : {1, 2, 3, 4}) {
        CheckContention(&attr, type, 2000, 0);
        CheckContention(&attr, type, 0, 4);
        CheckContention(&attr, type, 100, 4);
        CheckContention(&attr, type, -1, -1);
        CheckCondRelock(&attr, type, 2000, 4, false);
        CheckCondRelock(&attr, type, 0, 0, false);
        CheckCondRelock(&attr, type, 2000, 4, true);
    }
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
}
