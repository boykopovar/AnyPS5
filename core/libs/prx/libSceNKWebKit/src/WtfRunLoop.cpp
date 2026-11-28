// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/RunLoop.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/generic/RunLoopGeneric.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2016 Konstantin Tokavev <annulen@yandex.ru>
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/WtfRunLoopExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace NKWebKit::WtfAbi {

struct RunLoopScheduledTask {
    explicit RunLoopScheduledTask(RunLoopTimerBase* owner) : timer(owner) {}

    void Ref() { refCount.fetch_add(1); }
    void Deref() {
        if (refCount.fetch_sub(1) == 1) {
            delete this;
        }
    }

    void Fired() {
        if (!IsActive()) {
            return;
        }
        if (!isRepeating) {
            Deactivate();
        }
        if (IsActive()) {
            UpdateReadyTime();
        }
        timer->vtable->fired(timer);
    }

    void UpdateReadyTime() {
        scheduledTimePoint = _ZN3WTF13MonotonicTime3nowEv().m_value;
        if (!fireInterval) {
            return;
        }
        scheduledTimePoint += fireInterval;
    }

    bool IsActive() const { return isActive.load(); }

    void Activate(double interval, bool repeating) {
        fireInterval = interval;
        isRepeating = repeating;
        UpdateReadyTime();
        isActive.store(true);
    }

    void Deactivate() { isActive.store(false); }

    std::atomic<unsigned> refCount{1};
    RunLoopTimerBase* timer;
    double scheduledTimePoint = 0.0;
    double fireInterval = 0.0;
    std::atomic<bool> isActive{false};
    bool isRepeating = false;
    bool isScheduled = false;
    std::multimap<double, RunLoopScheduledTask*>::iterator schedulePosition;
};

enum class RunLoopStatus {
    Clear,
    Stopping,
};

struct RunLoopImpl {
    std::deque<OwnedFunction> currentIteration;
    std::mutex nextIterationLock;
    std::deque<OwnedFunction> nextIteration;
    bool isFunctionDispatchSuspended = false;
    bool hasSuspendedFunctions = false;
    std::mutex loopLock;
    std::condition_variable readyToRun;
    std::condition_variable stopCondition;
    std::multimap<double, RunLoopScheduledTask*> schedules;
    std::vector<RunLoopStatus*> mainLoops;
    bool shutdown = false;
    bool pendingTasks = false;
    OwnedFunction wakeUpCallback;
};

}  // namespace NKWebKit::WtfAbi

namespace {

using NKWebKit::WtfAbi::OwnedFunction;
using NKWebKit::WtfAbi::RunLoop;
using NKWebKit::WtfAbi::RunLoopCycleResult;
using NKWebKit::WtfAbi::RunLoopDispatchTimer;
using NKWebKit::WtfAbi::RunLoopDispatchTimerRef;
using NKWebKit::WtfAbi::RunLoopImpl;
using NKWebKit::WtfAbi::RunLoopScheduledTask;
using NKWebKit::WtfAbi::RunLoopStatus;
using NKWebKit::WtfAbi::RunLoopTimerBase;
using NKWebKit::WtfAbi::RunLoopTimerBaseVtable;
using NKWebKit::WtfAbi::RunLoopVtable;
using NKWebKit::WtfAbi::VoidFunction;

enum class RunMode {
    Iterate,
    Drain,
};

class ScheduledTaskRef {
public:
    explicit ScheduledTaskRef(RunLoopScheduledTask* task) : m_task(task) { m_task->Ref(); }
    ScheduledTaskRef(ScheduledTaskRef&& other) noexcept : m_task(std::exchange(other.m_task, nullptr)) {}
    ScheduledTaskRef(const ScheduledTaskRef&) = delete;
    ScheduledTaskRef& operator=(const ScheduledTaskRef&) = delete;
    ScheduledTaskRef& operator=(ScheduledTaskRef&&) = delete;
    ~ScheduledTaskRef() {
        if (RunLoopScheduledTask* task = std::exchange(m_task, nullptr)) {
            task->Deref();
        }
    }

