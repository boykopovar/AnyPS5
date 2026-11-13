#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidCallableWrapperVtable;
using NKWebKit::WtfAbi::VoidFunction;

void Require(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", what);
        std::abort();
    }
}

struct CountingWrapper {
    VoidCallableWrapper base;
    std::atomic<int> calls{0};
    std::atomic<int> completeDestructions{0};
    std::atomic<int> deletingDestructions{0};
    std::atomic<bool> calledOnMainThread{false};
    std::thread::id callingThread;
    bool stopLoop = false;
};

std::atomic<int> s_liveWrappers{0};

CountingWrapper* Self(VoidCallableWrapper* self) {
    return reinterpret_cast<CountingWrapper*>(self);
}

void APS5_VABI CompleteDestructor(VoidCallableWrapper* self) {
    Self(self)->completeDestructions.fetch_add(1);
}

void APS5_VABI DeletingDestructor(VoidCallableWrapper* self) {
    Self(self)->deletingDestructions.fetch_add(1);
    s_liveWrappers.fetch_sub(1);
}

void APS5_VABI Call(VoidCallableWrapper* self) {
    CountingWrapper* wrapper = Self(self);
    wrapper->callingThread = std::this_thread::get_id();
    wrapper->calledOnMainThread.store(_ZN3WTF12isMainThreadEv());
    wrapper->calls.fetch_add(1);
    if (wrapper->stopLoop) {
        WKRunLoopStopMain();
    }
}

const VoidCallableWrapperVtable s_vtable{CompleteDestructor, DeletingDestructor, Call};

void Arm(CountingWrapper& wrapper, bool stopLoop) {
    wrapper.base.vtable = &s_vtable;
    wrapper.stopLoop = stopLoop;
    s_liveWrappers.fetch_add(1);
}

void TestDispatchToMainLoop(void (APS5_VABI* dispatch)(VoidFunction*), const char* name) {
    WKRunLoopInitializeMain();
    CountingWrapper wrapper;
    Arm(wrapper, true);
    VoidFunction function{&wrapper.base};
    dispatch(&function);
    Require(function.callableWrapper == nullptr, name);
    Require(wrapper.calls.load() == 0, name);
    WKRunLoopRunMain();
    Require(wrapper.calls.load() == 1, name);
    Require(wrapper.calledOnMainThread.load(), name);
    Require(wrapper.callingThread == std::this_thread::get_id(), name);
    Require(wrapper.deletingDestructions.load() == 1, name);
    Require(wrapper.completeDestructions.load() == 0, name);
}

void TestDispatchFromWorker(void (APS5_VABI* dispatch)(VoidFunction*), const char* name) {
    WKRunLoopInitializeMain();
    CountingWrapper wrapper;
    Arm(wrapper, true);
    std::thread worker([&] {
        VoidFunction function{&wrapper.base};
        dispatch(&function);
        Require(function.callableWrapper == nullptr, name);
    });
    WKRunLoopRunMain();
    worker.join();
    Require(wrapper.calls.load() == 1, name);
    Require(wrapper.callingThread == std::this_thread::get_id(), name);
    Require(wrapper.deletingDestructions.load() == 1, name);
}

void TestAndWaitInlineOnMain(void (APS5_VABI* callAndWait)(VoidFunction*), const char* name) {
    WKRunLoopInitializeMain();
    CountingWrapper wrapper;
    Arm(wrapper, false);
    VoidFunction function{&wrapper.base};
    callAndWait(&function);
    Require(wrapper.calls.load() == 1, name);
    Require(wrapper.callingThread == std::this_thread::get_id(), name);
    Require(function.callableWrapper == &wrapper.base, name);
    Require(wrapper.deletingDestructions.load() == 0, name);
    Require(wrapper.completeDestructions.load() == 0, name);
    s_liveWrappers.fetch_sub(1);
}

