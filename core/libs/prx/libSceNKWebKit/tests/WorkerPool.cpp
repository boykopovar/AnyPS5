#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidCallableWrapperVtable;
using NKWebKit::WtfAbi::VoidFunction;
using NKWebKit::WtfAbi::WorkerPool;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "worker pool check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

constexpr auto construct = &_ZN3WTF10WorkerPoolC1ENS_12ASCIILiteralEjNS_7SecondsE;
constexpr auto destroy = &_ZN3WTF10WorkerPoolD1Ev;
constexpr auto postTask = &_ZN3WTF10WorkerPool8postTaskEONS_8FunctionIFvvEEE;
constexpr double kForever = std::numeric_limits<double>::infinity();
static const char kName[] = "nkwebkit test pool";

static std::mutex logMutex;
static std::vector<std::string> eventLog;
static std::atomic<int> liveCallables{0};

static void Log(const std::string& event) {
    std::lock_guard lock(logMutex);
    eventLog.push_back(event);
}

static std::vector<std::string> TakeLog() {
    std::lock_guard lock(logMutex);
    std::vector<std::string> taken;
    taken.swap(eventLog);
    return taken;
}

static const char* Where() {
    return _ZN3WTF12isMainThreadEv() ? "main" : "other";
}

struct Callable {
    VoidCallableWrapper base;
    std::string name;
    std::function<void()> body;
};

static void APS5_VABI CallableCompleteDestructor(VoidCallableWrapper*) { Require(false); }
static void APS5_VABI CallableDeletingDestructor(VoidCallableWrapper* self) {
    auto* callable = reinterpret_cast<Callable*>(self);
    Log("delete:" + callable->name + ":" + Where());
    delete callable;
    --liveCallables;
}
static void APS5_VABI CallableCall(VoidCallableWrapper* self) {
    auto* callable = reinterpret_cast<Callable*>(self);
    Log("call:" + callable->name + ":" + Where());
    if (callable->body) callable->body();
}
static const VoidCallableWrapperVtable callableVtable = {CallableCompleteDestructor, CallableDeletingDestructor, CallableCall};

static VoidFunction Make(const std::string& name, std::function<void()> body = {}) {
    ++liveCallables;
    return VoidFunction{&(new Callable{{&callableVtable}, name, std::move(body)})->base};
}

static void Wait(const std::atomic<bool>& flag) {
    while (!flag) std::this_thread::yield();
}

static void WaitUntilIdle() {
    while (liveCallables.load() != 0) std::this_thread::yield();
}

static void Settle() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static bool Same(const std::vector<std::string>& actual, std::initializer_list<const char*> expected) {
    if (actual.size() != expected.size()) return false;
    std::size_t index = 0;
    for (const char* event : expected)
        if (actual[index++] != event) return false;
    return true;
}

struct Pool {
    alignas(16) unsigned char bytes[160];

    explicit Pool(unsigned workers) {
        std::memset(bytes, 0xA5, sizeof(bytes));
        construct(get(), kName, workers, kForever);
    }

    ~Pool() {
        if (get()->state) destroy(get());
        Require(Untouched());
    }

    WorkerPool* get() {
        return reinterpret_cast<WorkerPool*>(bytes);
    }

    bool Untouched() const {
        for (std::size_t index = 4; index < sizeof(bytes); ++index) {
            const bool field = (index >= 8 && index < 16) || (index >= 96 && index < 104);
            if (!field && bytes[index] != 0xA5) return false;
        }
        return true;
    }

    void Post(const std::string& name, std::function<void()> body = {}) {
        VoidFunction task = Make(name, std::move(body));
        postTask(get(), &task);
        Require(task.callableWrapper == nullptr);
    }
};

static void TestConstruction() {
    Pool pool(3);
    Require(pool.get()->refCount.load() == 1 && pool.get()->name == kName && pool.get()->state != nullptr);
    Require(pool.Untouched());
    destroy(pool.get());
    Require(pool.get()->state == nullptr && pool.get()->refCount.load() == 1 && pool.get()->name == kName);

    std::memset(pool.bytes, 0xA5, sizeof(pool.bytes));
    _ZN3WTF10WorkerPoolC2ENS_12ASCIILiteralEjNS_7SecondsE(pool.get(), nullptr, 1, 0.5);
    Require(pool.get()->refCount.load() == 1 && pool.get()->name == nullptr && pool.get()->state != nullptr);
    std::atomic<bool> ran{false};
    pool.Post("only", [&] { ran = true; });
    _ZN3WTF10WorkerPoolD2Ev(pool.get());
    Require(pool.get()->state == nullptr && ran);
    Require(Same(TakeLog(), {"call:only:other", "delete:only:other"}));
}

