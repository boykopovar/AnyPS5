#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfRunLoopExports.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using NKWebKit::WtfAbi::RunLoop;
using NKWebKit::WtfAbi::RunLoopCycleResult;
using NKWebKit::WtfAbi::RunLoopDispatchTimerRef;
using NKWebKit::WtfAbi::RunLoopTimerBase;
using NKWebKit::WtfAbi::RunLoopTimerBaseVtable;
using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidCallableWrapperVtable;
using NKWebKit::WtfAbi::VoidFunction;

void Require(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", what);
        std::abort();
    }
}

template<typename Exception, typename Body>
void RequireThrows(Body body, const char* what) {
    bool threw = false;
    try {
        body();
    } catch (const Exception&) {
        threw = true;
    }
    Require(threw, what);
}

struct GuestCallable {
    VoidCallableWrapper base;
    std::function<void()> action;
    std::atomic<int> calls{0};
    std::atomic<int> deletions{0};
};

GuestCallable* Self(VoidCallableWrapper* self) {
    return reinterpret_cast<GuestCallable*>(self);
}

void APS5_VABI CallableCompleteDestructor(VoidCallableWrapper*) {}

void APS5_VABI CallableDeletingDestructor(VoidCallableWrapper* self) {
    Self(self)->deletions.fetch_add(1);
}

void APS5_VABI CallableCall(VoidCallableWrapper* self) {
    GuestCallable* callable = Self(self);
    callable->calls.fetch_add(1);
    if (callable->action) {
        callable->action();
    }
}

const VoidCallableWrapperVtable kCallableVtable{CallableCompleteDestructor, CallableDeletingDestructor, CallableCall};

void Arm(GuestCallable& callable, std::function<void()> action) {
    callable.base.vtable = &kCallableVtable;
    callable.action = std::move(action);
}

RunLoop* Main() {
    return _ZN3WTF7RunLoop4mainEv();
}

void Dispatch(RunLoop* runLoop, GuestCallable& callable) {
    VoidFunction function{&callable.base};
    _ZN3WTF7RunLoop8dispatchEONS_8FunctionIFvvEEE(runLoop, &function);
    Require(function.callableWrapper == nullptr, "dispatch takes the function");
}

void StopMain() {
    _ZN3WTF7RunLoop4stopEv(Main());
}

void Cycle() {
    Require(_ZN3WTF7RunLoop5cycleEj(0) == RunLoopCycleResult::Continue, "cycle returns Continue");
}

double Now() {
    return _ZN3WTF13MonotonicTime3nowEv().m_value;
}

struct GuestTimer {
    RunLoopTimerBase base;
    std::atomic<int> fired{0};
    std::function<void(GuestTimer&)> onFired;
};

void APS5_VABI GuestTimerCompleteDestructor(RunLoopTimerBase* self) {
    _ZN3WTF7RunLoop9TimerBaseD2Ev(self);
}

void APS5_VABI GuestTimerDeletingDestructor(RunLoopTimerBase* self) {
    GuestTimerCompleteDestructor(self);
}

void APS5_VABI GuestTimerFired(RunLoopTimerBase* self) {
    GuestTimer* timer = reinterpret_cast<GuestTimer*>(self);
    timer->fired.fetch_add(1);
    if (timer->onFired) {
        timer->onFired(*timer);
    }
}

const RunLoopTimerBaseVtable kGuestTimerVtable{GuestTimerCompleteDestructor, GuestTimerDeletingDestructor, GuestTimerFired};

void Construct(GuestTimer& timer, RunLoop* runLoop) {
    _ZN3WTF7RunLoop9TimerBaseC2ERS0_(&timer.base, runLoop);
    Require(timer.base.vtable != nullptr, "TimerBase constructor installs its vtable");
    Require(timer.base.runLoop == runLoop, "TimerBase keeps its run loop");
    Require(timer.base.scheduledTask != nullptr, "TimerBase creates its scheduled task");
    timer.base.vtable = &kGuestTimerVtable;
}

void Destroy(GuestTimer& timer) {
    _ZN3WTF7RunLoop9TimerBaseD2Ev(&timer.base);
}

void Start(GuestTimer& timer, double seconds, bool repeat) {
    _ZN3WTF7RunLoop9TimerBase5startENS_7SecondsEb(&timer.base, WTF::Seconds{seconds}, repeat);
}

