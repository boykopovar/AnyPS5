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
int APS5_VABI pthread_rwlock_init_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr);
int APS5_VABI pthread_rwlockattr_init_nid_postfix(PthreadRwlockattr* attr);
int APS5_VABI pthread_rwlockattr_destroy_nid_postfix(PthreadRwlockattr* attr);
int APS5_VABI pthread_rwlockattr_getpshared_nid_postfix(const PthreadRwlockattr* attr, int* pshared);
int APS5_VABI pthread_rwlockattr_setpshared_nid_postfix(PthreadRwlockattr* attr, int pshared);
int APS5_VABI pthread_rwlock_rdlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_unlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_tryrdlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_trywrlock_nid_postfix(PthreadRwlock* rwlock);
int APS5_VABI pthread_rwlock_timedrdlock_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime);
int APS5_VABI pthread_rwlock_timedwrlock_nid_postfix(PthreadRwlock* rwlock, const KernelTimespec* abstime);
int APS5_VABI pthread_rwlock_reltimedrdlock_np_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec);
int APS5_VABI pthread_rwlock_reltimedwrlock_np_nid_postfix(PthreadRwlock* rwlock, KernelUseconds usec);
int APS5_VABI scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name);
int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock* rwlock);
int APS5_VABI scePthreadRwlockattrDestroy(PthreadRwlockattr* attr);
int APS5_VABI scePthreadRwlockRdlock(PthreadRwlock* rwlock);
int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock* rwlock);
int APS5_VABI scePthreadRwlockTryrdlock(PthreadRwlock* rwlock);
int APS5_VABI scePthreadRwlockUnlock(PthreadRwlock* rwlock);
}

using TimedLock = int (APS5_VABI *)(PthreadRwlock*, const KernelTimespec*);

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_EBUSY = 16;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ETIMEDOUT = 60;
static constexpr int GUEST_REALTIME_CLOCK = 0;
static constexpr int PROCESS_PRIVATE = 0;
static constexpr std::int64_t NANOS_PER_SECOND = 1000000000;

static void Require(bool value) { if (!value) std::abort(); }

static const PthreadRwlock DestroyedRwlock = reinterpret_cast<PthreadRwlock>(std::uintptr_t{1});

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
    Require(state.rwlock != nullptr);
    state.firstRelease.store(true);
    Require(scePthreadJoin(state.firstThread, nullptr) == SCE_OK);
    while (!state.secondAcquired.load()) std::this_thread::yield();
    Require(state.rwlock != nullptr);
    state.secondRelease.store(true);
    Require(scePthreadJoin(state.secondThread, nullptr) == SCE_OK);
    Require(pthread_rwlock_destroy_nid_postfix(&state.rwlock) == 0);
    Require(state.rwlock == DestroyedRwlock);
}

static void CheckDestroyLifecycle() {
    PthreadRwlock held = nullptr;
    Require(scePthreadRwlockInit(&held, nullptr, nullptr) == SCE_OK);
    Require(scePthreadRwlockWrlock(&held) == SCE_OK);
    Require(scePthreadRwlockDestroy(&held) == SCE_OK);
    Require(held == DestroyedRwlock);
    Require(scePthreadRwlockUnlock(&held) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockRdlock(&held) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockTryrdlock(&held) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadRwlockDestroy(&held) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_rwlock_rdlock_nid_postfix(&held) == GUEST_EINVAL);
    Require(held == DestroyedRwlock);
    Require(scePthreadRwlockInit(&held, nullptr, nullptr) == SCE_OK);
    Require(held != nullptr && held != DestroyedRwlock);
    Require(scePthreadRwlockRdlock(&held) == SCE_OK);
    Require(scePthreadRwlockRdlock(&held) == SCE_OK);
    Require(scePthreadRwlockDestroy(&held) == SCE_OK);
    Require(held == DestroyedRwlock);

    PthreadRwlock zero = nullptr;
    Require(scePthreadRwlockDestroy(&zero) == SCE_OK);
    Require(zero == nullptr);
    Require(scePthreadRwlockRdlock(&zero) == SCE_OK);
    Require(zero != nullptr && zero != DestroyedRwlock);
    Require(scePthreadRwlockUnlock(&zero) == SCE_OK);
    Require(scePthreadRwlockDestroy(&zero) == SCE_OK);
    Require(zero == DestroyedRwlock);
}

