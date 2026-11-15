#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEfault = 14;
constexpr int PosixEinval = 22;
constexpr int PosixEtimedout = 60;

constexpr KernelClockid ClockRealtime = 0;
constexpr KernelClockid ClockVirtual = 1;
constexpr KernelClockid ClockProf = 2;
constexpr KernelClockid ClockMonotonic = 4;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

struct MutexHolder {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> locked{false};
    std::atomic<bool> release{false};
};

void* APS5_VABI HolderThread(void* arg) {
    auto* holder = static_cast<MutexHolder*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&holder->mutex) == 0);
    holder->locked.store(true);
    while (!holder->release.load()) {
        scePthreadYield();
    }
    Require(pthread_mutex_unlock_nid_postfix(&holder->mutex) == 0);
    return nullptr;
}

void TestTimespecValidationAndNullChecks() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    const KernelTimespec validPast{0, 0};

    Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &validPast) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &validPast) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, nullptr) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, nullptr) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, &validPast) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, nullptr) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, nullptr) == PosixEinval);

    const KernelTimespec invalidTimespecs[] = {
        {-1, 0},
        {-2, 500000},
        {-100, 0},
        {std::numeric_limits<std::int64_t>::min(), 0},
        {0, -1},
        {100, -1},
        {100, -500000},
        {100, std::numeric_limits<std::int64_t>::min()},
        {0, 1000000000},
        {100, 1000000000},
        {100, 1000000001},
        {0, 1500000000},
        {100, 2000000000},
        {100, std::numeric_limits<std::int64_t>::max()},
        {-1, -1},
        {-1, 1000000000},
        {-100, 2000000000},
        {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::min()},
        {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}
    };

    for (const auto& ts : invalidTimespecs) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == PosixEinval);
        Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &ts) == PosixEinval);
        Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &ts) == PosixEinval);
        Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, &ts) == PosixEinval);
    }

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    for (const auto& ts : invalidTimespecs) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == PosixEinval);
    }
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestUninitializedAndInvalidCond() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    const KernelTimespec validPast{0, 0};

    PthreadCond lazyCond = nullptr;
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_cond_timedwait_nid_postfix(&lazyCond, &mutex, &validPast) == PosixEtimedout);
    Require(lazyCond != nullptr);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_cond_destroy_nid_postfix(&lazyCond) == 0);

    PthreadCond destroyedCond = nullptr;
    Require(pthread_cond_init_nid_postfix(&destroyedCond, nullptr) == 0);
    Require(pthread_cond_destroy_nid_postfix(&destroyedCond) == 0);

    Require(pthread_cond_timedwait_nid_postfix(&destroyedCond, &mutex, &validPast) == PosixEinval);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_cond_timedwait_nid_postfix(&destroyedCond, &mutex, &validPast) == PosixEinval);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestUninitializedAndInvalidMutex() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);

    const KernelTimespec validPast{0, 0};

    PthreadMutex uninitMutex = nullptr;
    Require(pthread_cond_timedwait_nid_postfix(&cond, &uninitMutex, &validPast) == PosixEperm);

    PthreadMutex unlockedMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&unlockedMutex, nullptr) == 0);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &unlockedMutex, &validPast) == PosixEperm);

    PthreadMutex destroyedMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&destroyedMutex, nullptr) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&destroyedMutex) == 0);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &destroyedMutex, &validPast) == PosixEinval);

    MutexHolder holder;
    Require(pthread_mutex_init_nid_postfix(&holder.mutex, nullptr) == 0);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, HolderThread, &holder, nullptr) == 0);
    while (!holder.locked.load()) {
        scePthreadYield();
    }

    Require(pthread_cond_timedwait_nid_postfix(&cond, &holder.mutex, &validPast) == PosixEperm);

    holder.release.store(true);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&holder.mutex) == 0);

    Require(pthread_mutex_destroy_nid_postfix(&unlockedMutex) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
}