bool IsActive(const RunLoopTimerBase* timer) {
    return _ZNK3WTF7RunLoop9TimerBase8isActiveEv(timer);
}

double SecondsUntilFire(const RunLoopTimerBase* timer) {
    return _ZNK3WTF7RunLoop9TimerBase16secondsUntilFireEv(timer).m_value;
}

void RunWithStopperAfter(double seconds) {
    GuestTimer stopper;
    Construct(stopper, Main());
    stopper.onFired = [](GuestTimer&) { StopMain(); };
    Start(stopper, seconds, false);
    _ZN3WTF7RunLoop3runEv();
    Require(stopper.fired.load() == 1, "stopper fired once");
    Destroy(stopper);
}

void TestMainAndCurrent() {
    RequireThrows<std::logic_error>([] { _ZN3WTF7RunLoop4mainEv(); }, "main before initializeMainThread throws");
    Require(!_ZN3WTF13isMainRunLoopEv(), "no main run loop before initialization");
    RunLoop* early = _ZN3WTF7RunLoop7currentEv();
    Require(early != nullptr, "current creates a run loop");
    Require(early->refCount.load() == 1, "the thread holder owns one reference");
    _ZN3WTF20initializeMainThreadEv();
    Require(Main() == early, "main is the initializing thread's current loop");
    Require(Main() == Main(), "main is stable");
    Require(_ZN3WTF7RunLoop7currentEv() == Main(), "current on the main thread is main");
    Require(_ZN3WTF13isMainRunLoopEv(), "isMainRunLoop on the main thread");
    WKRunLoopInitializeMain();
    Require(Main() == early, "WKRunLoopInitializeMain keeps the main loop");
    _ZN3WTF20initializeMainThreadEv();
    Require(Main() == early, "initializeMainThread again keeps the main loop");
    bool workerInitialized = false;
    std::thread worker([&] {
        _ZN3WTF20initializeMainThreadEv();
        workerInitialized = !_ZN3WTF12isMainThreadEv() && !_ZN3WTF13isMainRunLoopEv();
    });
    worker.join();
    Require(workerInitialized, "initializeMainThread from another thread keeps the first main thread");
    Require(Main() == early && _ZN3WTF12isMainThreadEv(), "main thread and loop are unchanged after a worker initializes");
}

void TestCurrentOnOtherThread() {
    RunLoop* workerLoop = nullptr;
    bool stable = false;
    bool workerIsMain = true;
    std::thread worker([&] {
        workerLoop = _ZN3WTF7RunLoop7currentEv();
        stable = _ZN3WTF7RunLoop7currentEv() == workerLoop;
        workerIsMain = _ZN3WTF13isMainRunLoopEv();
        workerLoop->refCount.fetch_add(1);
    });
    worker.join();
    Require(workerLoop != nullptr && workerLoop != Main(), "another thread gets its own loop");
    Require(stable, "current is stable per thread");
    Require(!workerIsMain, "a worker loop is not the main run loop");
    Require(workerLoop->refCount.load() == 1, "thread exit releases the holder reference");

    GuestCallable queued;
    Arm(queued, nullptr);
    VoidFunction function{&queued.base};
    workerLoop->vtable->dispatch(workerLoop, &function);
    Require(function.callableWrapper == nullptr, "vtable dispatch takes the function");
    if (workerLoop->refCount.fetch_sub(1) == 1) {
        workerLoop->refCount.store(1);
        workerLoop->vtable->deletingDestructor(workerLoop);
    }
    Require(queued.calls.load() == 0, "a destroyed loop never runs its queue");
    Require(queued.deletions.load() == 1, "destroying a loop destroys its queued functions");
}

void TestDispatchOrder() {
    std::vector<int> order;
    GuestCallable first;
    GuestCallable second;
    GuestCallable third;
    Arm(first, [&] { order.push_back(1); });
    Arm(second, [&] { order.push_back(2); });
    Arm(third, [&] {
        order.push_back(3);
        StopMain();
    });
    Dispatch(Main(), first);
    Dispatch(Main(), second);
    Dispatch(Main(), third);
    _ZN3WTF7RunLoop3runEv();
    Require(order == std::vector<int>({1, 2, 3}), "dispatch runs in FIFO order");
    Require(first.deletions.load() == 1 && second.deletions.load() == 1 && third.deletions.load() == 1, "each function is destroyed after running");
}

