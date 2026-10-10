#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
int APS5_VABI pthread_rwlock_destroy_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_rdlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_unlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_tryrdlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_trywrlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_timedrdlock_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime);
int APS5_VABI pthread_rwlock_timedwrlock_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime);
}

namespace {

using TimedLock = int (APS5_VABI *)(PthreadRwlock*, const KernelTimespec*);
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int sceOk = 0;
constexpr int guestEdeadlk = 11;
constexpr int guestEbusy = 16;
constexpr int guestEinval = 22;
constexpr int guestEtimedout = 60;
constexpr int guestRealtimeClock = 0;
constexpr std::int64_t nanosPerSecond = 1000000000;
constexpr KernelTimespec invalidTimespec{0, nanosPerSecond};

KernelTimespec After(std::int64_t millis) {
    KernelTimespec now{};
    RequireEqual(clock_gettime_nid_postfix(guestRealtimeClock, &now), 0, "clock_gettime");
    const std::int64_t nanos = now.tv_sec * nanosPerSecond + now.tv_nsec + millis * 1000000;
    return {nanos / nanosPerSecond, nanos % nanosPerSecond};
}

class Rwlock {
public:
    Rwlock() {
        RequireEqual(pthread_rwlock_trywrlock_nid_postfix(&handle), 0, "initialize with trywrlock");
        RequireEqual(pthread_rwlock_unlock_nid_postfix(&handle), 0, "unlock after initialization");
    }

    ~Rwlock() {
        if (!destroyed) pthread_rwlock_destroy_nid_postfix(&handle);
    }

    Rwlock(const Rwlock&) = delete;
    Rwlock& operator=(const Rwlock&) = delete;

    PthreadRwlock* Get() noexcept { return &handle; }

    int Destroy() {
        destroyed = true;
        return pthread_rwlock_destroy_nid_postfix(&handle);
    }

private:
    PthreadRwlock handle = nullptr;
    bool destroyed = false;
};

class Holder {
public:
    Holder(PthreadRwlock* rwlock, bool write) : rwlock(rwlock), write(write) {
        RequireEqual(scePthreadCreate(&thread, nullptr, Hold, this, nullptr), sceOk, "create holder thread");
        started = true;
        while (!held.load()) std::this_thread::yield();
        const int locked = lockResult.load();
        if (locked != 0) {
            joined = true;
            scePthreadJoin(thread, nullptr);
            Testing::Fail(std::string(write ? "holder wrlock" : "holder rdlock") + " failed with " + std::to_string(locked));
        }
    }

    ~Holder() {
        release.store(true);
        if (started && !joined) scePthreadJoin(thread, nullptr);
    }

    Holder(const Holder&) = delete;
    Holder& operator=(const Holder&) = delete;

    void Release() { release.store(true); }

    int Join() {
        joined = true;
        return scePthreadJoin(thread, nullptr);
    }