    RunLoopScheduledTask* operator->() const { return m_task; }
    RunLoopScheduledTask& operator*() const { return *m_task; }

private:
    RunLoopScheduledTask* m_task;
};

std::atomic<RunLoop*> s_mainRunLoop{nullptr};

void APS5_VABI RunLoopCompleteDestructor(RunLoop* self);
void APS5_VABI RunLoopDeletingDestructor(RunLoop* self);
void APS5_VABI RunLoopDispatch(RunLoop* self, VoidFunction* function);

const RunLoopVtable kRunLoopVtable{RunLoopCompleteDestructor, RunLoopDeletingDestructor, RunLoopDispatch};

RunLoop* RequireRunLoop(RunLoop* runLoop) {
    if (!runLoop) {
        throw std::invalid_argument("null WTF::RunLoop");
    }
    return runLoop;
}

RunLoopTimerBase* RequireTimer(const RunLoopTimerBase* timer) {
    if (!timer) {
        throw std::invalid_argument("null WTF::RunLoop::TimerBase");
    }
    return const_cast<RunLoopTimerBase*>(timer);
}

OwnedFunction TakeFunction(VoidFunction* function) {
    if (!function) {
        throw std::invalid_argument("null WTF::Function reference");
    }
    if (!function->callableWrapper) {
        throw std::invalid_argument("null WTF::Function");
    }
    return OwnedFunction(std::exchange(function->callableWrapper, nullptr));
}

double Now() {
    return _ZN3WTF13MonotonicTime3nowEv().m_value;
}

void RefRunLoop(RunLoop* runLoop) {
    runLoop->refCount.fetch_add(1);
}

void DerefRunLoop(RunLoop* runLoop) {
    if (runLoop->refCount.fetch_sub(1) == 1) {
        runLoop->refCount.store(1);
        RunLoopDeletingDestructor(runLoop);
    }
}

RunLoop* CreateRunLoop() {
    auto runLoopImpl = std::make_unique<RunLoopImpl>();
    auto* runLoop = new (_ZN3WTF10fastMallocEm(sizeof(RunLoop))) RunLoop;
    runLoop->vtable = &kRunLoopVtable;
    runLoop->refCount.store(1);
    runLoop->impl = runLoopImpl.release();
    return runLoop;
}

void ThreadWillExit(RunLoopImpl& loop) {
    loop.currentIteration.clear();
    std::lock_guard<std::mutex> lock(loop.nextIterationLock);
    loop.nextIteration.clear();
}

class Holder {
public:
    Holder() : m_runLoop(CreateRunLoop()) {}
    Holder(const Holder&) = delete;
    Holder& operator=(const Holder&) = delete;
    ~Holder() {
        ThreadWillExit(*m_runLoop->impl);
        DerefRunLoop(m_runLoop);
    }

