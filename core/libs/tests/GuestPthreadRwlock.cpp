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
int APS5_VABI scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name);
int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock* rwlock);
}

using TimedLock = int (APS5_VABI *)(PthreadRwlock*, const KernelTimespec*);

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
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

struct HandoffState {
    PthreadRwlock rwlock = nullptr;
    std::atomic<bool> firstHeld{false};
    std::atomic<bool> firstRelease{false};
    std::atomic<bool> secondAcquired{false};
    std::atomic<bool> secondRelease{false};
    Pthread firstThread = nullptr;
    Pthread secondThread = nullptr;
};

static void* APS5_VABI HandoffFirst(void* arg) {
    auto& state = *static_cast<HandoffState*>(arg);
    Require(pthread_rwlock_wrlock_nid_postfix(&state.rwlock) == 0);
    state.firstHeld.store(true);
    while (!state.firstRelease.load()) std::this_thread::yield();
    Require(pthread_rwlock_unlock_nid_postfix(&state.rwlock) == 0);
    return nullptr;
}

static void* APS5_VABI HandoffSecond(void* arg) {
    auto& state = *static_cast<HandoffState*>(arg);
    while (!state.firstHeld.load()) std::this_thread::yield();
    Require(pthread_rwlock_wrlock_nid_postfix(&state.rwlock) == 0);
    state.secondAcquired.store(true);
    while (!state.secondRelease.load()) std::this_thread::yield();
    Require(pthread_rwlock_unlock_nid_postfix(&state.rwlock) == 0);
    return nullptr;
}

static void CheckWriterHandoff() {
    HandoffState state;
    Require(scePthreadCreate(&state.firstThread, nullptr, HandoffFirst, &state, nullptr) == SCE_OK);
    Require(scePthreadCreate(&state.secondThread, nullptr, HandoffSecond, &state, nullptr) == SCE_OK);
    while (!state.firstHeld.load()) std::this_thread::yield();
    Require(pthread_rwlock_destroy_nid_postfix(&state.rwlock) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&state.rwlock) == SCE_KERNEL_ERROR_EBUSY);
    Require(state.rwlock != nullptr);
    state.firstRelease.store(true);
    Require(scePthreadJoin(state.firstThread, nullptr) == SCE_OK);
    while (!state.secondAcquired.load()) std::this_thread::yield();
    Require(pthread_rwlock_destroy_nid_postfix(&state.rwlock) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&state.rwlock) == SCE_KERNEL_ERROR_EBUSY);
    Require(state.rwlock != nullptr);
    state.secondRelease.store(true);
    Require(scePthreadJoin(state.secondThread, nullptr) == SCE_OK);
    Require(pthread_rwlock_destroy_nid_postfix(&state.rwlock) == 0);
    Require(state.rwlock == nullptr);
}

static void CheckFailedLockAttempts() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == 0);
    const KernelTimespec timeout = After(10);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &timeout) == GUEST_EDEADLK);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&rwlock) == SCE_KERNEL_ERROR_EBUSY);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    Require(rwlock == nullptr);

    Holder reader{&rwlock, false};
    Start(reader);
    const KernelTimespec past = After(-1000);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &past) == GUEST_ETIMEDOUT);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&rwlock) == SCE_KERNEL_ERROR_EBUSY);
    reader.release.store(true);
    Require(scePthreadJoin(reader.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    Require(rwlock == nullptr);
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
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&rwlock) == SCE_KERNEL_ERROR_EBUSY);
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
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&rwlock) == SCE_KERNEL_ERROR_EBUSY);
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
    Require(rwlock == nullptr);

    PthreadRwlock unheld = nullptr;
    Require(scePthreadRwlockInit(&unheld, nullptr, nullptr) == SCE_OK);
    Require(unheld != nullptr);
    Require(pthread_rwlock_destroy_nid_postfix(&unheld) == 0);
    Require(unheld == nullptr);

    PthreadRwlock rd = nullptr;
    Require(pthread_rwlock_rdlock_nid_postfix(&rd) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rd) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&rd) == SCE_KERNEL_ERROR_EBUSY);
    Require(rd != nullptr);
    Require(pthread_rwlock_unlock_nid_postfix(&rd) == 0);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rd) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rd) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rd) == 0);
    Require(rd == nullptr);

    PthreadRwlock wr = nullptr;
    Require(pthread_rwlock_wrlock_nid_postfix(&wr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&wr) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&wr) == SCE_KERNEL_ERROR_EBUSY);
    Require(wr != nullptr);
    Require(pthread_rwlock_unlock_nid_postfix(&wr) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&wr) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&wr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&wr) == 0);
    Require(wr == nullptr);

    PthreadRwlock remoteRd = nullptr;
    Holder bgReader{&remoteRd, false};
    Start(bgReader);
    Require(pthread_rwlock_destroy_nid_postfix(&remoteRd) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&remoteRd) == SCE_KERNEL_ERROR_EBUSY);
    Require(remoteRd != nullptr);
    bgReader.release.store(true);
    Require(scePthreadJoin(bgReader.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_destroy_nid_postfix(&remoteRd) == 0);
    Require(remoteRd == nullptr);

    PthreadRwlock remoteWr = nullptr;
    Holder bgWriter{&remoteWr, true};
    Start(bgWriter);
    Require(pthread_rwlock_destroy_nid_postfix(&remoteWr) == GUEST_EBUSY);
    Require(scePthreadRwlockDestroy(&remoteWr) == SCE_KERNEL_ERROR_EBUSY);
    Require(remoteWr != nullptr);
    bgWriter.release.store(true);
    Require(scePthreadJoin(bgWriter.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_destroy_nid_postfix(&remoteWr) == 0);
    Require(remoteWr == nullptr);

    CheckWriterHandoff();
    CheckFailedLockAttempts();
}
