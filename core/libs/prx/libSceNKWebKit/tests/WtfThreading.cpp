#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 3, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 3, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 4) == 0);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* WritableBeforeGuard() const { return base + pageSize * 2; }
    unsigned char* Guard() const { return base + pageSize * 3; }
};

template<typename TAction> void RequireNotImplemented(TAction action, const char* symbol, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == std::string(symbol) + " not implemented", location);
        return;
    }
    Require(false, location);
}

void TestGuestPointers() {
    GuardedPages pages;
    auto* guardMutex = reinterpret_cast<Mutex*>(pages.Guard());
    auto* readOnlyMutex = reinterpret_cast<Mutex*>(pages.ReadOnly());
    auto* splitMutex = reinterpret_cast<Mutex*>(pages.Guard() - 4);
    auto* misalignedMutex = reinterpret_cast<Mutex*>(pages.WritableBeforeGuard() + 4);
    auto* guardCondition = reinterpret_cast<ThreadCondition*>(pages.Guard());
    auto* readOnlyCondition = reinterpret_cast<ThreadCondition*>(pages.ReadOnly());
    auto* splitCondition = reinterpret_cast<ThreadCondition*>(pages.Guard() - 4);
    auto* misalignedCondition = reinterpret_cast<ThreadCondition*>(pages.WritableBeforeGuard() + 4);

    RequireNotImplemented([&] { _ZN3WTF5MutexC1Ev(guardMutex); }, "_ZN3WTF5MutexC1Ev");
    RequireNotImplemented([&] { _ZN3WTF5MutexC1Ev(readOnlyMutex); }, "_ZN3WTF5MutexC1Ev");
    RequireNotImplemented([&] { _ZN3WTF5MutexC1Ev(splitMutex); }, "_ZN3WTF5MutexC1Ev");
    RequireNotImplemented([&] { _ZN3WTF5MutexC1Ev(misalignedMutex); }, "_ZN3WTF5MutexC1Ev");

    RequireNotImplemented([&] { _ZN3WTF5MutexC2Ev(guardMutex); }, "_ZN3WTF5MutexC2Ev");
    RequireNotImplemented([&] { _ZN3WTF5MutexC2Ev(readOnlyMutex); }, "_ZN3WTF5MutexC2Ev");

    RequireNotImplemented([&] { _ZN3WTF5MutexD1Ev(guardMutex); }, "_ZN3WTF5MutexD1Ev");
    RequireNotImplemented([&] { _ZN3WTF5MutexD1Ev(readOnlyMutex); }, "_ZN3WTF5MutexD1Ev");
    RequireNotImplemented([&] { _ZN3WTF5MutexD2Ev(splitMutex); }, "_ZN3WTF5MutexD2Ev");

    RequireNotImplemented([&] { _ZN3WTF5Mutex4lockEv(guardMutex); }, "_ZN3WTF5Mutex4lockEv");
    RequireNotImplemented([&] { _ZN3WTF5Mutex4lockEv(readOnlyMutex); }, "_ZN3WTF5Mutex4lockEv");
    RequireNotImplemented([&] { _ZN3WTF5Mutex4lockEv(misalignedMutex); }, "_ZN3WTF5Mutex4lockEv");

    RequireNotImplemented([&] { static_cast<void>(_ZN3WTF5Mutex7tryLockEv(guardMutex)); }, "_ZN3WTF5Mutex7tryLockEv");
    RequireNotImplemented([&] { static_cast<void>(_ZN3WTF5Mutex7tryLockEv(readOnlyMutex)); }, "_ZN3WTF5Mutex7tryLockEv");

    RequireNotImplemented([&] { _ZN3WTF5Mutex6unlockEv(guardMutex); }, "_ZN3WTF5Mutex6unlockEv");
    RequireNotImplemented([&] { _ZN3WTF5Mutex6unlockEv(splitMutex); }, "_ZN3WTF5Mutex6unlockEv");
    RequireNotImplemented([&] { _ZN3WTF5Mutex6unlockEv(misalignedMutex); }, "_ZN3WTF5Mutex6unlockEv");

    RequireNotImplemented([&] { _ZN3WTF15ThreadConditionD1Ev(guardCondition); }, "_ZN3WTF15ThreadConditionD1Ev");
    RequireNotImplemented([&] { _ZN3WTF15ThreadConditionD1Ev(readOnlyCondition); }, "_ZN3WTF15ThreadConditionD1Ev");
    RequireNotImplemented([&] { _ZN3WTF15ThreadConditionD2Ev(splitCondition); }, "_ZN3WTF15ThreadConditionD2Ev");

    RequireNotImplemented([&] { _ZN3WTF15ThreadCondition6signalEv(guardCondition); }, "_ZN3WTF15ThreadCondition6signalEv");
    RequireNotImplemented([&] { _ZN3WTF15ThreadCondition6signalEv(readOnlyCondition); }, "_ZN3WTF15ThreadCondition6signalEv");
    RequireNotImplemented([&] { _ZN3WTF15ThreadCondition6signalEv(misalignedCondition); }, "_ZN3WTF15ThreadCondition6signalEv");

    RequireNotImplemented([&] { _ZN3WTF15ThreadCondition9broadcastEv(guardCondition); }, "_ZN3WTF15ThreadCondition9broadcastEv");
    RequireNotImplemented([&] { _ZN3WTF15ThreadCondition9broadcastEv(readOnlyCondition); }, "_ZN3WTF15ThreadCondition9broadcastEv");

    Mutex mutex{};
    _ZN3WTF5MutexC1Ev(&mutex);
    ThreadCondition condition{};

    RequireNotImplemented([&] { _ZN3WTF15ThreadCondition4waitERNS_5MutexE(guardCondition, &mutex); }, "_ZN3WTF15ThreadCondition4waitERNS_5MutexE");
    RequireNotImplemented([&] { _ZN3WTF15ThreadCondition4waitERNS_5MutexE(readOnlyCondition, &mutex); }, "_ZN3WTF15ThreadCondition4waitERNS_5MutexE");
    RequireNotImplemented([&] { _ZN3WTF15ThreadCondition4waitERNS_5MutexE(&condition, guardMutex); }, "_ZN3WTF15ThreadCondition4waitERNS_5MutexE");
    RequireNotImplemented([&] { _ZN3WTF15ThreadCondition4waitERNS_5MutexE(&condition, misalignedMutex); }, "_ZN3WTF15ThreadCondition4waitERNS_5MutexE");

    RequireInvalidArgument([&] {
        static_cast<void>(_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(guardCondition, &mutex, {std::numeric_limits<double>::quiet_NaN()}));
    });
    RequireNotImplemented([&] { static_cast<void>(_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(guardCondition, &mutex, After(1.0))); }, "_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE");
    RequireNotImplemented([&] { static_cast<void>(_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(readOnlyCondition, &mutex, After(1.0))); }, "_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE");
    RequireNotImplemented([&] { static_cast<void>(_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(splitCondition, &mutex, {-std::numeric_limits<double>::infinity()})); }, "_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE");
    RequireNotImplemented([&] { static_cast<void>(_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, guardMutex, After(1.0))); }, "_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE");
    RequireNotImplemented([&] { static_cast<void>(_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, splitMutex, After(1.0))); }, "_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE");
    RequireNotImplemented([&] { static_cast<void>(_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(&condition, misalignedMutex, After(1.0))); }, "_ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE");

    _ZN3WTF15ThreadConditionD1Ev(&condition);
    _ZN3WTF5MutexD1Ev(&mutex);
}

}

int main() {
    TestMutex();
    TestCounterUnderContention();
    TestSignalAndBroadcast();
    TestTimedWait();
    TestNullObjects();
    TestGuestPointers();
    std::puts("WTF threading tests passed");
    return 0;
}
