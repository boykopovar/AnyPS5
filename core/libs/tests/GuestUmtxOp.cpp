#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

extern "C" {
int APS5_VABI _umtx_op_nid_postfix(void* object, int operation, std::uint64_t value, void* sizeArgument, void* timeArgument);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
}

static constexpr int UMTX_OP_WAIT = 2;
static constexpr int UMTX_OP_WAKE = 3;
static constexpr int UMTX_OP_MUTEX_LOCK = 5;
static constexpr int UMTX_OP_WAIT_UINT = 11;
static constexpr int UMTX_OP_WAIT_UINT_PRIVATE = 15;
static constexpr int UMTX_OP_WAKE_PRIVATE = 16;
static constexpr int UMTX_OP_SEM2_WAIT = 23;
static constexpr int UMTX_OP_MAX = 27;
static constexpr std::uint32_t UMTX_ABSTIME = 1;
static constexpr int CLOCK_MONOTONIC_ID = 4;

static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EOPNOTSUPP = 45;
static constexpr int GUEST_ETIMEDOUT = 60;

static constexpr auto SETTLE = std::chrono::milliseconds(100);

struct UmtxTime {
    KernelTimespec timeout;
    std::uint32_t flags;
    std::uint32_t clockId;
};

static void Require(bool value) { if (!value) std::abort(); }

static KernelTimespec FailsafeTimeout() { return {10, 0}; }

static int Umtx(void* object, int operation, std::uint64_t value, void* sizeArgument = nullptr, void* timeArgument = nullptr) {
    *__error_nid_postfix() = 0;
    return _umtx_op_nid_postfix(object, operation, value, sizeArgument, timeArgument);
}

static bool FailsWith(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

static void* SizeOf(std::size_t size) { return reinterpret_cast<void*>(size); }

static void AwaitCount(const std::atomic<int>& counter, int value) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (counter.load() != value) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static void ReturnsWhenTheValueDiffers() {
    std::uint64_t value = 2;
    Require(Umtx(&value, UMTX_OP_WAIT, 1) == 0);
    KernelTimespec none{0, 0};
    Require(Umtx(&value, UMTX_OP_WAIT, 1, nullptr, &none) == 0);

    std::uint32_t word = 2;
    Require(Umtx(&word, UMTX_OP_WAIT_UINT, 1) == 0);
    Require(Umtx(&word, UMTX_OP_WAIT_UINT_PRIVATE, 1, nullptr, &none) == 0);
}

static void WaitComparesALongAndWaitUintAnInt() {
    std::uint64_t value = 0x100000001ULL;
    KernelTimespec none{0, 0};
    Require(Umtx(&value, UMTX_OP_WAIT, 1, nullptr, &none) == 0);
    Require(FailsWith(Umtx(&value, UMTX_OP_WAIT, 0x100000001ULL, nullptr, &none), GUEST_ETIMEDOUT));

    auto* low = reinterpret_cast<std::uint32_t*>(&value);
    Require(FailsWith(Umtx(low, UMTX_OP_WAIT_UINT, 1, nullptr, &none), GUEST_ETIMEDOUT));
    Require(FailsWith(Umtx(low, UMTX_OP_WAIT_UINT_PRIVATE, 0x700000001ULL, nullptr, &none), GUEST_ETIMEDOUT));
}

static void TimesOutWhileTheValueMatches() {
    std::uint32_t word = 1;
    KernelTimespec none{0, 0};
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, nullptr, &none), GUEST_ETIMEDOUT));

    KernelTimespec relative{0, 20000000};
    auto start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(relative)), &relative), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));

    UmtxTime time{{0, 20000000}, 0, CLOCK_MONOTONIC_ID};
    start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(time)), &time), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));

    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(CLOCK_MONOTONIC_ID, &now) == 0);
    UmtxTime absolute{{now.tv_sec, now.tv_nsec + 20000000}, UMTX_ABSTIME, CLOCK_MONOTONIC_ID};
    if (absolute.timeout.tv_nsec >= 1000000000) {
        absolute.timeout.tv_sec += 1;
        absolute.timeout.tv_nsec -= 1000000000;
    }
    start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(absolute)), &absolute), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15));

    UmtxTime past{{now.tv_sec, now.tv_nsec}, UMTX_ABSTIME, CLOCK_MONOTONIC_ID};
    start = std::chrono::steady_clock::now();
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(past)), &past), GUEST_ETIMEDOUT));
    Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
}