void TestDispatchFromWorkerWakesRun() {
    GuestCallable stopper;
    std::thread::id caller;
    Arm(stopper, [&] {
        caller = std::this_thread::get_id();
        StopMain();
    });
    std::thread worker([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Dispatch(Main(), stopper);
    });
    _ZN3WTF7RunLoop3runEv();
    worker.join();
    Require(stopper.calls.load() == 1, "worker dispatch wakes run");
    Require(caller == std::this_thread::get_id(), "dispatched function runs on the loop thread");
}

void TestNestedRun() {
    std::vector<std::string> log;
    GuestCallable inner;
    GuestCallable outer;
    Arm(inner, [&] {
        log.push_back("inner");
        StopMain();
    });
    Arm(outer, [&] {
        log.push_back("outer begin");
        Dispatch(Main(), inner);
        _ZN3WTF7RunLoop3runEv();
        log.push_back("outer end");
        StopMain();
    });
    Dispatch(Main(), outer);
    _ZN3WTF7RunLoop3runEv();
    Require(log == std::vector<std::string>({"outer begin", "inner", "outer end"}), "stop ends the innermost run");
}

void TestStopWithoutRunningLoop() {
    StopMain();
    GuestCallable task;
    Arm(task, [] { StopMain(); });
    Dispatch(Main(), task);
    _ZN3WTF7RunLoop3runEv();
    Require(task.calls.load() == 1, "stop without a running loop does not stop a later run");
}

void TestThrowingFunctionLeavesRun() {
    GuestCallable thrower;
    GuestCallable follower;
    Arm(thrower, [] { throw std::runtime_error("dispatched function failed"); });
    Arm(follower, [] { StopMain(); });
    Dispatch(Main(), thrower);
    Dispatch(Main(), follower);
    RequireThrows<std::runtime_error>([] { _ZN3WTF7RunLoop3runEv(); }, "an exception from a dispatched function leaves run");
    Require(thrower.deletions.load() == 1, "the throwing function is destroyed");
    Require(follower.calls.load() == 0, "functions after the throwing one wait for the next run");
    StopMain();
    _ZN3WTF7RunLoop3runEv();
    Require(follower.calls.load() == 1, "stop after a throwing run does not stop the next run");
    Require(follower.deletions.load() == 1, "the remaining function is destroyed after running");
}