    RunLoop* runLoop() const { return m_runLoop; }

private:
    RunLoop* m_runLoop;
};

thread_local std::unique_ptr<Holder> t_runLoopHolder;

RunLoop* Current() {
    if (!t_runLoopHolder) {
        t_runLoopHolder = std::make_unique<Holder>();
    }
    return t_runLoopHolder->runLoop();
}

bool IsCurrent(const RunLoop* runLoop) {
    return t_runLoopHolder && runLoop == t_runLoopHolder->runLoop();
}

void ScheduleWithLock(RunLoopImpl& loop, RunLoopScheduledTask& task) {
    if (!task.isScheduled) {
        task.schedulePosition = loop.schedules.emplace(task.scheduledTimePoint, &task);
        task.isScheduled = true;
    }
}

void UnscheduleWithLock(RunLoopImpl& loop, RunLoopScheduledTask& task) {
    if (task.isScheduled) {
        loop.schedules.erase(task.schedulePosition);
        task.isScheduled = false;
    }
}

void WakeUpWithLock(RunLoopImpl& loop) {
    loop.pendingTasks = true;
    loop.readyToRun.notify_one();
    if (loop.wakeUpCallback) {
        loop.wakeUpCallback();
    }
}

void WakeUp(RunLoopImpl& loop) {
    std::lock_guard<std::mutex> lock(loop.loopLock);
    WakeUpWithLock(loop);
}

void PerformWork(RunLoopImpl& loop) {
    bool didSuspendFunctions = false;
    {
        std::lock_guard<std::mutex> lock(loop.nextIterationLock);
        while (!loop.currentIteration.empty()) {
            loop.nextIteration.push_front(std::move(loop.currentIteration.back()));
            loop.currentIteration.pop_back();
        }
        loop.currentIteration = std::exchange(loop.nextIteration, {});
    }
    while (!loop.currentIteration.empty()) {
        if (loop.isFunctionDispatchSuspended) {
            didSuspendFunctions = true;
            break;
        }
        OwnedFunction function = std::move(loop.currentIteration.front());
        loop.currentIteration.pop_front();
        function();
    }
    loop.isFunctionDispatchSuspended = false;
    loop.hasSuspendedFunctions = didSuspendFunctions;
    if (loop.hasSuspendedFunctions) {
        WakeUp(loop);
    }
}

void Dispatch(RunLoopImpl& loop, OwnedFunction function) {
    bool needsWakeup = false;
    {
        std::lock_guard<std::mutex> lock(loop.nextIterationLock);
        needsWakeup = loop.nextIteration.empty();
        loop.nextIteration.push_back(std::move(function));
    }
    if (needsWakeup) {
        WakeUp(loop);
    }
}

void LeaveLoopWithLock(RunLoopImpl& loop, RunLoopStatus* status) {
    auto position = std::find(loop.mainLoops.rbegin(), loop.mainLoops.rend(), status);
    if (position != loop.mainLoops.rend()) {
        loop.mainLoops.erase(std::next(position).base());
    }
    if (loop.mainLoops.empty()) {
        loop.stopCondition.notify_one();
    }
}

bool PopulateTasks(RunLoopImpl& loop, RunMode runMode, RunLoopStatus& statusOfThisLoop, std::deque<ScheduledTaskRef>& firedTimers) {
    std::unique_lock<std::mutex> lock(loop.loopLock);
    if (runMode == RunMode::Drain) {
        auto ready = [&] {
            return loop.shutdown || loop.pendingTasks || statusOfThisLoop == RunLoopStatus::Stopping;
        };
        if (loop.schedules.empty()) {
            loop.readyToRun.wait(lock, ready);
        } else {
            const double sleepUntil = loop.schedules.begin()->second->scheduledTimePoint;
            while (!ready()) {
                const double remaining = sleepUntil - Now();
                if (remaining <= 0.0) {
                    break;
                }
                loop.readyToRun.wait_for(lock, std::chrono::duration<double>(remaining));
            }
        }
    }
    if (statusOfThisLoop == RunLoopStatus::Stopping || loop.shutdown) {
        LeaveLoopWithLock(loop, &statusOfThisLoop);
        return false;
    }
    loop.pendingTasks = false;
    if (runMode == RunMode::Iterate) {
        statusOfThisLoop = RunLoopStatus::Stopping;
    }
    const double now = Now();
    while (!loop.schedules.empty()) {
        RunLoopScheduledTask* task = loop.schedules.begin()->second;
        if (task->scheduledTimePoint > now) {
            break;
        }
        UnscheduleWithLock(loop, *task);
        firedTimers.emplace_back(task);
    }
    return true;
}

void RunImpl(RunLoop* runLoop, RunMode runMode) {
    RunLoopImpl& loop = *runLoop->impl;
    RunLoopStatus statusOfThisLoop = RunLoopStatus::Clear;
    {
        std::lock_guard<std::mutex> lock(loop.loopLock);
        loop.mainLoops.push_back(&statusOfThisLoop);
    }
    std::deque<ScheduledTaskRef> firedTimers;
    try {
        while (true) {
            if (!PopulateTasks(loop, runMode, statusOfThisLoop, firedTimers)) {
                return;
            }
            while (!firedTimers.empty()) {
                firedTimers.front()->Fired();
                ScheduledTaskRef task = std::move(firedTimers.front());
                firedTimers.pop_front();
                std::lock_guard<std::mutex> lock(loop.loopLock);
                if (task->IsActive() && !task->isScheduled) {
                    ScheduleWithLock(loop, *task);
                }
            }
            PerformWork(loop);
        }
    } catch (...) {
        std::lock_guard<std::mutex> lock(loop.loopLock);
        for (ScheduledTaskRef& task : firedTimers) {
            if (task->IsActive() && !task->isScheduled) {
                ScheduleWithLock(loop, *task);
            }
        }
        firedTimers.clear();
        loop.pendingTasks = true;
        LeaveLoopWithLock(loop, &statusOfThisLoop);
        throw;
    }
}

void StopWithLock(RunLoopTimerBase* timer) {
    UnscheduleWithLock(*timer->runLoop->impl, *timer->scheduledTask);
    timer->scheduledTask->Deactivate();
}

void StartTimer(RunLoopTimerBase* timer, double interval, bool repeating) {
    RunLoopImpl& loop = *timer->runLoop->impl;
    std::lock_guard<std::mutex> lock(loop.loopLock);
    StopWithLock(timer);
    timer->scheduledTask->Activate(interval, repeating);
    ScheduleWithLock(loop, *timer->scheduledTask);
    WakeUpWithLock(loop);
}

void StopTimer(RunLoopTimerBase* timer) {
    std::lock_guard<std::mutex> lock(timer->runLoop->impl->loopLock);
    StopWithLock(timer);
}

void APS5_VABI TimerBaseDeletingDestructor(RunLoopTimerBase* self);
void APS5_VABI TimerBasePureFired(RunLoopTimerBase* self);

const RunLoopTimerBaseVtable kTimerBaseVtable{_ZN3WTF7RunLoop9TimerBaseD1Ev, TimerBaseDeletingDestructor, TimerBasePureFired};

void APS5_VABI TimerBaseDeletingDestructor(RunLoopTimerBase* self) {
    _ZN3WTF7RunLoop9TimerBaseD1Ev(self);
    ::operator delete(self);
}

void APS5_VABI TimerBasePureFired(RunLoopTimerBase*) {
    throw std::logic_error("pure virtual WTF::RunLoop::TimerBase::fired called");
}

RunLoopDispatchTimer* AsDispatchTimer(RunLoopTimerBase* timer) {
    return reinterpret_cast<RunLoopDispatchTimer*>(timer);
}

void APS5_VABI DispatchTimerCompleteDestructor(RunLoopTimerBase* self) {
    OwnedFunction function(std::exchange(AsDispatchTimer(self)->function.callableWrapper, nullptr));
    function.Reset();
    _ZN3WTF7RunLoop9TimerBaseD2Ev(self);
}

void APS5_VABI DispatchTimerDeletingDestructor(RunLoopTimerBase* self) {
    DispatchTimerCompleteDestructor(self);
    _ZN3WTF8fastFreeEPv(self);
}

void APS5_VABI DispatchTimerFired(RunLoopTimerBase* self) {
    NKWebKit::WtfAbi::VoidCallableWrapper* callableWrapper = AsDispatchTimer(self)->function.callableWrapper;
    callableWrapper->vtable->call(callableWrapper);
}

const RunLoopTimerBaseVtable kDispatchTimerVtable{DispatchTimerCompleteDestructor, DispatchTimerDeletingDestructor, DispatchTimerFired};

class DispatchTimerRef {
public:
    explicit DispatchTimerRef(RunLoopDispatchTimer* timer) : m_timer(timer) {}
    DispatchTimerRef(DispatchTimerRef&& other) noexcept : m_timer(std::exchange(other.m_timer, nullptr)) {}
    DispatchTimerRef(const DispatchTimerRef&) = delete;
    DispatchTimerRef& operator=(const DispatchTimerRef&) = delete;
    DispatchTimerRef& operator=(DispatchTimerRef&&) = delete;
    ~DispatchTimerRef() {
        RunLoopDispatchTimer* timer = std::exchange(m_timer, nullptr);
        if (timer && timer->refCount.fetch_sub(1) == 1) {
            timer->refCount.store(1);
            DispatchTimerDeletingDestructor(&timer->timerBase);
        }
    }

