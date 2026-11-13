#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <semaphore>
#include <stdexcept>
#include <thread>

namespace {

using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidFunction;

std::once_flag s_initializeOnce;
std::atomic<bool> s_mainThreadInitialized{false};
std::thread::id s_mainThreadId;

struct MainTask {
    VoidCallableWrapper* callableWrapper;
    std::binary_semaphore* semaphore;
};

VoidCallableWrapper* RequireCallable(const VoidFunction* function) {
    if (!function) {
        throw std::invalid_argument("null WTF::Function reference");
    }
    if (!function->callableWrapper) {
        throw std::invalid_argument("null WTF::Function");
    }
    return function->callableWrapper;
}

VoidCallableWrapper* TakeCallable(VoidFunction* function) {
    VoidCallableWrapper* callableWrapper = RequireCallable(function);
    function->callableWrapper = nullptr;
    return callableWrapper;
}

void APS5_VABI RunMainTask(void* context) {
    std::unique_ptr<MainTask> task(static_cast<MainTask*>(context));
    task->callableWrapper->vtable->call(task->callableWrapper);
    if (task->semaphore) {
        task->semaphore->release();
    }
    task->callableWrapper->vtable->deletingDestructor(task->callableWrapper);
}

void DispatchToMain(VoidCallableWrapper* callableWrapper, std::binary_semaphore* semaphore) {
    WKRunLoopCallOnMainThread(RunMainTask, new MainTask{callableWrapper, semaphore});
}

void CallOnMainAndWait(VoidFunction* function, bool isMain) {
    if (isMain) {
        VoidCallableWrapper* callableWrapper = RequireCallable(function);
        callableWrapper->vtable->call(callableWrapper);
        return;
    }
    std::binary_semaphore semaphore{0};
    DispatchToMain(TakeCallable(function), &semaphore);
    semaphore.acquire();
}

}

extern "C" {

void APS5_VABI _ZN3WTF20initializeMainThreadEv() {
    std::call_once(s_initializeOnce, []() {
        s_mainThreadId = std::this_thread::get_id();
        s_mainThreadInitialized.store(true, std::memory_order_release);
    });
}

bool APS5_VABI _ZN3WTF12isMainThreadEv() {
    if (!s_mainThreadInitialized.load(std::memory_order_acquire)) {
        return false;
    }
    return std::this_thread::get_id() == s_mainThreadId;
}

bool APS5_VABI _ZN3WTF13isMainRunLoopEv() {
    return _ZN3WTF12isMainThreadEv();
}

bool APS5_VABI _ZN3WTF22isMainThreadOrGCThreadEv() {
    return _ZN3WTF12isMainThreadEv();
}

void APS5_VABI _ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE(VoidFunction* function) {
    DispatchToMain(TakeCallable(function), nullptr);
}

void APS5_VABI _ZN3WTF17callOnMainRunLoopEONS_8FunctionIFvvEEE(VoidFunction* function) {
    DispatchToMain(TakeCallable(function), nullptr);
}

void APS5_VABI _ZN3WTF23callOnMainThreadAndWaitEONS_8FunctionIFvvEEE(VoidFunction* function) {
    CallOnMainAndWait(function, _ZN3WTF12isMainThreadEv());
}

void APS5_VABI _ZN3WTF24callOnMainRunLoopAndWaitEONS_8FunctionIFvvEEE(VoidFunction* function) {
    CallOnMainAndWait(function, _ZN3WTF13isMainRunLoopEv());
}

}