void TestAndWaitBlocksWorker(void (APS5_VABI* callAndWait)(VoidFunction*), const char* name) {
    WKRunLoopInitializeMain();
    CountingWrapper wrapper;
    Arm(wrapper, true);
    std::atomic<bool> workerReturned{false};
    std::atomic<int> callsSeenOnReturn{-1};
    std::atomic<bool> workerSawNullFunction{false};
    std::thread::id workerId;
    std::thread worker([&] {
        VoidFunction function{&wrapper.base};
        callAndWait(&function);
        callsSeenOnReturn.store(wrapper.calls.load());
        workerSawNullFunction.store(function.callableWrapper == nullptr);
        workerReturned.store(true);
    });
    workerId = worker.get_id();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!workerReturned.load(), name);
    Require(wrapper.calls.load() == 0, name);
    WKRunLoopRunMain();
    worker.join();
    Require(workerReturned.load(), name);
    Require(callsSeenOnReturn.load() == 1, name);
    Require(workerSawNullFunction.load(), name);
    Require(wrapper.calls.load() == 1, name);
    Require(wrapper.callingThread == std::this_thread::get_id(), name);
    Require(wrapper.callingThread != workerId, name);
    Require(wrapper.deletingDestructions.load() == 1, name);
    Require(wrapper.completeDestructions.load() == 0, name);
}

void TestNullFunctionThrows(void (APS5_VABI* entry)(VoidFunction*), const char* name) {
    bool threwForNullReference = false;
    try {
        entry(nullptr);
    } catch (const std::invalid_argument&) {
        threwForNullReference = true;
    }
    Require(threwForNullReference, name);
    bool threwForEmptyFunction = false;
    VoidFunction empty{nullptr};
    try {
        entry(&empty);
    } catch (const std::invalid_argument&) {
        threwForEmptyFunction = true;
    }
    Require(threwForEmptyFunction, name);
}

}

int main() {
    _ZN3WTF20initializeMainThreadEv();

    TestDispatchToMainLoop(_ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE, "callOnMainThread");
    TestDispatchToMainLoop(_ZN3WTF17callOnMainRunLoopEONS_8FunctionIFvvEEE, "callOnMainRunLoop");
    TestDispatchFromWorker(_ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE, "callOnMainThread from worker");
    TestDispatchFromWorker(_ZN3WTF17callOnMainRunLoopEONS_8FunctionIFvvEEE, "callOnMainRunLoop from worker");
    TestAndWaitInlineOnMain(_ZN3WTF23callOnMainThreadAndWaitEONS_8FunctionIFvvEEE, "callOnMainThreadAndWait inline");
    TestAndWaitInlineOnMain(_ZN3WTF24callOnMainRunLoopAndWaitEONS_8FunctionIFvvEEE, "callOnMainRunLoopAndWait inline");
    TestAndWaitBlocksWorker(_ZN3WTF23callOnMainThreadAndWaitEONS_8FunctionIFvvEEE, "callOnMainThreadAndWait worker");
    TestAndWaitBlocksWorker(_ZN3WTF24callOnMainRunLoopAndWaitEONS_8FunctionIFvvEEE, "callOnMainRunLoopAndWait worker");
    TestNullFunctionThrows(_ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE, "callOnMainThread null");
    TestNullFunctionThrows(_ZN3WTF17callOnMainRunLoopEONS_8FunctionIFvvEEE, "callOnMainRunLoop null");
    TestNullFunctionThrows(_ZN3WTF23callOnMainThreadAndWaitEONS_8FunctionIFvvEEE, "callOnMainThreadAndWait null");
    TestNullFunctionThrows(_ZN3WTF24callOnMainRunLoopAndWaitEONS_8FunctionIFvvEEE, "callOnMainRunLoopAndWait null");

    Require(s_liveWrappers.load() == 0, "every taken wrapper was deleted");

    std::printf("All libSceNKWebKit WtfCallOnMain tests passed!\n");
    return 0;
}
