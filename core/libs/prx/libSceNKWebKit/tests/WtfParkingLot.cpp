#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

using NKWebKit::WtfAbi::BeforeSleepLambda;
using NKWebKit::WtfAbi::BinarySemaphore;
using NKWebKit::WtfAbi::Lock;
using NKWebKit::WtfAbi::ParkResult;
using NKWebKit::WtfAbi::ReadWriteLock;
using NKWebKit::WtfAbi::UnparkCallbackLambda;
using NKWebKit::WtfAbi::UnparkResult;
using NKWebKit::WtfAbi::ValidationLambda;
using NKWebKit::WtfAbi::WordLock;
using WTF::TimeWithDynamicClockType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WtfParkingLot check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static const TimeWithDynamicClockType Infinity{std::numeric_limits<double>::infinity(), WTF::ClockType::Wall};

static TimeWithDynamicClockType MonotonicIn(double seconds) {
    return TimeWithDynamicClockType{_ZN3WTF13MonotonicTime3nowEv().m_value + seconds, WTF::ClockType::Monotonic};
}

static std::uint8_t Load(const std::uint8_t& byte) {
    return std::atomic_ref<std::uint8_t>(const_cast<std::uint8_t&>(byte)).load();
}

static void GuestLock(Lock& lock) {
    std::uint8_t expected = 0;
    if (std::atomic_ref<std::uint8_t>(lock.byte).compare_exchange_weak(expected, 1)) return;
    _ZN3WTF4Lock8lockSlowEv(&lock);
}

static void GuestUnlock(Lock& lock) {
    std::uint8_t expected = 1;
    if (std::atomic_ref<std::uint8_t>(lock.byte).compare_exchange_weak(expected, 0)) return;
    _ZN3WTF4Lock10unlockSlowEv(&lock);
}

static void GuestLock(WordLock& lock) {
    std::uintptr_t expected = 0;
    if (std::atomic_ref<std::uintptr_t>(lock.word).compare_exchange_weak(expected, 1)) return;
    _ZN3WTF8WordLock8lockSlowEv(&lock);
}

static void GuestUnlock(WordLock& lock) {
    std::uintptr_t expected = 1;
    if (std::atomic_ref<std::uintptr_t>(lock.word).compare_exchange_weak(expected, 0)) return;
    _ZN3WTF8WordLock10unlockSlowEv(&lock);
}

struct ParkContext {
    bool validate{true};
    std::atomic<int> validations{0};
    std::atomic<int> sleeps{0};
};

static bool APS5_VABI Validate(void* argument) {
    auto* context = static_cast<ParkContext*>(argument);
    ++context->validations;
    return context->validate;
}

static void APS5_VABI BeforeSleep(void* argument) {
    ++static_cast<ParkContext*>(argument)->sleeps;
}

struct UnparkContext {
    std::intptr_t token{0};
    int calls{0};
    UnparkResult seen;
};

static std::intptr_t APS5_VABI OnUnpark(void* argument, UnparkResult result) {
    auto* context = static_cast<UnparkContext*>(argument);
    ++context->calls;
    context->seen = result;
    return context->token;
}

static ParkResult Park(const void* address, ParkContext& context, const TimeWithDynamicClockType& timeout) {
    const ValidationLambda validation{&Validate, &context};
    const BeforeSleepLambda beforeSleep{&BeforeSleep, &context};
    return _ZN3WTF10ParkingLot21parkConditionallyImplEPKvRKNS_12ScopedLambdaIFbvEEERKNS3_IFvvEEERKNS_24TimeWithDynamicClockTypeE(address, &validation, &beforeSleep, &timeout);
}

static void WaitForParked(ParkContext& context, int count) {
    while (context.validations.load() < count) std::this_thread::yield();
}

static void ParkRefusedByValidation() {
    int address = 0;
    ParkContext context;
    context.validate = false;
    const ParkResult result = Park(&address, context, Infinity);
    Require(!result.wasUnparked && result.token == 0);
    Require(context.validations == 1 && context.sleeps == 0);
}

