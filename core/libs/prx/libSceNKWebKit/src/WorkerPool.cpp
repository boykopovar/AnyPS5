// Derived from https://github.com/WebKit/WebKit/blob/9d2532291ea8c8182d78314fc3da10baa9d4a639/Source/WTF/wtf/WorkerPool.cpp
/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

// Derived from https://github.com/WebKit/WebKit/blob/9d2532291ea8c8182d78314fc3da10baa9d4a639/Source/WTF/wtf/AutomaticThread.cpp
/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNKWebKit/include/WorkerPoolExports.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace NKWebKit::WtfAbi {

struct WorkerPoolState {
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<VoidCallableWrapper*> tasks;
    std::vector<Pthread> threads;
    unsigned numberOfWorkers = 0;
    unsigned waiting = 0;
};

}  // namespace NKWebKit::WtfAbi

namespace {

using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidFunction;
using NKWebKit::WtfAbi::WorkerPool;
using NKWebKit::WtfAbi::WorkerPoolState;

void* APS5_VABI RunWorker(void* argument) {
    auto* state = static_cast<WorkerPoolState*>(argument);
    for (;;) {
        VoidCallableWrapper* task = nullptr;
        {
            std::unique_lock lock(state->mutex);
            if (state->tasks.empty()) {
                ++state->waiting;
                state->condition.wait(lock, [state] { return !state->tasks.empty(); });
            }
            task = state->tasks.front();
            state->tasks.pop_front();
        }
        if (!task) return nullptr;
        task->vtable->call(task);
        task->vtable->deletingDestructor(task);
    }
}

WorkerPoolState& StateOf(WorkerPool* self) {
    if (!self) throw std::invalid_argument("null WTF::WorkerPool");
    if (!self->state) throw std::invalid_argument("destroyed WTF::WorkerPool");
    return *self->state;
}

void Construct(WorkerPool* self, const char* name, unsigned numberOfWorkers) {
    if (!self) throw std::invalid_argument("null WTF::WorkerPool");
    auto* state = new WorkerPoolState;
    state->numberOfWorkers = numberOfWorkers;
    self->refCount = 1;
    self->state = state;
    self->name = name;
}

void Destroy(WorkerPool* self) {
    WorkerPoolState& state = StateOf(self);
    self->state = nullptr;
    {
        std::lock_guard lock(state.mutex);
        for (unsigned index = 0; index < state.numberOfWorkers; ++index) state.tasks.push_back(nullptr);
    }
    state.condition.notify_all();
    for (Pthread thread : state.threads) pthread_join_nid_postfix(thread, nullptr);
    for (VoidCallableWrapper* task : state.tasks)
        if (task) task->vtable->deletingDestructor(task);
    delete &state;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF10WorkerPoolC1ENS_12ASCIILiteralEjNS_7SecondsE(WorkerPool* self, const char* name, unsigned numberOfWorkers, double) {
    Construct(self, name, numberOfWorkers);
}

void APS5_VABI _ZN3WTF10WorkerPoolC2ENS_12ASCIILiteralEjNS_7SecondsE(WorkerPool* self, const char* name, unsigned numberOfWorkers, double) {
    Construct(self, name, numberOfWorkers);
}

void APS5_VABI _ZN3WTF10WorkerPoolD1Ev(WorkerPool* self) {
    Destroy(self);
}

void APS5_VABI _ZN3WTF10WorkerPoolD2Ev(WorkerPool* self) {
    Destroy(self);
}

void APS5_VABI _ZN3WTF10WorkerPool8postTaskEONS_8FunctionIFvvEEE(WorkerPool* self, VoidFunction* task) {
    WorkerPoolState& state = StateOf(self);
    if (!task) throw std::invalid_argument("null WTF::Function reference for WTF::WorkerPool::postTask");
    if (!task->callableWrapper) throw std::invalid_argument("null WTF::Function for WTF::WorkerPool::postTask");
    {
        std::lock_guard lock(state.mutex);
        if (!state.waiting && state.threads.size() < state.numberOfWorkers) {
            Pthread thread = nullptr;
            if (const int error = pthread_create_nid_postfix(&thread, nullptr, &RunWorker, &state))
                throw std::runtime_error("WTF::WorkerPool: pthread_create failed with " + std::to_string(error));
            state.threads.push_back(thread);
        }
        state.tasks.push_back(task->callableWrapper);
        task->callableWrapper = nullptr;
        if (state.waiting) --state.waiting;
    }
    state.condition.notify_one();
}

}