void TestThrowingTimerLeavesRun() {
    GuestTimer thrower;
    GuestTimer repeating;
    GuestTimer sibling;
    Construct(thrower, Main());
    Construct(repeating, Main());
    Construct(sibling, Main());
    thrower.onFired = [](GuestTimer&) { throw std::runtime_error("timer failed"); };
    repeating.onFired = [](GuestTimer& self) {
        if (self.fired.load() == 1) {
            throw std::runtime_error("repeating timer failed");
        }
    };
    Start(thrower, 0.0, false);
    Start(sibling, 0.0, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    RequireThrows<std::runtime_error>([] { _ZN3WTF7RunLoop3runEv(); }, "an exception from a timer leaves run");
    Require(thrower.fired.load() == 1, "the throwing timer fired once");
    Require(!IsActive(&thrower.base), "a one-shot timer that threw is inactive");
    Require(sibling.fired.load() == 0 && IsActive(&sibling.base), "a due timer after the throwing one stays active");
    StopMain();
    RunWithStopperAfter(0.01);
    Require(thrower.fired.load() == 1, "the throwing timer does not fire again");
    Require(sibling.fired.load() == 1, "a due timer after the throwing one fires in the next run");

    Start(repeating, 0.001, true);
    RequireThrows<std::runtime_error>([] { _ZN3WTF7RunLoop3runEv(); }, "an exception from a repeating timer leaves run");
    Require(IsActive(&repeating.base), "a repeating timer that threw stays active");
    RunWithStopperAfter(0.02);
    Require(repeating.fired.load() > 1, "a repeating timer that threw fires again");
    _ZN3WTF7RunLoop9TimerBase4stopEv(&repeating.base);
    Destroy(repeating);
    Destroy(sibling);
    Destroy(thrower);
}

void TestThrowingTimerKeepsQueuedFunctions() {
    GuestTimer thrower;
    GuestCallable follower;
    Construct(thrower, Main());
    thrower.onFired = [](GuestTimer&) { throw std::runtime_error("timer failed"); };
    Arm(follower, [] { StopMain(); });
    Start(thrower, 0.0, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Dispatch(Main(), follower);
    RequireThrows<std::runtime_error>([] { _ZN3WTF7RunLoop3runEv(); }, "an exception from a timer before queued functions leaves run");
    Require(follower.calls.load() == 0, "a function queued behind a throwing timer waits for the next run");
    _ZN3WTF7RunLoop3runEv();
    Require(follower.calls.load() == 1, "the next run performs a function queued behind a throwing timer");
    Require(follower.deletions.load() == 1, "a function queued behind a throwing timer is destroyed after running");
    Destroy(thrower);
}

void TestCycle() {
    std::vector<int> order;
    GuestCallable later;
    GuestCallable first;
    GuestCallable second;
    Arm(later, [&] { order.push_back(3); });
    Arm(first, [&] {
        order.push_back(1);
        Dispatch(Main(), later);
    });
    Arm(second, [&] { order.push_back(2); });
    Dispatch(Main(), first);
    Dispatch(Main(), second);
    Cycle();
    Require(order == std::vector<int>({1, 2}), "cycle runs one iteration");
    Cycle();
    Require(order == std::vector<int>({1, 2, 3}), "functions dispatched during a cycle run in the next");
    Cycle();
    Require(order.size() == 3, "an idle cycle returns");
}

void TestSuspendFunctionDispatch() {
    std::vector<int> order;
    GuestCallable first;
    GuestCallable second;
    Arm(first, [&] {
        order.push_back(1);
        _ZN3WTF7RunLoop38suspendFunctionDispatchForCurrentCycleEv(Main());
    });
    Arm(second, [&] { order.push_back(2); });
    Dispatch(Main(), first);
    Dispatch(Main(), second);
    Cycle();
    Require(order == std::vector<int>({1}), "suspension defers the rest of the cycle");
    _ZN3WTF7RunLoop38suspendFunctionDispatchForCurrentCycleEv(Main());
    Cycle();
    Require(order == std::vector<int>({1, 2}), "suspension is ignored while suspended functions are pending");
    Require(first.deletions.load() == 1 && second.deletions.load() == 1, "suspended functions are destroyed after running");
}

void TestWakeUpCallback() {
    GuestCallable wakeUp;
    Arm(wakeUp, nullptr);
    VoidFunction callback{&wakeUp.base};
    _ZN3WTF7RunLoop17setWakeUpCallbackEONS_8FunctionIFvvEEE(&callback);
    Require(callback.callableWrapper == nullptr, "setWakeUpCallback takes the function");

    GuestCallable first;
    GuestCallable second;
    Arm(first, nullptr);
    Arm(second, nullptr);
    Dispatch(Main(), first);
    Require(wakeUp.calls.load() == 1, "dispatch to an empty queue wakes up");
    Dispatch(Main(), second);
    Require(wakeUp.calls.load() == 1, "dispatch to a non-empty queue does not wake up again");
    Cycle();
    Require(first.calls.load() == 1 && second.calls.load() == 1, "queued functions ran");

    GuestTimer timer;
    Construct(timer, Main());
    Start(timer, 10.0, false);
    Require(wakeUp.calls.load() == 2, "starting a timer wakes up");
    _ZN3WTF7RunLoop9TimerBase4stopEv(&timer.base);
    Destroy(timer);

    VoidFunction empty{nullptr};
    _ZN3WTF7RunLoop17setWakeUpCallbackEONS_8FunctionIFvvEEE(&empty);
    Require(wakeUp.deletions.load() == 1, "replacing the callback destroys the old one");
    GuestCallable third;
    Arm(third, nullptr);
    Dispatch(Main(), third);
    Require(wakeUp.calls.load() == 2, "a cleared callback is not called");
    Cycle();
    Require(third.calls.load() == 1, "queue still runs without a callback");
}

void TestTimerConstruction() {
    const unsigned references = Main()->refCount.load();
    GuestTimer timer;
    _ZN3WTF7RunLoop9TimerBaseC2ERS0_(&timer.base, Main());
    Require(Main()->refCount.load() == references + 1, "TimerBase references its run loop");
    Require(!IsActive(&timer.base), "a new timer is inactive");
    Require(SecondsUntilFire(&timer.base) == 0.0, "an inactive timer fires in zero seconds");
    RequireThrows<std::logic_error>([&] { timer.base.vtable->fired(&timer.base); }, "TimerBase::fired is pure");
    timer.base.vtable = &kGuestTimerVtable;
    timer.base.vtable->completeDestructor(&timer.base);
    Require(Main()->refCount.load() == references, "TimerBase destructor releases its run loop");
    Require(timer.base.runLoop == nullptr && timer.base.scheduledTask == nullptr, "TimerBase destructor clears its references");
}

void TestOneShotTimer() {
    GuestTimer timer;
    Construct(timer, Main());
    timer.onFired = [](GuestTimer& self) {
        Require(!IsActive(&self.base), "a one-shot timer is inactive while firing");
        StopMain();
    };
    const double started = Now();
    Start(timer, 0.01, false);
    Require(IsActive(&timer.base), "a started timer is active");
    const double remaining = SecondsUntilFire(&timer.base);
    Require(remaining > 0.0 && remaining <= 0.01 + 1e-9, "secondsUntilFire counts down from the interval");
    _ZN3WTF7RunLoop3runEv();
    Require(Now() - started >= 0.0099, "a one-shot timer waits for its interval");
    Require(timer.fired.load() == 1, "a one-shot timer fires");
    Require(!IsActive(&timer.base), "a fired one-shot timer is inactive");
    Require(SecondsUntilFire(&timer.base) == 0.0, "a fired one-shot timer has no fire time");
    RunWithStopperAfter(0.03);
    Require(timer.fired.load() == 1, "a one-shot timer fires once");
    Destroy(timer);
}

void TestRepeatingTimer() {
    GuestTimer timer;
    Construct(timer, Main());
    timer.onFired = [](GuestTimer& self) {
        Require(IsActive(&self.base), "a repeating timer stays active while firing");
        if (self.fired.load() == 3) {
            _ZN3WTF7RunLoop9TimerBase4stopEv(&self.base);
            StopMain();
        }
    };
    Start(timer, 0.005, true);
    _ZN3WTF7RunLoop3runEv();
    Require(timer.fired.load() == 3, "a repeating timer fires until stopped");
    Require(!IsActive(&timer.base), "a stopped repeating timer is inactive");
    RunWithStopperAfter(0.02);
    Require(timer.fired.load() == 3, "a stopped repeating timer stays stopped");
    Destroy(timer);
}

void TestRestartAndStop() {
    GuestTimer timer;
    Construct(timer, Main());
    timer.onFired = [](GuestTimer&) { StopMain(); };
    Start(timer, 10.0, false);
    const double far = SecondsUntilFire(&timer.base);
    Require(far > 9.0 && far <= 10.0 + 1e-9, "secondsUntilFire before restart");
    Start(timer, 0.01, false);
    Require(SecondsUntilFire(&timer.base) <= 0.01 + 1e-9, "start reschedules an active timer");
    _ZN3WTF7RunLoop3runEv();
    Require(timer.fired.load() == 1, "a restarted timer fires at the new time");

    Start(timer, 0.01, false);
    _ZN3WTF7RunLoop9TimerBase4stopEv(&timer.base);
    Require(!IsActive(&timer.base), "stop deactivates");
    Require(SecondsUntilFire(&timer.base) == 0.0, "a stopped timer has no fire time");
    RunWithStopperAfter(0.03);
    Require(timer.fired.load() == 1, "a stopped timer does not fire");
    Destroy(timer);
}

void TestDestroyActiveTimer() {
    const unsigned references = Main()->refCount.load();
    GuestTimer timer;
    Construct(timer, Main());
    Start(timer, 0.01, false);
    Destroy(timer);
    Require(Main()->refCount.load() == references, "destroying a timer releases its run loop");
    RunWithStopperAfter(0.03);
    Require(timer.fired.load() == 0, "destroying an active timer unschedules it");
}

void TestRepeatingTimerDestroysItselfWhenFired() {
    const unsigned references = Main()->refCount.load();
    GuestTimer timer;
    Construct(timer, Main());
    timer.onFired = [](GuestTimer& self) { Destroy(self); };
    Start(timer, 0.001, true);
    RunWithStopperAfter(0.03);
    Require(timer.fired.load() == 1, "a repeating timer destroyed by its own callback does not fire again");
    Require(Main()->refCount.load() == references, "a timer destroyed by its own callback releases its run loop");
}

void TestTimersFireInScheduleOrder() {
    std::vector<int> order;
    GuestTimer late;
    GuestTimer early;
    Construct(late, Main());
    Construct(early, Main());
    late.onFired = [&](GuestTimer&) {
        order.push_back(2);
        StopMain();
    };
    early.onFired = [&](GuestTimer&) { order.push_back(1); };
    Start(late, 0.02, false);
    Start(early, 0.005, false);
    _ZN3WTF7RunLoop3runEv();
    Require(order == std::vector<int>({1, 2}), "timers fire by fire time");
    Destroy(early);
    Destroy(late);
}

void TestDispatchAfter() {
    const unsigned references = Main()->refCount.load();
    GuestCallable callable;
    double firedAt = 0.0;
    Arm(callable, [&] {
        firedAt = Now();
        StopMain();
    });
    VoidFunction function{&callable.base};
    RunLoopDispatchTimerRef timer{nullptr};
    const double started = Now();
    Require(_ZN3WTF7RunLoop13dispatchAfterENS_7SecondsEONS_8FunctionIFvvEEE(&timer, Main(), WTF::Seconds{0.02}, &function) == &timer, "dispatchAfter returns its result");
    Require(function.callableWrapper == nullptr, "dispatchAfter takes the function");
    Require(timer.ptr != nullptr, "dispatchAfter returns a timer");
    Require(timer.ptr->refCount.load() == 2, "the returned timer and its pending function hold references");
    Require(timer.ptr->timerBase.runLoop == Main(), "the timer belongs to the loop");
    Require(Main()->refCount.load() == references + 1, "the timer references the loop");
    Require(IsActive(&timer.ptr->timerBase), "the timer is scheduled");
    _ZN3WTF7RunLoop3runEv();
    Require(callable.calls.load() == 1, "dispatchAfter fires once");
    Require(firedAt - started >= 0.0199, "dispatchAfter waits for the delay");
    Require(timer.ptr->refCount.load() == 1, "firing drops the pending reference");
    Require(!IsActive(&timer.ptr->timerBase), "a fired dispatch timer is inactive");
    Require(callable.deletions.load() == 0, "the timer keeps its function until destroyed");
    if (timer.ptr->refCount.fetch_sub(1) == 1) {
        timer.ptr->refCount.store(1);
        timer.ptr->timerBase.vtable->deletingDestructor(&timer.ptr->timerBase);
    }
    Require(callable.deletions.load() == 1, "destroying the timer destroys its function");
    Require(Main()->refCount.load() == references, "destroying the timer releases the loop");
}

void TestDispatchAfterNegativeDelay() {
    GuestCallable callable;
    Arm(callable, [] { StopMain(); });
    VoidFunction function{&callable.base};
    RunLoopDispatchTimerRef timer{nullptr};
    _ZN3WTF7RunLoop13dispatchAfterENS_7SecondsEONS_8FunctionIFvvEEE(&timer, Main(), WTF::Seconds{-1.0}, &function);
    Require(SecondsUntilFire(&timer.ptr->timerBase) == 0.0, "a negative delay is clamped to zero");
    _ZN3WTF7RunLoop3runEv();
    Require(callable.calls.load() == 1, "a clamped dispatchAfter fires");
    timer.ptr->timerBase.vtable->deletingDestructor(&timer.ptr->timerBase);
    Require(callable.deletions.load() == 1, "the clamped timer is destroyed");
}

void APS5_VABI AppendTwo(void* context) {
    static_cast<std::vector<int>*>(context)->push_back(2);
}

void TestSharedMainQueue() {
    std::vector<int> order;
    GuestCallable first;
    GuestCallable third;
    Arm(first, [&] { order.push_back(1); });
    Arm(third, [&] { order.push_back(3); });
    Dispatch(Main(), first);
    WKRunLoopCallOnMainThread(AppendTwo, &order);
    VoidFunction function{&third.base};
    _ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE(&function);
    Cycle();
    Require(order == std::vector<int>({1, 2, 3}), "RunLoop, WKRunLoop and callOnMainThread share one main queue");
}

void TestNullArguments() {
    GuestTimer timer;
    VoidFunction empty{nullptr};
    RunLoopDispatchTimerRef result{nullptr};
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop8dispatchEONS_8FunctionIFvvEEE(nullptr, nullptr); }, "dispatch on a null loop");
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop8dispatchEONS_8FunctionIFvvEEE(Main(), nullptr); }, "dispatch of a null reference");
    RequireThrows<std::invalid_argument>([&] { _ZN3WTF7RunLoop8dispatchEONS_8FunctionIFvvEEE(Main(), &empty); }, "dispatch of an empty function");
    RequireThrows<std::invalid_argument>([&] { _ZN3WTF7RunLoop13dispatchAfterENS_7SecondsEONS_8FunctionIFvvEEE(&result, Main(), WTF::Seconds{0.0}, &empty); }, "dispatchAfter of an empty function");
    RequireThrows<std::invalid_argument>([&] { _ZN3WTF7RunLoop13dispatchAfterENS_7SecondsEONS_8FunctionIFvvEEE(&result, nullptr, WTF::Seconds{0.0}, &empty); }, "dispatchAfter on a null loop");
    RequireThrows<std::invalid_argument>([&] { _ZN3WTF7RunLoop13dispatchAfterENS_7SecondsEONS_8FunctionIFvvEEE(nullptr, Main(), WTF::Seconds{0.0}, &empty); }, "dispatchAfter without a result");
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop4stopEv(nullptr); }, "stop on a null loop");
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop38suspendFunctionDispatchForCurrentCycleEv(nullptr); }, "suspend on a null loop");
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop17setWakeUpCallbackEONS_8FunctionIFvvEEE(nullptr); }, "setWakeUpCallback of a null reference");
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop9TimerBaseC2ERS0_(nullptr, Main()); }, "TimerBase constructor on a null timer");
    RequireThrows<std::invalid_argument>([&] { _ZN3WTF7RunLoop9TimerBaseC2ERS0_(&timer.base, nullptr); }, "TimerBase constructor on a null loop");
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop9TimerBaseD1Ev(nullptr); }, "TimerBase D1 on a null timer");
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop9TimerBaseD2Ev(nullptr); }, "TimerBase D2 on a null timer");
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop9TimerBase5startENS_7SecondsEb(nullptr, WTF::Seconds{0.0}, false); }, "start on a null timer");
    RequireThrows<std::invalid_argument>([] { _ZN3WTF7RunLoop9TimerBase4stopEv(nullptr); }, "stop on a null timer");
    RequireThrows<std::invalid_argument>([] { _ZNK3WTF7RunLoop9TimerBase8isActiveEv(nullptr); }, "isActive on a null timer");
    RequireThrows<std::invalid_argument>([] { _ZNK3WTF7RunLoop9TimerBase16secondsUntilFireEv(nullptr); }, "secondsUntilFire on a null timer");
}

}

int main() {
    TestMainAndCurrent();
    TestCurrentOnOtherThread();
    TestDispatchOrder();
    TestDispatchFromWorkerWakesRun();
    TestNestedRun();
    TestStopWithoutRunningLoop();
    TestThrowingFunctionLeavesRun();
    TestThrowingTimerLeavesRun();
    TestThrowingTimerKeepsQueuedFunctions();
    TestCycle();
    TestSuspendFunctionDispatch();
    TestWakeUpCallback();
    TestTimerConstruction();
    TestOneShotTimer();
    TestRepeatingTimer();
    TestRestartAndStop();
    TestDestroyActiveTimer();
    TestRepeatingTimerDestroysItselfWhenFired();
    TestTimersFireInScheduleOrder();
    TestDispatchAfter();
    TestDispatchAfterNegativeDelay();
    TestSharedMainQueue();
    TestNullArguments();

    std::printf("All libSceNKWebKit WtfRunLoop tests passed!\n");
    return 0;
}
