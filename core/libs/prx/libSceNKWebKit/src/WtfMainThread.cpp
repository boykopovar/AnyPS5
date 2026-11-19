// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/MainThread.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfRunLoopExports.hpp"
#include <atomic>
#include <exception>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {

using NKWebKit::WtfAbi::OwnedFunction;
using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidFunction;

std::once_flag s_initializeOnce;
std::atomic<bool> s_mainThreadInitialized{false};
std::thread::id s_mainThreadId;

VoidCallableWrapper* RequireCallable(const VoidFunction* function) {
    if (!function) {
        throw std::invalid_argument("null WTF::Function reference");
    }
    if (!function->callableWrapper) {
        throw std::invalid_argument("null WTF::Function");
    }
    return function->callableWrapper;
}

void DispatchToMain(VoidFunction* function) {
    RequireCallable(function);
    _ZN3WTF7RunLoop8dispatchEONS_8FunctionIFvvEEE(_ZN3WTF7RunLoop4mainEv(), function);
}

void CallOnMainAndWait(VoidFunction* function, bool isMain) {
    VoidCallableWrapper* callableWrapper = RequireCallable(function);
    if (isMain) {
        callableWrapper->vtable->call(callableWrapper);
        return;
    }
    std::promise<void> completion;
    std::future<void> finished = completion.get_future();
    OwnedFunction task = NKWebKit::WtfAbi::MakeHostFunction(
        [callable = OwnedFunction(std::exchange(function->callableWrapper, nullptr)), completion = std::move(completion)]() mutable {
            std::exception_ptr failure;
            try {
                callable();
            } catch (...) {
                failure = std::current_exception();
            }
            callable.Reset();
            if (failure) {
                completion.set_exception(failure);
            } else {
                completion.set_value();
            }
        });
    DispatchToMain(task.Pass());
    finished.get();
}

}

extern "C" {

void APS5_VABI _ZN3WTF20initializeMainThreadEv() {
    std::call_once(s_initializeOnce, []() {
        s_mainThreadId = std::this_thread::get_id();
        s_mainThreadInitialized.store(true, std::memory_order_release);
        NKWebKit::WtfAbi::InitializeMainRunLoop();
    });
}

bool APS5_VABI _ZN3WTF12isMainThreadEv() {
    if (!s_mainThreadInitialized.load(std::memory_order_acquire)) {
        return false;
    }
    return std::this_thread::get_id() == s_mainThreadId;
}

bool APS5_VABI _ZN3WTF13isMainRunLoopEv() {
    return NKWebKit::WtfAbi::IsMainRunLoop();
}

bool APS5_VABI _ZN3WTF22isMainThreadOrGCThreadEv() {
    return _ZN3WTF12isMainThreadEv();
}

void APS5_VABI _ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE(VoidFunction* function) {
    DispatchToMain(function);
}

void APS5_VABI _ZN3WTF17callOnMainRunLoopEONS_8FunctionIFvvEEE(VoidFunction* function) {
    DispatchToMain(function);
}

void APS5_VABI _ZN3WTF23callOnMainThreadAndWaitEONS_8FunctionIFvvEEE(VoidFunction* function) {
    CallOnMainAndWait(function, _ZN3WTF12isMainThreadEv());
}

void APS5_VABI _ZN3WTF24callOnMainRunLoopAndWaitEONS_8FunctionIFvvEEE(VoidFunction* function) {
    CallOnMainAndWait(function, _ZN3WTF13isMainRunLoopEv());
}

}