static void CheckFailedLockAttempts() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == 0);
    const KernelTimespec timeout = After(10);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &timeout) == GUEST_EDEADLK);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    Require(rwlock == DestroyedRwlock);
    rwlock = nullptr;

    Holder reader{&rwlock, false};
    Start(reader);
    const KernelTimespec past = After(-1000);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &past) == GUEST_ETIMEDOUT);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    reader.release.store(true);
    Require(scePthreadJoin(reader.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    Require(rwlock == DestroyedRwlock);
}

static void CheckDeadlinesBeyondTheClockRange() {
    PthreadRwlock rwlock = nullptr;
    Holder reader{&rwlock, false};
    Start(reader);
    const KernelTimespec farPast{INT64_MIN, 0};
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &farPast) == GUEST_ETIMEDOUT);
    reader.release.store(true);
    const KernelTimespec farFuture{INT64_MAX, 0};
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &farFuture) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(scePthreadJoin(reader.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}

struct WaitingWriter {
    PthreadRwlock* rwlock;
    std::atomic<bool> started{false};
    std::atomic<bool> acquired{false};
    Pthread thread = nullptr;
};

static void* APS5_VABI WriteOnceStarted(void* arg) {
    auto& writer = *static_cast<WaitingWriter*>(arg);
    writer.started.store(true);
    Require(pthread_rwlock_wrlock_nid_postfix(writer.rwlock) == 0);
    writer.acquired.store(true);
    Require(pthread_rwlock_unlock_nid_postfix(writer.rwlock) == 0);
    return nullptr;
}

static void StartWaitingWriter(WaitingWriter& writer) {
    Require(scePthreadCreate(&writer.thread, nullptr, WriteOnceStarted, &writer, nullptr) == SCE_OK);
    while (!writer.started.load()) std::this_thread::yield();
}

static int TryReadUntilRefused(PthreadRwlock* rwlock) {
    for (int attempt = 0; attempt < 2000; ++attempt) {
        const int result = pthread_rwlock_tryrdlock_nid_postfix(rwlock);
        if (result != 0) return result;
        Require(pthread_rwlock_unlock_nid_postfix(rwlock) == 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 0;
}

struct RefusalProbe {
    PthreadRwlock* rwlock;
    std::atomic<int> result{-1};
};

static void* APS5_VABI ProbeRefusal(void* arg) {
    auto& probe = *static_cast<RefusalProbe*>(arg);
    probe.result.store(TryReadUntilRefused(probe.rwlock));
    return nullptr;
}

static void CheckWaitingWriterHoldsBackNewReaders() {
    PthreadRwlock rwlock = nullptr;
    Holder reader{&rwlock, false};
    Start(reader);
    WaitingWriter writer{&rwlock};
    StartWaitingWriter(writer);
    Require(TryReadUntilRefused(&rwlock) == GUEST_EBUSY);
    const KernelTimespec soon = After(20);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &soon) == GUEST_ETIMEDOUT);
    Require(!writer.acquired.load());
    reader.release.store(true);
    Require(scePthreadJoin(reader.thread, nullptr) == SCE_OK);
    Require(scePthreadJoin(writer.thread, nullptr) == SCE_OK);
    Require(writer.acquired.load());
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);

    PthreadRwlock recursive = nullptr;
    Require(pthread_rwlock_rdlock_nid_postfix(&recursive) == 0);
    WaitingWriter queued{&recursive};
    StartWaitingWriter(queued);
    RefusalProbe probe{&recursive};
    Pthread prober = nullptr;
    Require(scePthreadCreate(&prober, nullptr, ProbeRefusal, &probe, nullptr) == SCE_OK);
    Require(scePthreadJoin(prober, nullptr) == SCE_OK);
    Require(probe.result.load() == GUEST_EBUSY);
    Require(pthread_rwlock_rdlock_nid_postfix(&recursive) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&recursive) == 0);
    Require(!queued.acquired.load());
    Require(pthread_rwlock_unlock_nid_postfix(&recursive) == 0);
    Require(scePthreadJoin(queued.thread, nullptr) == SCE_OK);
    Require(queued.acquired.load());
    Require(pthread_rwlock_destroy_nid_postfix(&recursive) == 0);
}