static void RejectsAnInvalidTimeout() {
    std::uint32_t word = 1;
    KernelTimespec negative{-1, 0};
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, nullptr, &negative), GUEST_EINVAL));
    KernelTimespec tooManyNanos{0, 1000000000};
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, nullptr, &tooManyNanos), GUEST_EINVAL));
    UmtxTime negativeNanos{{0, -1}, 0, CLOCK_MONOTONIC_ID};
    Require(FailsWith(Umtx(&word, UMTX_OP_WAIT_UINT, 1, SizeOf(sizeof(negativeNanos)), &negativeNanos), GUEST_EINVAL));
}

static void WakeReleasesAWaiterWithoutAValueChange() {
    std::uint64_t value = 1;
    std::atomic<int> finished{0};
    int result = -2;
    std::thread waiter([&] {
        KernelTimespec failsafe = FailsafeTimeout();
        result = Umtx(&value, UMTX_OP_WAIT, 1, nullptr, &failsafe);
        ++finished;
    });
    while (finished.load() == 0) {
        Require(Umtx(&value, UMTX_OP_WAKE, 1) == 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    waiter.join();
    Require(result == 0);
}

static void WakeHonoursTheCountAndTheAddress() {
    std::uint32_t word = 1;
    std::uint32_t other = 1;
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::atomic<int> otherFinished{0};
    std::vector<int> results(4, -2);
    int otherResult = -2;

    std::vector<std::thread> waiters;
    for (int i = 0; i < 4; ++i) {
        waiters.emplace_back([&, i] {
            KernelTimespec failsafe = FailsafeTimeout();
            ++started;
            results[i] = Umtx(&word, UMTX_OP_WAIT_UINT_PRIVATE, 1, nullptr, &failsafe);
            ++finished;
        });
    }
    std::thread otherWaiter([&] {
        KernelTimespec failsafe = FailsafeTimeout();
        ++started;
        otherResult = Umtx(&other, UMTX_OP_WAIT_UINT, 1, nullptr, &failsafe);
        ++otherFinished;
    });
    AwaitCount(started, 5);
    std::this_thread::sleep_for(SETTLE);

    Require(Umtx(&word, UMTX_OP_WAKE_PRIVATE, 1) == 0);
    AwaitCount(finished, 1);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 1);

    Require(Umtx(&word, UMTX_OP_WAKE, 0) == 0);
    AwaitCount(finished, 2);
    std::this_thread::sleep_for(SETTLE);
    Require(finished.load() == 2);

    Require(Umtx(&word, UMTX_OP_WAKE, INT_MAX) == 0);
    AwaitCount(finished, 4);
    std::this_thread::sleep_for(SETTLE);
    Require(otherFinished.load() == 0);

    Require(Umtx(&other, UMTX_OP_WAKE_PRIVATE, 1) == 0);
    for (auto& waiter : waiters) waiter.join();
    otherWaiter.join();
    for (const int result : results) Require(result == 0);
    Require(otherResult == 0);
}

static void WakeWithoutWaitersSucceeds() {
    std::uint32_t word = 0;
    Require(Umtx(&word, UMTX_OP_WAKE, 1) == 0);
    Require(Umtx(&word, UMTX_OP_WAKE_PRIVATE, INT_MAX) == 0);
}

static void RejectsBadObjectsAndOperations() {
    Require(FailsWith(Umtx(nullptr, UMTX_OP_WAIT, 0), GUEST_EFAULT));
    Require(FailsWith(Umtx(nullptr, UMTX_OP_WAIT_UINT, 0), GUEST_EFAULT));
    Require(FailsWith(Umtx(nullptr, UMTX_OP_WAKE, 1), GUEST_EFAULT));
    Require(Umtx(nullptr, UMTX_OP_WAKE_PRIVATE, 1) == 0);

    std::uint64_t value = 0;
    Require(FailsWith(Umtx(&value, 0, 0), GUEST_EOPNOTSUPP));
    Require(FailsWith(Umtx(&value, 1, 0), GUEST_EOPNOTSUPP));
    Require(FailsWith(Umtx(&value, UMTX_OP_MAX, 0), GUEST_EINVAL));
    Require(FailsWith(Umtx(&value, -1, 0), GUEST_EINVAL));

    bool threw = false;
    try { Umtx(&value, UMTX_OP_MUTEX_LOCK, 0); }
    catch (const std::runtime_error&) { threw = true; }
    Require(threw);
    threw = false;
    try { Umtx(&value, UMTX_OP_SEM2_WAIT, 0); }
    catch (const std::runtime_error&) { threw = true; }
    Require(threw);
}

int main() {
    ReturnsWhenTheValueDiffers();
    WaitComparesALongAndWaitUintAnInt();
    TimesOutWhileTheValueMatches();
    RejectsAnInvalidTimeout();
    WakeReleasesAWaiterWithoutAValueChange();
    WakeHonoursTheCountAndTheAddress();
    WakeWithoutWaitersSucceeds();
    RejectsBadObjectsAndOperations();
}