static void ParkTimesOut() {
    int address = 0;
    for (const double seconds : {-1.0, 0.03}) {
        ParkContext context;
        const auto started = std::chrono::steady_clock::now();
        const ParkResult result = Park(&address, context, MonotonicIn(seconds));
        Require(!result.wasUnparked && result.token == 0);
        Require(context.validations == 1 && context.sleeps == 1);
        if (seconds > 0) Require(std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(25));
    }
    const ParkResult wall = [&] {
        ParkContext context;
        const TimeWithDynamicClockType past{_ZN3WTF8WallTime3nowEv().m_value - 1.0, WTF::ClockType::Wall};
        return Park(&address, context, past);
    }();
    Require(!wall.wasUnparked);
    UnparkContext unpark;
    const UnparkCallbackLambda callback{&OnUnpark, &unpark};
    _ZN3WTF10ParkingLot13unparkOneImplEPKvRKNS_12ScopedLambdaIFlNS0_12UnparkResultEEEE(&address, &callback);
    Require(unpark.calls == 1 && !unpark.seen.didUnparkThread && !unpark.seen.mayHaveMoreThreads && !unpark.seen.timeToBeFair);
}

static void UnparkOneHandsOverTheToken() {
    int address = 0;
    ParkContext context;
    ParkResult parked;
    std::thread waiter([&] { parked = Park(&address, context, Infinity); });
    WaitForParked(context, 1);
    UnparkContext unpark;
    unpark.token = 42;
    const UnparkCallbackLambda callback{&OnUnpark, &unpark};
    _ZN3WTF10ParkingLot13unparkOneImplEPKvRKNS_12ScopedLambdaIFlNS0_12UnparkResultEEEE(&address, &callback);
    waiter.join();
    Require(unpark.calls == 1 && unpark.seen.didUnparkThread && !unpark.seen.mayHaveMoreThreads);
    Require(parked.wasUnparked && parked.token == 42 && context.sleeps == 1);
    const UnparkResult none = _ZN3WTF10ParkingLot9unparkOneEPKv(&address);
    Require(!none.didUnparkThread && !none.mayHaveMoreThreads && !none.timeToBeFair);
}

static void UnparkOneReportsMoreThreads() {
    int address = 0;
    ParkContext context;
    std::vector<ParkResult> results(2);
    std::vector<std::thread> waiters;
    for (auto& result : results) waiters.emplace_back([&] { result = Park(&address, context, Infinity); });
    WaitForParked(context, 2);
    const UnparkResult first = _ZN3WTF10ParkingLot9unparkOneEPKv(&address);
    const UnparkResult second = _ZN3WTF10ParkingLot9unparkOneEPKv(&address);
    for (auto& waiter : waiters) waiter.join();
    Require(first.didUnparkThread && first.mayHaveMoreThreads);
    Require(second.didUnparkThread && !second.mayHaveMoreThreads);
    for (const auto& result : results) Require(result.wasUnparked && result.token == 0);
}

static void UnparkCountAndAll() {
    int address = 0;
    int other = 0;
    ParkContext context;
    ParkContext otherContext;
    std::vector<std::thread> waiters;
    for (int index = 0; index < 4; ++index) waiters.emplace_back([&] { Require(Park(&address, context, Infinity).wasUnparked); });
    std::thread elsewhere([&] { Require(Park(&other, otherContext, Infinity).wasUnparked); });
    WaitForParked(context, 4);
    WaitForParked(otherContext, 1);
    Require(_ZN3WTF10ParkingLot11unparkCountEPKvj(&address, 0) == 0);
    Require(_ZN3WTF10ParkingLot11unparkCountEPKvj(&address, 3) == 3);
    _ZN3WTF10ParkingLot9unparkAllEPKv(&address);
    for (auto& waiter : waiters) waiter.join();
    Require(_ZN3WTF10ParkingLot11unparkCountEPKvj(&address, 5) == 0);
    Require(_ZN3WTF10ParkingLot11unparkCountEPKvj(&other, 5) == 1);
    elsewhere.join();
}

