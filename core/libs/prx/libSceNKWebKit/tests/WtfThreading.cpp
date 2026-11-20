#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <thread>

namespace {

using NKWebKit::WtfAbi::Mutex;
using NKWebKit::WtfAbi::ThreadCondition;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "WTF threading check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

bool TryLockFromAnotherThread(Mutex& mutex) {
    bool locked = false;
    std::thread probe([&] {
        locked = _ZN3WTF5Mutex7tryLockEv(&mutex);
        if (locked)
            _ZN3WTF5Mutex6unlockEv(&mutex);
    });
    probe.join();
    return locked;
}

WTF::WallTime After(double seconds) {
    return {_ZN3WTF8WallTime3nowEv().m_value + seconds};
}

void TestMutex() {
    Mutex mutex{reinterpret_cast<PthreadMutex>(std::uintptr_t{0xA5A5A5A5})};
    _ZN3WTF5MutexC1Ev(&mutex);
    Require(mutex.mutex != nullptr);
    Require(mutex.mutex->_type == MutexType::Normal);
    Require(mutex.mutex->_name == "SceNKWTF");

    _ZN3WTF5Mutex4lockEv(&mutex);
    Require(!TryLockFromAnotherThread(mutex));
    _ZN3WTF5Mutex6unlockEv(&mutex);
    Require(TryLockFromAnotherThread(mutex));

    Require(_ZN3WTF5Mutex7tryLockEv(&mutex));
    Require(!TryLockFromAnotherThread(mutex));
    _ZN3WTF5Mutex6unlockEv(&mutex);
    _ZN3WTF5MutexD1Ev(&mutex);

    Mutex base{};
    _ZN3WTF5MutexC2Ev(&base);
    Require(base.mutex->_type == MutexType::Normal);
    _ZN3WTF5MutexD2Ev(&base);
}

void TestCounterUnderContention() {
    Mutex mutex{};
    _ZN3WTF5MutexC1Ev(&mutex);
    int counter = 0;
    auto work = [&] {
        for (int i = 0; i < 10000; ++i) {
            _ZN3WTF5Mutex4lockEv(&mutex);
            ++counter;
            _ZN3WTF5Mutex6unlockEv(&mutex);
        }
    };
    std::thread first(work);
    std::thread second(work);
    first.join();
    second.join();
    Require(counter == 20000);
    _ZN3WTF5MutexD1Ev(&mutex);
}

void TestSignalAndBroadcast() {
    Mutex mutex{};
    _ZN3WTF5MutexC1Ev(&mutex);
    ThreadCondition condition{};
    bool ready = false;
    std::atomic<int> woken = 0;

    auto waiter = [&] {
        _ZN3WTF5Mutex4lockEv(&mutex);
        while (!ready)
            _ZN3WTF15ThreadCondition4waitERNS_5MutexE(&condition, &mutex);
        _ZN3WTF5Mutex6unlockEv(&mutex);
        ++woken;
    };

    std::thread single(waiter);
    _ZN3WTF5Mutex4lockEv(&mutex);
    ready = true;
    _ZN3WTF15ThreadCondition6signalEv(&condition);
    _ZN3WTF5Mutex6unlockEv(&mutex);
    single.join();
    Require(woken == 1);

    ready = false;
    std::thread first(waiter);
    std::thread second(waiter);
    _ZN3WTF5Mutex4lockEv(&mutex);
    ready = true;
    _ZN3WTF15ThreadCondition9broadcastEv(&condition);
    _ZN3WTF5Mutex6unlockEv(&mutex);
    first.join();
    second.join();
    Require(woken == 3);

    _ZN3WTF15ThreadConditionD1Ev(&condition);
    _ZN3WTF5MutexD1Ev(&mutex);
}

bool TimedWaitWithSignal(WTF::WallTime deadline) {
    Mutex mutex{};
    _ZN3WTF5MutexC1Ev(&mutex);
    ThreadCondition condition{};
    bool waiting = false;
    _ZN3WTF5Mutex4lockEv(&mutex);
    std::thread signaller([&] {
        for (;;) {
            _ZN3WTF5Mutex4lockEv(&mutex);
            const bool done = waiting;
            if (done)
                _ZN3WTF15ThreadCondition6signalEv(&condition);
            _ZN3WTF5Mutex6unlockEv(&mutex);
            if (done)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    waiting = true;
    const bool signalled = _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, &mutex, deadline);
    _ZN3WTF5Mutex6unlockEv(&mutex);
    signaller.join();
    _ZN3WTF15ThreadConditionD2Ev(&condition);
    _ZN3WTF5MutexD1Ev(&mutex);
    return signalled;
}

void TestTimedWait() {
    Mutex mutex{};
    _ZN3WTF5MutexC1Ev(&mutex);
    ThreadCondition condition{};
    constexpr double infinity = std::numeric_limits<double>::infinity();

    _ZN3WTF5Mutex4lockEv(&mutex);
    Require(!_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, &mutex, {-infinity}));
    Require(!_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, &mutex, After(-1.0)));
    Require(!_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, &mutex, {0.0}));

    const auto start = std::chrono::steady_clock::now();
    Require(!_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, &mutex, After(0.05)));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    Require(!TryLockFromAnotherThread(mutex));
    _ZN3WTF5Mutex6unlockEv(&mutex);

    RequireInvalidArgument([&] {
        _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, &mutex, {std::numeric_limits<double>::quiet_NaN()});
    });
    _ZN3WTF15ThreadConditionD1Ev(&condition);
    _ZN3WTF5MutexD1Ev(&mutex);

    Require(TimedWaitWithSignal({infinity}));
    Require(TimedWaitWithSignal({1e300}));
    Require(TimedWaitWithSignal(After(30.0)));
}

void TestNullObjects() {
    Mutex mutex{};
    ThreadCondition condition{};
    RequireInvalidArgument([] { _ZN3WTF5MutexC1Ev(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF5MutexD1Ev(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF5Mutex4lockEv(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF5Mutex7tryLockEv(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF5Mutex6unlockEv(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF15ThreadConditionD1Ev(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF15ThreadCondition6signalEv(nullptr); });
    RequireInvalidArgument([] { _ZN3WTF15ThreadCondition9broadcastEv(nullptr); });
    RequireInvalidArgument([&] { _ZN3WTF15ThreadCondition4waitERNS_5MutexE(&condition, nullptr); });
    RequireInvalidArgument([&] { _ZN3WTF15ThreadCondition4waitERNS_5MutexE(nullptr, &mutex); });
    RequireInvalidArgument([&] { _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(nullptr, &mutex, {0.0}); });
}

}

int main() {
    TestMutex();
    TestCounterUnderContention();
    TestSignalAndBroadcast();
    TestTimedWait();
    TestNullObjects();
    std::puts("WTF threading tests passed");
    return 0;
}