using RelativeLock = int (APS5_VABI *)(PthreadRwlock*, KernelUseconds);

static void ExpectRelativeTimeout(RelativeLock lock, PthreadRwlock* rwlock) {
    const auto start = std::chrono::steady_clock::now();
    Require(lock(rwlock, 20000) == GUEST_ETIMEDOUT);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));
    Require(lock(rwlock, 0) == GUEST_ETIMEDOUT);
}

static void CheckRelativeTimeouts() {
    PthreadRwlock fresh[2] = {};
    Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&fresh[0], 0) == 0);
    Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&fresh[1], 0) == 0);
    for (auto& lock : fresh) {
        Require(lock != nullptr && lock != DestroyedRwlock);
        Require(pthread_rwlock_unlock_nid_postfix(&lock) == 0);
        Require(pthread_rwlock_destroy_nid_postfix(&lock) == 0);
        Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&lock, 0) == GUEST_EINVAL);
        Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&lock, 0) == GUEST_EINVAL);
    }

    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&rwlock, 5000000) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Holder reader{&rwlock, false};
    Start(reader);
    Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&rwlock, 0) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    ExpectRelativeTimeout(pthread_rwlock_reltimedwrlock_np_nid_postfix, &rwlock);
    reader.release.store(true);
    Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&rwlock, 5000000) == 0);
    Require(scePthreadJoin(reader.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Holder writer{&rwlock, true};
    Start(writer);
    ExpectRelativeTimeout(pthread_rwlock_reltimedrdlock_np_nid_postfix, &rwlock);
    ExpectRelativeTimeout(pthread_rwlock_reltimedwrlock_np_nid_postfix, &rwlock);
    writer.release.store(true);
    Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&rwlock, 5000000) == 0);
    Require(scePthreadJoin(writer.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}

template <typename Call>
static bool Throws(Call call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void CheckAttrProcessSharing() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    int pshared = -1;
    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == PROCESS_PRIVATE);
    for (const int invalid : {1, 2, -1}) {
        Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, invalid) == GUEST_EINVAL);
        pshared = -1;
        Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == PROCESS_PRIVATE);
    }
    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, PROCESS_PRIVATE) == 0);
    pshared = -1;
    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == PROCESS_PRIVATE);
    Require(Throws([&] { pthread_rwlockattr_getpshared_nid_postfix(&attr, nullptr); }));

    PthreadRwlockattr unset = nullptr;
    for (const int invalid : {1, 2, -1}) {
        Require(pthread_rwlockattr_setpshared_nid_postfix(nullptr, invalid) == GUEST_EINVAL);
        Require(pthread_rwlockattr_setpshared_nid_postfix(&unset, invalid) == GUEST_EINVAL);
    }
    Require(Throws([&] { pthread_rwlockattr_setpshared_nid_postfix(nullptr, PROCESS_PRIVATE); }));
    Require(Throws([&] { pthread_rwlockattr_setpshared_nid_postfix(&unset, PROCESS_PRIVATE); }));
    Require(Throws([&] { pthread_rwlockattr_getpshared_nid_postfix(nullptr, &pshared); }));
    Require(Throws([&] { pthread_rwlockattr_getpshared_nid_postfix(&unset, &pshared); }));
    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
    Require(attr != nullptr);
    pshared = -1;
    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == PROCESS_PRIVATE);
    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, PROCESS_PRIVATE) == 0);
    Require(Throws([&] { scePthreadRwlockattrDestroy(&attr); }));
}

