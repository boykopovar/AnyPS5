// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/CrossThreadTaskHandler.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/CrossThreadQueue.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNKWebKit/include/CrossThreadTaskHandlerExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

namespace NKWebKit::WtfAbi {

struct CrossThreadTaskHandlerState {
    std::atomic<unsigned> references{2};
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<VoidCallableWrapper*> tasks;
    std::deque<VoidCallableWrapper*> replies;
    CrossThreadTaskHandler* owner = nullptr;
    bool killed = false;
    bool replyScheduled = false;
};

}  // namespace NKWebKit::WtfAbi

namespace {

using NKWebKit::WtfAbi::CrossThreadTaskHandler;
using NKWebKit::WtfAbi::CrossThreadTaskHandlerState;
using NKWebKit::WtfAbi::CrossThreadTaskHandlerVtableStorage;
using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidCallableWrapperVtable;
using NKWebKit::WtfAbi::VoidFunction;

void Release(CrossThreadTaskHandlerState* state) {
    if (!--state->references) delete state;
}

void Delete(VoidCallableWrapper* callable) {
    if (callable) callable->vtable->deletingDestructor(callable);
}

void* APS5_VABI RunTasks(void* argument) {
    auto* state = static_cast<CrossThreadTaskHandlerState*>(argument);
    for (;;) {
        VoidCallableWrapper* task = nullptr;
        {
            std::unique_lock lock(state->mutex);
            state->condition.wait(lock, [state] { return state->killed || !state->tasks.empty(); });
            if (state->killed) break;
            task = state->tasks.front();
            state->tasks.pop_front();
        }
        if (!task) break;
        task->vtable->call(task);
        Delete(task);
    }
    VoidCallableWrapper* completionCallback = nullptr;
    {
        std::lock_guard lock(state->mutex);
        if (state->owner) completionCallback = state->owner->completionCallback;
    }
    if (completionCallback) completionCallback->vtable->call(completionCallback);
    Release(state);
    return nullptr;
}

struct ReplyJob {
    VoidCallableWrapper base;
    CrossThreadTaskHandlerState* state;
};

void APS5_VABI DestroyReplyJob(VoidCallableWrapper* self) {
    Release(reinterpret_cast<ReplyJob*>(self)->state);
}

void APS5_VABI DeleteReplyJob(VoidCallableWrapper* self) {
    auto* job = reinterpret_cast<ReplyJob*>(self);
    Release(job->state);
    delete job;
}

void APS5_VABI HandleReplies(VoidCallableWrapper* self) {
    CrossThreadTaskHandlerState* state = reinterpret_cast<ReplyJob*>(self)->state;
    {
        std::lock_guard lock(state->mutex);
        state->replyScheduled = false;
    }
    for (;;) {
        VoidCallableWrapper* reply = nullptr;
        {
            std::lock_guard lock(state->mutex);
            if (state->replies.empty()) return;
            reply = state->replies.front();
            state->replies.pop_front();
        }
        reply->vtable->call(reply);
        Delete(reply);
    }
}

const VoidCallableWrapperVtable replyJobVtable = {DestroyReplyJob, DeleteReplyJob, HandleReplies};

CrossThreadTaskHandlerState& StateOf(CrossThreadTaskHandler* self) {
    if (!self) throw std::invalid_argument("null WTF::CrossThreadTaskHandler");
    if (!self->state) throw std::invalid_argument("destroyed WTF::CrossThreadTaskHandler");
    return *self->state;
}

VoidFunction& Checked(VoidFunction* function) {
    if (!function) throw std::invalid_argument("null WTF::Function reference for WTF::CrossThreadTaskHandler");
    return *function;
}

const CrossThreadTaskHandlerVtableStorage vtable = {
    0,
    nullptr,
    {
        &_ZN3WTF22CrossThreadTaskHandlerD1Ev,
        &_ZN3WTF22CrossThreadTaskHandlerD0Ev,
    },
};

void Construct(CrossThreadTaskHandler* self) {
    if (!self) throw std::invalid_argument("null WTF::CrossThreadTaskHandler");
    auto* state = new CrossThreadTaskHandlerState;
    state->owner = self;
    self->completionCallback = nullptr;
    Pthread thread = nullptr;
    if (const int error = pthread_create_nid_postfix(&thread, nullptr, &RunTasks, state)) {
        delete state;
        throw std::runtime_error("WTF::CrossThreadTaskHandler: pthread_create failed with " + std::to_string(error));
    }
    pthread_detach_nid_postfix(thread);
    self->vtable = &vtable.dispatch;
    self->state = state;
}

void Destroy(CrossThreadTaskHandler* self) {
    CrossThreadTaskHandlerState& state = StateOf(self);
    self->state = nullptr;
    std::deque<VoidCallableWrapper*> tasks;
    std::deque<VoidCallableWrapper*> replies;
    {
        std::lock_guard lock(state.mutex);
        state.killed = true;
        tasks.swap(state.tasks);
        replies.swap(state.replies);
        state.owner = nullptr;
    }
    state.condition.notify_all();
    Delete(std::exchange(self->completionCallback, nullptr));
    for (VoidCallableWrapper* reply : replies) Delete(reply);
    for (VoidCallableWrapper* task : tasks) Delete(task);
    Release(&state);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerC1EPKcNS0_25AutodrainedPoolForRunLoopE(CrossThreadTaskHandler* self, const char*, int) {
    Construct(self);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerC2EPKcNS0_25AutodrainedPoolForRunLoopE(CrossThreadTaskHandler* self, const char*, int) {
    Construct(self);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerD1Ev(CrossThreadTaskHandler* self) {
    Destroy(self);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerD2Ev(CrossThreadTaskHandler* self) {
    Destroy(self);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerD0Ev(CrossThreadTaskHandler* self) {
    Destroy(self);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE(CrossThreadTaskHandler* self, VoidFunction* task) {
    CrossThreadTaskHandlerState& state = StateOf(self);
    VoidCallableWrapper* callable = std::exchange(Checked(task).callableWrapper, nullptr);
    {
        std::lock_guard lock(state.mutex);
        state.tasks.push_back(callable);
    }
    state.condition.notify_one();
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE(CrossThreadTaskHandler* self, VoidFunction* task) {
    CrossThreadTaskHandlerState& state = StateOf(self);
    if (!Checked(task).callableWrapper) throw std::invalid_argument("null WTF::Function for WTF::CrossThreadTaskHandler::postTaskReply");
    VoidCallableWrapper* callable = std::exchange(task->callableWrapper, nullptr);
    {
        std::lock_guard lock(state.mutex);
        state.replies.push_back(callable);
        if (state.replyScheduled) return;
        state.replyScheduled = true;
    }
    ++state.references;
    auto* job = new ReplyJob{{&replyJobVtable}, &state};
    VoidFunction function{&job->base};
    _ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE(&function);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandler4killEv(CrossThreadTaskHandler* self) {
    CrossThreadTaskHandlerState& state = StateOf(self);
    {
        std::lock_guard lock(state.mutex);
        state.killed = true;
    }
    state.condition.notify_all();
}

}