    RunLoopDispatchTimer* get() const { return m_timer; }
    RunLoopDispatchTimer* Release() { return std::exchange(m_timer, nullptr); }

private:
    RunLoopDispatchTimer* m_timer;
};

DispatchTimerRef CreateDispatchTimer(RunLoop* runLoop) {
    auto* timer = new (_ZN3WTF10fastMallocEm(sizeof(RunLoopDispatchTimer))) RunLoopDispatchTimer;
    timer->refCount.store(1);
    timer->function.callableWrapper = nullptr;
    try {
        _ZN3WTF7RunLoop9TimerBaseC2ERS0_(&timer->timerBase, runLoop);
    } catch (...) {
        _ZN3WTF8fastFreeEPv(timer);
        throw;
    }
    timer->timerBase.vtable = &kDispatchTimerVtable;
    return DispatchTimerRef(timer);
}

void APS5_VABI RunLoopCompleteDestructor(RunLoop* self) {
    RunLoopImpl* loop = std::exchange(self->impl, nullptr);
    {
        std::unique_lock<std::mutex> lock(loop->loopLock);
        loop->shutdown = true;
        loop->readyToRun.notify_one();
        loop->stopCondition.wait(lock, [&] { return loop->mainLoops.empty(); });
    }
    delete loop;
}

void APS5_VABI RunLoopDeletingDestructor(RunLoop* self) {
    RunLoopCompleteDestructor(self);
    _ZN3WTF8fastFreeEPv(self);
}

void APS5_VABI RunLoopDispatch(RunLoop* self, VoidFunction* function) {
    _ZN3WTF7RunLoop8dispatchEONS_8FunctionIFvvEEE(self, function);
}

}

