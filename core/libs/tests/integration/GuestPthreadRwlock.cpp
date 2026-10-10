#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
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

using TimedLock = int (APS5_VABI *)(PthreadRwlock*, const KernelTimespec*);

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_EBUSY = 16;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ETIMEDOUT = 60;
static constexpr int GUEST_REALTIME_CLOCK = 0;
static constexpr std::int64_t NANOS_PER_SECOND = 1000000000;

static void Require(bool value) { if (!value) std::abort(); }

static KernelTimespec After(std::int64_t millis) {
    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(GUEST_REALTIME_CLOCK, &now) == 0);
    const std::int64_t nanos = now.tv_sec * NANOS_PER_SECOND + now.tv_nsec + millis * 1000000;
    return {nanos / NANOS_PER_SECOND, nanos % NANOS_PER_SECOND};
}

struct Holder {
    PthreadRwlock* rwlock;
    bool write;
    std::atomic<bool> held{false};
    std::atomic<bool> release{false};
    Pthread thread = nullptr;
};

static void* APS5_VABI Hold(void* arg) {
    auto& holder = *static_cast<Holder*>(arg);
    Require((holder.write ? pthread_rwlock_wrlock_nid_postfix(holder.rwlock) : pthread_rwlock_rdlock_nid_postfix(holder.rwlock)) == 0);
    holder.held.store(true);
    while (!holder.release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(pthread_rwlock_unlock_nid_postfix(holder.rwlock) == 0);
    return nullptr;
}

static void Start(Holder& holder) {
    Require(scePthreadCreate(&holder.thread, nullptr, Hold, &holder, nullptr) == SCE_OK);
    while (!holder.held.load()) std::this_thread::yield();
}

static void ExpectTimeout(TimedLock lock, PthreadRwlock* rwlock) {
    const KernelTimespec deadline = After(20);
    const auto start = std::chrono::steady_clock::now();
    Require(lock(rwlock, &deadline) == GUEST_ETIMEDOUT);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
    const KernelTimespec past = After(-1000);
    Require(lock(rwlock, &past) == GUEST_ETIMEDOUT);
    const KernelTimespec invalid{deadline.tv_sec, NANOS_PER_SECOND};
    Require(lock(rwlock, &invalid) == GUEST_EINVAL);
    const KernelTimespec negative{deadline.tv_sec, -1};
    Require(lock(rwlock, &negative) == GUEST_EINVAL);
}

struct FirstLockRace {
    PthreadRwlock rwlock = nullptr;
    std::atomic<int> ready{0};
    std::atomic<int> inside{0};
    std::atomic<int> overlaps{0};
};

static void* APS5_VABI FirstWrlock(void* arg) {
    auto& race = *static_cast<FirstLockRace*>(arg);
    race.ready.fetch_add(1);
    while (race.ready.load() < 2) {}
    Require(pthread_rwlock_wrlock_nid_postfix(&race.rwlock) == 0);
    if (race.inside.fetch_add(1) != 0) race.overlaps.fetch_add(1);
    for (int spin = 0; spin < 2000; ++spin) std::atomic_signal_fence(std::memory_order_seq_cst);
    race.inside.fetch_sub(1);
    Require(pthread_rwlock_unlock_nid_postfix(&race.rwlock) == 0);
    return nullptr;
}

static void CheckConcurrentFirstWrlockExcludes() {
    int overlaps = 0;
    for (int round = 0; round < 500; ++round) {
        FirstLockRace race;
        Pthread first = nullptr;
        Pthread second = nullptr;
        Require(scePthreadCreate(&first, nullptr, FirstWrlock, &race, nullptr) == SCE_OK);
        Require(scePthreadCreate(&second, nullptr, FirstWrlock, &race, nullptr) == SCE_OK);
        Require(scePthreadJoin(first, nullptr) == SCE_OK);
        Require(scePthreadJoin(second, nullptr) == SCE_OK);
        overlaps += race.overlaps.load();
        Require(pthread_rwlock_destroy_nid_postfix(&race.rwlock) == 0);
    }
    Require(overlaps == 0);
}

int main() {
    CheckConcurrentFirstWrlockExcludes();
    const KernelTimespec invalid{0, NANOS_PER_SECOND};
    PthreadRwlock fresh[4] = {};
    Require(pthread_rwlock_tryrdlock_nid_postfix(&fresh[0]) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&fresh[1]) == 0);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&fresh[2], &invalid) == 0);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&fresh[3], &invalid) == 0);
    for (auto& lock : fresh) {
        Require(lock != nullptr);
        Require(pthread_rwlock_unlock_nid_postfix(&lock) == 0);
        Require(pthread_rwlock_destroy_nid_postfix(&lock) == 0);
    }

    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    KernelTimespec deadline = After(5000);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &deadline) == GUEST_EDEADLK);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &deadline) == GUEST_EDEADLK);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Holder reader{&rwlock, false};
    Start(reader);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &invalid) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    ExpectTimeout(pthread_rwlock_timedwrlock_nid_postfix, &rwlock);
    deadline = After(5000);
    reader.release.store(true);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &deadline) == 0);
    Require(scePthreadJoin(reader.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Holder writer{&rwlock, true};
    Start(writer);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    ExpectTimeout(pthread_rwlock_timedrdlock_nid_postfix, &rwlock);
    ExpectTimeout(pthread_rwlock_timedwrlock_nid_postfix, &rwlock);
    deadline = After(5000);
    writer.release.store(true);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &deadline) == 0);
    Require(scePthreadJoin(writer.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    bool rejected = false;
    try { pthread_rwlock_timedrdlock_nid_postfix(&rwlock, nullptr); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}