    int UnlockResult() const { return unlockResult.load(); }

private:
    static void* APS5_VABI Hold(void* arg) {
        auto& holder = *static_cast<Holder*>(arg);
        const int locked = holder.write ? pthread_rwlock_wrlock_nid_postfix(holder.rwlock)
                                        : pthread_rwlock_rdlock_nid_postfix(holder.rwlock);
        holder.lockResult.store(locked);
        holder.held.store(true);
        if (locked != 0) return nullptr;
        while (!holder.release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        holder.unlockResult.store(pthread_rwlock_unlock_nid_postfix(holder.rwlock));
        return nullptr;
    }

    PthreadRwlock* rwlock;
    bool write;
    Pthread thread = nullptr;
    bool started = false;
    bool joined = false;
    std::atomic<bool> held{false};
    std::atomic<bool> release{false};
    std::atomic<int> lockResult{-1};
    std::atomic<int> unlockResult{-1};
};

void ExpectTimeout(TimedLock lock, PthreadRwlock* rwlock) {
    const KernelTimespec deadline = After(20);
    const auto start = std::chrono::steady_clock::now();
    RequireEqual(lock(rwlock, &deadline), guestEtimedout, "deadline 20 ms ahead");
    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    Require(waited >= std::chrono::milliseconds(15), "waited at least 15 ms, got " + std::to_string(waited.count()) + " ms");
    const KernelTimespec past = After(-1000);
    RequireEqual(lock(rwlock, &past), guestEtimedout, "deadline in the past");
    const KernelTimespec invalid{deadline.tv_sec, nanosPerSecond};
    RequireEqual(lock(rwlock, &invalid), guestEinval, "nanoseconds 1e9");
    const KernelTimespec negative{deadline.tv_sec, -1};
    RequireEqual(lock(rwlock, &negative), guestEinval, "nanoseconds -1");
}

struct FirstLockRace {
    PthreadRwlock rwlock = nullptr;
    std::atomic<int> ready{0};
    std::atomic<int> inside{0};
    std::atomic<int> overlaps{0};
    std::atomic<int> failures{0};
};

void* APS5_VABI FirstWrlock(void* arg) {
    auto& race = *static_cast<FirstLockRace*>(arg);
    race.ready.fetch_add(1);
    while (race.ready.load() < 2) {}
    if (pthread_rwlock_wrlock_nid_postfix(&race.rwlock) != 0) {
        race.failures.fetch_add(1);
        return nullptr;
    }
    if (race.inside.fetch_add(1) != 0) race.overlaps.fetch_add(1);
    for (int spin = 0; spin < 2000; ++spin) std::atomic_signal_fence(std::memory_order_seq_cst);
    race.inside.fetch_sub(1);
    if (pthread_rwlock_unlock_nid_postfix(&race.rwlock) != 0) race.failures.fetch_add(1);
    return nullptr;
}

const Case firstWrlockRace{"Wrlock_ConcurrentFirstLockOnStaticLock_ExcludesWriters", [] {
    int overlaps = 0;
    for (int round = 0; round < 500; ++round) {
        const std::string context = "round " + std::to_string(round);
        FirstLockRace race;
        Pthread first = nullptr;
        Pthread second = nullptr;
        const int firstCreated = scePthreadCreate(&first, nullptr, FirstWrlock, &race, nullptr);
        const int secondCreated = firstCreated == sceOk ? scePthreadCreate(&second, nullptr, FirstWrlock, &race, nullptr) : -1;
        if (secondCreated != sceOk) race.ready.fetch_add(1);
        const int firstJoined = firstCreated == sceOk ? scePthreadJoin(first, nullptr) : -1;
        const int secondJoined = secondCreated == sceOk ? scePthreadJoin(second, nullptr) : -1;
        const int destroyed = pthread_rwlock_destroy_nid_postfix(&race.rwlock);
        RequireEqual(firstCreated, sceOk, context + " create first thread");
        RequireEqual(secondCreated, sceOk, context + " create second thread");
        RequireEqual(firstJoined, sceOk, context + " join first thread");
        RequireEqual(secondJoined, sceOk, context + " join second thread");
        RequireEqual(race.failures.load(), 0, context + " wrlock or unlock failures");
        RequireEqual(destroyed, 0, context + " destroy");
        overlaps += race.overlaps.load();
    }
    RequireEqual(overlaps, 0, "rounds with two writers inside");
}};

struct FirstLock {
    const char* name;
    int (*lock)(PthreadRwlock*);
};

const Case staticInitializer{"StaticLock_FirstLockOfAnyKind_InitializesLock", [] {
    const std::array<FirstLock, 4> locks{{
        {"tryrdlock", [](PthreadRwlock* rwlock) { return pthread_rwlock_tryrdlock_nid_postfix(rwlock); }},
        {"trywrlock", [](PthreadRwlock* rwlock) { return pthread_rwlock_trywrlock_nid_postfix(rwlock); }},
        {"timedrdlock with invalid time", [](PthreadRwlock* rwlock) {
            return pthread_rwlock_timedrdlock_nid_postfix(rwlock, &invalidTimespec);
        }},
        {"timedwrlock with invalid time", [](PthreadRwlock* rwlock) {
            return pthread_rwlock_timedwrlock_nid_postfix(rwlock, &invalidTimespec);
        }},
    }};
    for (const auto& entry : locks) {
        const std::string context = entry.name;
        PthreadRwlock rwlock = nullptr;
        const int locked = entry.lock(&rwlock);
        const bool initialized = rwlock != nullptr;
        const int unlocked = initialized ? pthread_rwlock_unlock_nid_postfix(&rwlock) : -1;
        const int destroyed = initialized ? pthread_rwlock_destroy_nid_postfix(&rwlock) : -1;
        RequireEqual(locked, 0, context + " result");
        Require(initialized, context + " initializes the lock");
        RequireEqual(unlocked, 0, context + " unlock");
        RequireEqual(destroyed, 0, context + " destroy");
    }
}};

const Case ownWriteTry{"TryLock_WriteHeldBySelf_FailsWithEbusy", [] {
    Rwlock rwlock;
    RequireEqual(pthread_rwlock_trywrlock_nid_postfix(rwlock.Get()), 0, "trywrlock");
    const int read = pthread_rwlock_tryrdlock_nid_postfix(rwlock.Get());
    const int write = pthread_rwlock_trywrlock_nid_postfix(rwlock.Get());
    const int unlocked = pthread_rwlock_unlock_nid_postfix(rwlock.Get());
    RequireEqual(read, guestEbusy, "tryrdlock");
    RequireEqual(write, guestEbusy, "trywrlock again");
    RequireEqual(unlocked, 0, "unlock");
}};

const Case ownWriteTimed{"TimedLock_WriteHeldBySelf_FailsWithEdeadlk", [] {
    Rwlock rwlock;
    RequireEqual(pthread_rwlock_trywrlock_nid_postfix(rwlock.Get()), 0, "trywrlock");
    const KernelTimespec deadline = After(5000);
    const int read = pthread_rwlock_timedrdlock_nid_postfix(rwlock.Get(), &deadline);
    const int write = pthread_rwlock_timedwrlock_nid_postfix(rwlock.Get(), &deadline);
    const int unlocked = pthread_rwlock_unlock_nid_postfix(rwlock.Get());
    RequireEqual(read, guestEdeadlk, "timedrdlock");
    RequireEqual(write, guestEdeadlk, "timedwrlock");
    RequireEqual(unlocked, 0, "unlock");
}};

const Case otherReaderShared{"ReadLock_ReadHeldByOtherThread_Succeeds", [] {
    Rwlock rwlock;
    Holder reader(rwlock.Get(), false);
    RequireEqual(pthread_rwlock_tryrdlock_nid_postfix(rwlock.Get()), 0, "tryrdlock");
    RequireEqual(pthread_rwlock_unlock_nid_postfix(rwlock.Get()), 0, "unlock after tryrdlock");
    RequireEqual(pthread_rwlock_timedrdlock_nid_postfix(rwlock.Get(), &invalidTimespec), 0, "timedrdlock with invalid time");
    RequireEqual(pthread_rwlock_unlock_nid_postfix(rwlock.Get()), 0, "unlock after timedrdlock");
}};

const Case otherReaderTryWrite{"TryWrlock_ReadHeldByOtherThread_FailsWithEbusy", [] {
    Rwlock rwlock;
    Holder reader(rwlock.Get(), false);
    RequireEqual(pthread_rwlock_trywrlock_nid_postfix(rwlock.Get()), guestEbusy, "trywrlock");
}};

const Case otherReaderTimedWrite{"TimedWrlock_ReadHeldByOtherThread_TimesOutOrRejectsInvalidTime", [] {
    Rwlock rwlock;
    Holder reader(rwlock.Get(), false);
    ExpectTimeout(pthread_rwlock_timedwrlock_nid_postfix, rwlock.Get());
}};

const Case otherReaderReleased{"TimedWrlock_ReaderReleasesBeforeDeadline_Succeeds", [] {
    Rwlock rwlock;
    Holder reader(rwlock.Get(), false);
    const KernelTimespec deadline = After(5000);
    reader.Release();
    RequireEqual(pthread_rwlock_timedwrlock_nid_postfix(rwlock.Get(), &deadline), 0, "timedwrlock");
    RequireEqual(reader.Join(), sceOk, "join reader");
    RequireEqual(reader.UnlockResult(), 0, "reader unlock");
    RequireEqual(pthread_rwlock_unlock_nid_postfix(rwlock.Get()), 0, "unlock");
}};

const Case otherWriterTry{"TryLock_WriteHeldByOtherThread_FailsWithEbusy", [] {
    Rwlock rwlock;
    Holder writer(rwlock.Get(), true);
    RequireEqual(pthread_rwlock_tryrdlock_nid_postfix(rwlock.Get()), guestEbusy, "tryrdlock");
    RequireEqual(pthread_rwlock_trywrlock_nid_postfix(rwlock.Get()), guestEbusy, "trywrlock");
}};

const Case otherWriterTimedRead{"TimedRdlock_WriteHeldByOtherThread_TimesOutOrRejectsInvalidTime", [] {
    Rwlock rwlock;
    Holder writer(rwlock.Get(), true);
    ExpectTimeout(pthread_rwlock_timedrdlock_nid_postfix, rwlock.Get());
}};

const Case otherWriterTimedWrite{"TimedWrlock_WriteHeldByOtherThread_TimesOutOrRejectsInvalidTime", [] {
    Rwlock rwlock;
    Holder writer(rwlock.Get(), true);
    ExpectTimeout(pthread_rwlock_timedwrlock_nid_postfix, rwlock.Get());
}};

const Case otherWriterReleased{"TimedRdlock_WriterReleasesBeforeDeadline_Succeeds", [] {
    Rwlock rwlock;
    Holder writer(rwlock.Get(), true);
    const KernelTimespec deadline = After(5000);
    writer.Release();
    RequireEqual(pthread_rwlock_timedrdlock_nid_postfix(rwlock.Get(), &deadline), 0, "timedrdlock");
    RequireEqual(writer.Join(), sceOk, "join writer");
    RequireEqual(writer.UnlockResult(), 0, "writer unlock");
    RequireEqual(pthread_rwlock_unlock_nid_postfix(rwlock.Get()), 0, "unlock");
}};

const Case nullAbstime{"TimedRdlock_NullAbstime_ThrowsRuntimeErrorAndLockStillDestroys", [] {
    Rwlock rwlock;
    RequireThrows<std::runtime_error>([&rwlock] { pthread_rwlock_timedrdlock_nid_postfix(rwlock.Get(), nullptr); },
                                      "timedrdlock with null abstime");
    RequireEqual(rwlock.Destroy(), 0, "destroy");
}};

} // namespace