void TestClockSelectionAndTimedwaitSemantics() {
    PthreadCondattr attr = nullptr;
    Require(pthread_condattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    KernelClockid clockId = -1;
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == 0);
    Require(clockId == ClockRealtime);

    const KernelClockid validClocks[] = {
        ClockRealtime,
        ClockVirtual,
        ClockProf,
        ClockMonotonic
    };

    for (const auto validClock : validClocks) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, validClock) == 0);
        clockId = -1;
        Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == 0);
        Require(clockId == validClock);
    }

    const KernelClockid invalidClocks[] = {-100, -1, 3, 5, 6, 7, 10, 100, 1000};
    for (const auto invalidClock : invalidClocks) {
        Require(pthread_condattr_setclock_nid_postfix(&attr, invalidClock) == PosixEinval);
    }

    Require(pthread_condattr_setclock_nid_postfix(nullptr, ClockRealtime) == PosixEinval);
    Require(pthread_condattr_setclock_nid_postfix(nullptr, ClockMonotonic) == PosixEinval);
    Require(pthread_condattr_getclock_nid_postfix(nullptr, &clockId) == PosixEinval);
    Require(pthread_condattr_getclock_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(pthread_condattr_getclock_nid_postfix(nullptr, nullptr) == PosixEinval);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    {
        Require(pthread_condattr_setclock_nid_postfix(&attr, ClockRealtime) == 0);
        PthreadCond realtimeCond = nullptr;
        Require(pthread_cond_init_nid_postfix(&realtimeCond, &attr) == 0);

        KernelTimespec nowRealtime{};
        Require(clock_gettime_nid_postfix(static_cast<int>(ClockRealtime), &nowRealtime) == 0);
        const KernelTimespec pastRealtime{nowRealtime.tv_sec - 10, 0};

        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        Require(pthread_cond_timedwait_nid_postfix(&realtimeCond, &mutex, &pastRealtime) == PosixEtimedout);
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

        Require(pthread_cond_destroy_nid_postfix(&realtimeCond) == 0);
    }

    {
        Require(pthread_condattr_setclock_nid_postfix(&attr, ClockMonotonic) == 0);
        PthreadCond monotonicCond = nullptr;
        Require(pthread_cond_init_nid_postfix(&monotonicCond, &attr) == 0);

        KernelTimespec nowMonotonic{};
        Require(clock_gettime_nid_postfix(static_cast<int>(ClockMonotonic), &nowMonotonic) == 0);
        const KernelTimespec pastMonotonic{nowMonotonic.tv_sec - 10, 0};

        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        Require(pthread_cond_timedwait_nid_postfix(&monotonicCond, &mutex, &pastMonotonic) == PosixEtimedout);
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

        Require(pthread_cond_destroy_nid_postfix(&monotonicCond) == 0);
    }

    Require(pthread_condattr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_condattr_setclock_nid_postfix(&attr, ClockRealtime) == PosixEinval);
    Require(pthread_condattr_getclock_nid_postfix(&attr, &clockId) == PosixEinval);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestRelativeTimedwaitBounds() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    Require(pthread_cond_reltimedwait_np_nid_postfix(nullptr, &mutex, 0) == PosixEinval);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, nullptr, 0) == PosixEinval);
    Require(pthread_cond_reltimedwait_np_nid_postfix(nullptr, nullptr, 0) == PosixEinval);

    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &mutex, 0) == PosixEperm);

    PthreadMutex destroyedMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&destroyedMutex, nullptr) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&destroyedMutex) == 0);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &destroyedMutex, 0) == PosixEinval);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &mutex, 0) == PosixEtimedout);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

} // namespace

int main() {
    TestTimespecValidationAndNullChecks();
    TestUninitializedAndInvalidCond();
    TestUninitializedAndInvalidMutex();
    TestClockSelectionAndTimedwaitSemantics();
    TestRelativeTimedwaitBounds();
    return 0;
}