int main() {
    CheckAttrProcessSharing();
    CheckWaitingWriterHoldsBackNewReaders();
    CheckConcurrentFirstWrlockExcludes();
    CheckDeadlinesBeyondTheClockRange();
    CheckRelativeTimeouts();
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

    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);
    PthreadRwlock withAttr = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&withAttr, &attr) == 0);
    Require(pthread_rwlock_wrlock_nid_postfix(&withAttr) == 0);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&withAttr) == GUEST_EBUSY);
    Require(pthread_rwlock_unlock_nid_postfix(&withAttr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&withAttr) == 0);
    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);

    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    KernelTimespec deadline = After(5000);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &deadline) == GUEST_EDEADLK);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &deadline) == GUEST_EDEADLK);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == GUEST_EPERM);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == GUEST_EPERM);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == 0);
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
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == GUEST_EPERM);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == GUEST_EBUSY);
    ExpectTimeout(pthread_rwlock_timedrdlock_nid_postfix, &rwlock);
    ExpectTimeout(pthread_rwlock_timedwrlock_nid_postfix, &rwlock);
    deadline = After(5000);
    writer.release.store(true);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &deadline) == 0);
    Require(scePthreadJoin(writer.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == GUEST_EPERM);

    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, nullptr) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    Require(rwlock == DestroyedRwlock);

    PthreadRwlock unheld = nullptr;
    Require(scePthreadRwlockInit(&unheld, nullptr, nullptr) == SCE_OK);
    Require(unheld != nullptr);
    Require(pthread_rwlock_destroy_nid_postfix(&unheld) == 0);
    Require(unheld == DestroyedRwlock);

    PthreadRwlock rd = nullptr;
    Require(pthread_rwlock_rdlock_nid_postfix(&rd) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rd) == 0);
    Require(rd == DestroyedRwlock);
    Require(pthread_rwlock_unlock_nid_postfix(&rd) == GUEST_EINVAL);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rd) == GUEST_EINVAL);
    Require(pthread_rwlock_destroy_nid_postfix(&rd) == GUEST_EINVAL);
    Require(rd == DestroyedRwlock);

    PthreadRwlock wr = nullptr;
    Require(pthread_rwlock_wrlock_nid_postfix(&wr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&wr) == 0);
    Require(wr == DestroyedRwlock);
    Require(pthread_rwlock_unlock_nid_postfix(&wr) == GUEST_EINVAL);
    Require(pthread_rwlock_trywrlock_nid_postfix(&wr) == GUEST_EINVAL);
    Require(pthread_rwlock_destroy_nid_postfix(&wr) == GUEST_EINVAL);
    Require(wr == DestroyedRwlock);

    PthreadRwlock remoteRd = nullptr;
    Holder bgReader{&remoteRd, false};
    Start(bgReader);
    Require(remoteRd != nullptr);
    bgReader.release.store(true);
    Require(scePthreadJoin(bgReader.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_destroy_nid_postfix(&remoteRd) == 0);
    Require(remoteRd == DestroyedRwlock);

    PthreadRwlock remoteWr = nullptr;
    Holder bgWriter{&remoteWr, true};
    Start(bgWriter);
    Require(remoteWr != nullptr);
    bgWriter.release.store(true);
    Require(scePthreadJoin(bgWriter.thread, nullptr) == SCE_OK);
    Require(pthread_rwlock_destroy_nid_postfix(&remoteWr) == 0);
    Require(remoteWr == DestroyedRwlock);

    CheckWriterHandoff();
    CheckFailedLockAttempts();
    CheckDestroyLifecycle();
}
