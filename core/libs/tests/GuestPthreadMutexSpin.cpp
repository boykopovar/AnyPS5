#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdlib>
#include <initializer_list>

static constexpr int SCE_OK = 0;
static constexpr int THREADS = 4;
static constexpr int ROUNDS = 2000;

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
    for (const int type : {1, 2, 3, 4}) {
        CheckContention(&attr, type, 2000, 0);
        CheckContention(&attr, type, 0, 4);
        CheckContention(&attr, type, 100, 4);
        CheckContention(&attr, type, -1, -1);
    }
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
}