static void TestOneWorker() {
    Pool pool(1);
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
    std::thread::id first;
    std::thread::id second;
    pool.Post("gate", [&] {
        started = true;
        while (!release) std::this_thread::yield();
    });
    Wait(started);
    pool.Post("a", [&] { first = std::this_thread::get_id(); });
    pool.Post("b", [&] { second = std::this_thread::get_id(); });
    Settle();
    Require(Same(TakeLog(), {"call:gate:other"}));
    release = true;
    WaitUntilIdle();
    Require(Same(TakeLog(), {"delete:gate:other", "call:a:other", "delete:a:other", "call:b:other", "delete:b:other"}));
    Require(first == second && first != std::this_thread::get_id());

    Settle();
    std::thread::id third;
    pool.Post("c", [&] { third = std::this_thread::get_id(); });
    WaitUntilIdle();
    Require(third == first);
    TakeLog();
}

static void TestTwoWorkers() {
    Pool pool(2);
    std::atomic<bool> firstStarted{false};
    std::atomic<bool> secondStarted{false};
    std::atomic<int> started{0};
    std::atomic<bool> release{false};
    std::thread::id first;
    std::thread::id second;
    pool.Post("first", [&] {
        first = std::this_thread::get_id();
        firstStarted = true;
        ++started;
        while (!secondStarted || !release) std::this_thread::yield();
    });
    pool.Post("second", [&] {
        second = std::this_thread::get_id();
        secondStarted = true;
        ++started;
        while (!firstStarted || !release) std::this_thread::yield();
    });
    pool.Post("third", [&] { ++started; });
    Wait(firstStarted);
    Wait(secondStarted);
    Settle();
    Require(started.load() == 2 && first != second);
    release = true;
    WaitUntilIdle();
    Require(started.load() == 3);
    TakeLog();

    Settle();
    std::thread::id reused;
    pool.Post("reused", [&] { reused = std::this_thread::get_id(); });
    WaitUntilIdle();
    Require(reused == first || reused == second);
    TakeLog();
}

static void TestTheDestructorWaits() {
    std::atomic<int> ran{0};
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
    std::thread helper;
    {
        Pool pool(1);
        pool.Post("gate", [&] {
            started = true;
            while (!release) std::this_thread::yield();
            ++ran;
        });
        for (int index = 0; index < 4; ++index) pool.Post("queued", [&] { ++ran; });
        Wait(started);
        helper = std::thread([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            release = true;
        });
        destroy(pool.get());
        Require(release && ran.load() == 5 && liveCallables.load() == 0);
    }
    helper.join();
    Require(TakeLog().size() == 10);
}

static void TestNoWorker() {
    {
        Pool pool(0);
        pool.Post("stranded");
        Settle();
        Require(TakeLog().empty() && liveCallables.load() == 1);
    }
    Require(Same(TakeLog(), {"delete:stranded:main"}));
}

static void TestInvalidArguments() {
    Pool pool(1);
    VoidFunction empty{nullptr};
    VoidFunction task = Make("kept");
    Require(ThrowsInvalidArgument([&] { construct(nullptr, kName, 1, kForever); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10WorkerPoolC2ENS_12ASCIILiteralEjNS_7SecondsE(nullptr, kName, 1, kForever); }));
    Require(ThrowsInvalidArgument([&] { destroy(nullptr); }));
    Require(ThrowsInvalidArgument([&] { postTask(nullptr, &task); }));
    Require(ThrowsInvalidArgument([&] { postTask(pool.get(), nullptr); }));
    Require(ThrowsInvalidArgument([&] { postTask(pool.get(), &empty); }));
    destroy(pool.get());
    Require(ThrowsInvalidArgument([&] { postTask(pool.get(), &task); }));
    Require(ThrowsInvalidArgument([&] { destroy(pool.get()); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10WorkerPoolD2Ev(pool.get()); }));
    Require(task.callableWrapper != nullptr && TakeLog().empty());
    task.callableWrapper->vtable->deletingDestructor(task.callableWrapper);
    TakeLog();
    Require(liveCallables.load() == 0);
}

int main() {
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(20));
        std::fprintf(stderr, "worker pool test timed out\n");
        std::fflush(stderr);
        std::_Exit(3);
    }).detach();
    _ZN3WTF20initializeMainThreadEv();
    TestConstruction();
    TestOneWorker();
    TestTwoWorkers();
    TestTheDestructorWaits();
    TestNoWorker();
    TestInvalidArguments();
    std::puts("libSceNKWebKit worker pool tests passed");
    return 0;
}