namespace NKWebKit::WtfAbi {

void InitializeMainRunLoop() {
    RunLoop* expected = nullptr;
    if (!s_mainRunLoop.compare_exchange_strong(expected, Current())) {
        throw std::logic_error("WTF::RunLoop::initializeMain called twice");
    }
}

bool IsMainRunLoop() {
    RunLoop* mainRunLoop = s_mainRunLoop.load();
    return mainRunLoop && IsCurrent(mainRunLoop);
}

}  // namespace NKWebKit::WtfAbi

extern "C" {

RunLoop* APS5_VABI _ZN3WTF7RunLoop4mainEv() {
    RunLoop* mainRunLoop = s_mainRunLoop.load();
    if (!mainRunLoop) {
        throw std::logic_error("WTF::RunLoop::main called before initializeMainThread");
    }
    return mainRunLoop;
}

RunLoop* APS5_VABI _ZN3WTF7RunLoop7currentEv() {
    return Current();
}

void APS5_VABI _ZN3WTF7RunLoop8dispatchEONS_8FunctionIFvvEEE(RunLoop* self, VoidFunction* function) {
    RequireRunLoop(self);
    Dispatch(*self->impl, TakeFunction(function));
}

RunLoopDispatchTimerRef* APS5_VABI _ZN3WTF7RunLoop13dispatchAfterENS_7SecondsEONS_8FunctionIFvvEEE(RunLoopDispatchTimerRef* result, RunLoop* self, WTF::Seconds delay, VoidFunction* function) {
    if (!result) {
        throw std::invalid_argument("null Ref<WTF::RunLoop::DispatchTimer> result");
    }
    RequireRunLoop(self);
    OwnedFunction callable = TakeFunction(function);
    DispatchTimerRef timer = CreateDispatchTimer(self);
    RunLoopDispatchTimer* dispatchTimer = timer.get();
    dispatchTimer->refCount.fetch_add(1);
    dispatchTimer->function = NKWebKit::WtfAbi::MakeHostFunction([capturedTimer = DispatchTimerRef(dispatchTimer), function = std::move(callable)]() mutable {
        DispatchTimerRef protectedTimer(std::move(capturedTimer));
        function();
        StopTimer(&protectedTimer.get()->timerBase);
    }).Release();
    StartTimer(&dispatchTimer->timerBase, std::max(delay.m_value, 0.0), false);
    result->ptr = timer.Release();
    return result;
}

void APS5_VABI _ZN3WTF7RunLoop3runEv() {
    RunImpl(Current(), RunMode::Drain);
}

void APS5_VABI _ZN3WTF7RunLoop4stopEv(RunLoop* self) {
    RunLoopImpl& loop = *RequireRunLoop(self)->impl;
    std::lock_guard<std::mutex> lock(loop.loopLock);
    if (loop.mainLoops.empty()) {
        return;
    }
    RunLoopStatus* status = loop.mainLoops.back();
    if (*status != RunLoopStatus::Stopping) {
        *status = RunLoopStatus::Stopping;
        loop.readyToRun.notify_one();
    }
}

RunLoopCycleResult APS5_VABI _ZN3WTF7RunLoop5cycleEj(unsigned) {
    RunImpl(Current(), RunMode::Iterate);
    return RunLoopCycleResult::Continue;
}

void APS5_VABI _ZN3WTF7RunLoop17setWakeUpCallbackEONS_8FunctionIFvvEEE(VoidFunction* function) {
    if (!function) {
        throw std::invalid_argument("null WTF::Function reference");
    }
    OwnedFunction callback(std::exchange(function->callableWrapper, nullptr));
    RunLoopImpl& loop = *Current()->impl;
    std::lock_guard<std::mutex> lock(loop.loopLock);
    std::swap(loop.wakeUpCallback, callback);
}

void APS5_VABI _ZN3WTF7RunLoop38suspendFunctionDispatchForCurrentCycleEv(RunLoop* self) {
    RunLoopImpl& loop = *RequireRunLoop(self)->impl;
    if (loop.isFunctionDispatchSuspended || loop.hasSuspendedFunctions) {
        return;
    }
    loop.isFunctionDispatchSuspended = true;
    WakeUp(loop);
}

void APS5_VABI _ZN3WTF7RunLoop9TimerBaseC2ERS0_(RunLoopTimerBase* self, RunLoop* runLoop) {
    RequireTimer(self);
    RequireRunLoop(runLoop);
    auto* scheduledTask = new RunLoopScheduledTask(self);
    self->vtable = &kTimerBaseVtable;
    RefRunLoop(runLoop);
    self->runLoop = runLoop;
    self->scheduledTask = scheduledTask;
}

void APS5_VABI _ZN3WTF7RunLoop9TimerBaseD1Ev(RunLoopTimerBase* self) {
    _ZN3WTF7RunLoop9TimerBaseD2Ev(self);
}

void APS5_VABI _ZN3WTF7RunLoop9TimerBaseD2Ev(RunLoopTimerBase* self) {
    RequireTimer(self);
    self->vtable = &kTimerBaseVtable;
    StopTimer(self);
    std::exchange(self->scheduledTask, nullptr)->Deref();
    DerefRunLoop(std::exchange(self->runLoop, nullptr));
}

void APS5_VABI _ZN3WTF7RunLoop9TimerBase5startENS_7SecondsEb(RunLoopTimerBase* self, WTF::Seconds interval, bool repeat) {
    StartTimer(RequireTimer(self), interval.m_value, repeat);
}

void APS5_VABI _ZN3WTF7RunLoop9TimerBase4stopEv(RunLoopTimerBase* self) {
    StopTimer(RequireTimer(self));
}

bool APS5_VABI _ZNK3WTF7RunLoop9TimerBase8isActiveEv(const RunLoopTimerBase* self) {
    RunLoopTimerBase* timer = RequireTimer(self);
    std::lock_guard<std::mutex> lock(timer->runLoop->impl->loopLock);
    return timer->scheduledTask->IsActive();
}

WTF::Seconds APS5_VABI _ZNK3WTF7RunLoop9TimerBase16secondsUntilFireEv(const RunLoopTimerBase* self) {
    RunLoopTimerBase* timer = RequireTimer(self);
    std::lock_guard<std::mutex> lock(timer->runLoop->impl->loopLock);
    if (timer->scheduledTask->IsActive()) {
        return WTF::Seconds{std::max(timer->scheduledTask->scheduledTimePoint - Now(), 0.0)};
    }
    return WTF::Seconds{0.0};
}

}