template<typename LockType>
static void CountsUnderContention() {
    LockType lock{};
    long counter = 0;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 8; ++thread) {
        threads.emplace_back([&] {
            for (int index = 0; index < 20000; ++index) {
                GuestLock(lock);
                ++counter;
                GuestUnlock(lock);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    Require(counter == 160000);
}

static void LockParksAndIsHandedOver() {
    Lock lock{1};
    std::atomic<bool> acquired{false};
    std::thread waiter([&] {
        GuestLock(lock);
        acquired = true;
        GuestUnlock(lock);
    });
    while (Load(lock.byte) != 3) std::this_thread::yield();
    Require(!acquired);
    GuestUnlock(lock);
    waiter.join();
    Require(acquired && Load(lock.byte) == 0);
}

static void SafepointWithoutWaitersKeepsTheLock() {
    Lock lock{1};
    _ZN3WTF4Lock13safepointSlowEv(&lock);
    Require(Load(lock.byte) == 1);
    std::atomic<bool> ran{false};
    std::thread waiter([&] {
        GuestLock(lock);
        ran = true;
        GuestUnlock(lock);
    });
    while (Load(lock.byte) != 3) std::this_thread::yield();
    _ZN3WTF4Lock13safepointSlowEv(&lock);
    Require(Load(lock.byte) & 1);
    GuestUnlock(lock);
    waiter.join();
    Require(ran && Load(lock.byte) == 0);
}

static void WordLockQueuesWaiters() {
    WordLock lock{1};
    std::atomic<int> acquired{0};
    std::vector<std::thread> waiters;
    for (int index = 0; index < 3; ++index) {
        waiters.emplace_back([&] {
            GuestLock(lock);
            ++acquired;
            GuestUnlock(lock);
        });
    }
    for (;;) {
        const auto word = std::atomic_ref<std::uintptr_t>(lock.word).load();
        if (word & ~std::uintptr_t{3}) break;
        std::this_thread::yield();
    }
    Require(acquired == 0);
    GuestUnlock(lock);
    for (auto& waiter : waiters) waiter.join();
    Require(acquired == 3 && std::atomic_ref<std::uintptr_t>(lock.word).load() == 0);
}

static unsigned LoadUnsigned(unsigned& value) {
    return std::atomic_ref<unsigned>(value).load();
}

static void ReadWriteLockSharesAndExcludes() {
    ReadWriteLock rw{};
    _ZN3WTF13ReadWriteLock8readLockEv(&rw);
    std::thread secondReader([&] {
        _ZN3WTF13ReadWriteLock8readLockEv(&rw);
        _ZN3WTF13ReadWriteLock10readUnlockEv(&rw);
    });
    secondReader.join();
    Require(rw.numReaders == 1 && !rw.isWriteLocked);
    std::atomic<bool> wrote{false};
    std::thread writer([&] {
        _ZN3WTF13ReadWriteLock9writeLockEv(&rw);
        wrote = true;
        _ZN3WTF13ReadWriteLock11writeUnlockEv(&rw);
    });
    while (LoadUnsigned(rw.numWaitingWriters) != 1) std::this_thread::yield();
    std::atomic<bool> lateReader{false};
    std::thread blockedReader([&] {
        _ZN3WTF13ReadWriteLock8readLockEv(&rw);
        lateReader = true;
        _ZN3WTF13ReadWriteLock10readUnlockEv(&rw);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(!wrote && !lateReader);
    _ZN3WTF13ReadWriteLock10readUnlockEv(&rw);
    writer.join();
    blockedReader.join();
    Require(wrote && lateReader);
    Require(rw.numReaders == 0 && rw.numWaitingWriters == 0 && !rw.isWriteLocked && rw.lock.byte == 0);
}

static void ReadWriteLockCounts() {
    ReadWriteLock rw{};
    long value = 0;
    std::atomic<bool> torn{false};
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread) {
        threads.emplace_back([&] {
            for (int index = 0; index < 5000; ++index) {
                _ZN3WTF13ReadWriteLock9writeLockEv(&rw);
                value += 2;
                _ZN3WTF13ReadWriteLock11writeUnlockEv(&rw);
            }
        });
        threads.emplace_back([&] {
            for (int index = 0; index < 5000; ++index) {
                _ZN3WTF13ReadWriteLock8readLockEv(&rw);
                if (value % 2) torn = true;
                _ZN3WTF13ReadWriteLock10readUnlockEv(&rw);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    Require(value == 40000 && !torn);
}

static void BinarySemaphoreSignalsOnce() {
    BinarySemaphore semaphore{};
    _ZN3WTF15BinarySemaphore6signalEv(&semaphore);
    Require(semaphore.isSet);
    Require(_ZN3WTF15BinarySemaphore9waitUntilERKNS_24TimeWithDynamicClockTypeE(&semaphore, &Infinity));
    Require(!semaphore.isSet);
    const auto past = MonotonicIn(-1.0);
    Require(!_ZN3WTF15BinarySemaphore9waitUntilERKNS_24TimeWithDynamicClockTypeE(&semaphore, &past));
    const auto started = std::chrono::steady_clock::now();
    const auto soon = MonotonicIn(0.03);
    Require(!_ZN3WTF15BinarySemaphore9waitUntilERKNS_24TimeWithDynamicClockTypeE(&semaphore, &soon));
    Require(std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(25));
    Require(semaphore.lock.byte == 0);
}

static void BinarySemaphoreWakesAWaiter() {
    BinarySemaphore semaphore{};
    std::atomic<bool> woke{false};
    std::thread waiter([&] {
        Require(_ZN3WTF15BinarySemaphore9waitUntilERKNS_24TimeWithDynamicClockTypeE(&semaphore, &Infinity));
        woke = true;
    });
    while (!Load(semaphore.condition.hasWaiters)) std::this_thread::yield();
    Require(!woke);
    _ZN3WTF15BinarySemaphore6signalEv(&semaphore);
    waiter.join();
    Require(woke && !semaphore.isSet && semaphore.lock.byte == 0);
}

static void RefusesNull() {
    int address = 0;
    ParkContext context;
    const ValidationLambda validation{&Validate, &context};
    const BeforeSleepLambda beforeSleep{&BeforeSleep, &context};
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10ParkingLot21parkConditionallyImplEPKvRKNS_12ScopedLambdaIFbvEEERKNS3_IFvvEEERKNS_24TimeWithDynamicClockTypeE(&address, nullptr, &beforeSleep, &Infinity); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10ParkingLot21parkConditionallyImplEPKvRKNS_12ScopedLambdaIFbvEEERKNS3_IFvvEEERKNS_24TimeWithDynamicClockTypeE(&address, &validation, nullptr, &Infinity); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10ParkingLot21parkConditionallyImplEPKvRKNS_12ScopedLambdaIFbvEEERKNS3_IFvvEEERKNS_24TimeWithDynamicClockTypeE(&address, &validation, &beforeSleep, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10ParkingLot13unparkOneImplEPKvRKNS_12ScopedLambdaIFlNS0_12UnparkResultEEEE(&address, nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF4Lock8lockSlowEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF4Lock10unlockSlowEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF4Lock13safepointSlowEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF8WordLock8lockSlowEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF8WordLock10unlockSlowEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF13ReadWriteLock8readLockEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF13ReadWriteLock11writeUnlockEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN3WTF15BinarySemaphore6signalEv(nullptr); }));
    BinarySemaphore semaphore{};
    Require(ThrowsInvalidArgument([&] { _ZN3WTF15BinarySemaphore9waitUntilERKNS_24TimeWithDynamicClockTypeE(&semaphore, nullptr); }));
    Require(context.validations == 0);
}

int main() {
    ParkRefusedByValidation();
    ParkTimesOut();
    UnparkOneHandsOverTheToken();
    UnparkOneReportsMoreThreads();
    UnparkCountAndAll();
    CountsUnderContention<Lock>();
    CountsUnderContention<WordLock>();
    LockParksAndIsHandedOver();
    SafepointWithoutWaitersKeepsTheLock();
    WordLockQueuesWaiters();
    ReadWriteLockSharesAndExcludes();
    ReadWriteLockCounts();
    BinarySemaphoreSignalsOnce();
    BinarySemaphoreWakesAWaiter();
    RefusesNull();
    std::puts("WtfParkingLot passed");
    return 0;
}
