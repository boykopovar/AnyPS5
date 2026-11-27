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
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNKWebKit/include/CrossThreadTaskHandlerExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
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
    bool destroyed = false;
    bool replyScheduled = false;
};

}  // namespace NKWebKit::WtfAbi

namespace {

using NKWebKit::WtfAbi::CrossThreadTaskHandler;
using NKWebKit::WtfAbi::CrossThreadTaskHandlerState;
using NKWebKit::WtfAbi::CrossThreadTaskHandlerVtable;
using NKWebKit::WtfAbi::CrossThreadTaskHandlerVtableStorage;
using NKWebKit::WtfAbi::VoidCallableWrapper;
using NKWebKit::WtfAbi::VoidCallableWrapperVtable;
using NKWebKit::WtfAbi::VoidFunction;

struct CrossThreadTaskHandlerHeader {
    const CrossThreadTaskHandlerVtable* vtable;
    CrossThreadTaskHandlerState* state;
};

static_assert(std::is_trivially_copyable_v<CrossThreadTaskHandlerHeader>);
static_assert(sizeof(CrossThreadTaskHandlerHeader) == 16);
static_assert(offsetof(CrossThreadTaskHandlerHeader, vtable) == offsetof(CrossThreadTaskHandler, vtable));
static_assert(offsetof(CrossThreadTaskHandlerHeader, state) == offsetof(CrossThreadTaskHandler, state));
static_assert(std::is_trivially_copyable_v<VoidCallableWrapper>);
static_assert(std::is_trivially_copyable_v<VoidCallableWrapperVtable>);
static_assert(std::is_trivially_copyable_v<VoidFunction>);

struct LiveStateEntry {
    CrossThreadTaskHandler* owner{nullptr};
    bool retiring{false};
};

std::mutex& LiveStatesLock() {
    static std::mutex* const lock = new std::mutex;
    return *lock;
}

std::unordered_map<CrossThreadTaskHandlerState*, LiveStateEntry>& LiveStates() {
    static std::unordered_map<CrossThreadTaskHandlerState*, LiveStateEntry>* const states =
        new std::unordered_map<CrossThreadTaskHandlerState*, LiveStateEntry>;
    return *states;
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void ValidateCallableWrapper(VoidCallableWrapper* callable, bool requireCall, const char* function) {
    if (!callable) return;
    const VoidCallableWrapperVtable* vtablePtr = nullptr;
    if (!GuestCopyIn_nid_no_patch(&vtablePtr, callable, sizeof(vtablePtr)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!vtablePtr) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    VoidCallableWrapperVtable vtableCopy{};
    if (!GuestCopyIn_nid_no_patch(&vtableCopy, vtablePtr, sizeof(vtableCopy)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!vtableCopy.deletingDestructor || (requireCall && !vtableCopy.call))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void Release(CrossThreadTaskHandlerState* state) {
    if (!--state->references) delete state;
}

struct StateRef {
    CrossThreadTaskHandlerState* ptr{nullptr};

    explicit StateRef(CrossThreadTaskHandlerState* state) : ptr(state) {}
    ~StateRef() {
        if (ptr) Release(ptr);
    }

    StateRef(const StateRef&) = delete;
    StateRef& operator=(const StateRef&) = delete;
};

StateRef AcquireLiveState(CrossThreadTaskHandler* self, const char* function) {
    if (!self) throw std::invalid_argument("null WTF::CrossThreadTaskHandler");
    CrossThreadTaskHandlerState* state = nullptr;
    if (!GuestCopyIn_nid_no_patch(&state, reinterpret_cast<const unsigned char*>(self) + offsetof(CrossThreadTaskHandler, state), sizeof(state)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!state) throw std::invalid_argument("destroyed WTF::CrossThreadTaskHandler");
    std::lock_guard lock(LiveStatesLock());
    const auto it = LiveStates().find(state);
    if (it == LiveStates().end() || it->second.owner != self)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (it->second.retiring)
        throw std::invalid_argument("destroyed WTF::CrossThreadTaskHandler");
    ++state->references;
    return StateRef(state);
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
        if (state->owner) {
            if (!GuestCopyIn_nid_no_patch(
                    &completionCallback,
                    reinterpret_cast<const unsigned char*>(state->owner) + offsetof(CrossThreadTaskHandler, completionCallback),
                    sizeof(completionCallback))) {
                completionCallback = nullptr;
            }
        }
    }
    if (completionCallback) {
        const VoidCallableWrapperVtable* callbackVtablePtr = nullptr;
        VoidCallableWrapperVtable callbackVtable{};
        if (GuestCopyIn_nid_no_patch(&callbackVtablePtr, completionCallback, sizeof(callbackVtablePtr))
            && callbackVtablePtr
            && GuestCopyIn_nid_no_patch(&callbackVtable, callbackVtablePtr, sizeof(callbackVtable))
            && callbackVtable.call) {
            callbackVtable.call(completionCallback);
        }
    }
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

const CrossThreadTaskHandlerVtableStorage vtable = {
    0,
    nullptr,
    {
        &_ZN3WTF22CrossThreadTaskHandlerD1Ev,
        &_ZN3WTF22CrossThreadTaskHandlerD0Ev,
    },
};

void Construct(CrossThreadTaskHandler* self, const char* function) {
    if (!self) throw std::invalid_argument("null WTF::CrossThreadTaskHandler");
    const GuestArena::HostWrite writeGuard(self, sizeof(CrossThreadTaskHandler));
    if (!writeGuard.Open() || !GuestWritable_nid_no_patch(self, sizeof(CrossThreadTaskHandler)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    VoidCallableWrapper* nullCallback = nullptr;
    if (!GuestCopyOut_nid_no_patch(
            reinterpret_cast<unsigned char*>(self) + offsetof(CrossThreadTaskHandler, completionCallback),
            &nullCallback,
            sizeof(nullCallback))) {
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    auto* state = new CrossThreadTaskHandlerState;
    state->owner = self;
    try {
        std::lock_guard lock(LiveStatesLock());
        LiveStates().emplace(state, LiveStateEntry{self, false});
    } catch (...) {
        delete state;
        throw;
    }
    Pthread thread = nullptr;
    if (const int error = pthread_create_nid_postfix(&thread, nullptr, &RunTasks, state)) {
        {
            std::lock_guard lock(LiveStatesLock());
            LiveStates().erase(state);
        }
        delete state;
        throw std::runtime_error("WTF::CrossThreadTaskHandler: pthread_create failed with " + std::to_string(error));
    }
    pthread_detach_nid_postfix(thread);
    const CrossThreadTaskHandlerHeader header{&vtable.dispatch, state};
    if (!GuestCopyOut_nid_no_patch(self, &header, sizeof(header))) {
        {
            std::lock_guard liveLock(LiveStatesLock());
            LiveStates().erase(state);
        }
        {
            std::lock_guard stateLock(state->mutex);
            state->killed = true;
            state->destroyed = true;
            state->owner = nullptr;
        }
        state->condition.notify_all();
        Release(state);
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
}

void Destroy(CrossThreadTaskHandler* self, const char* function) {
    if (!self) throw std::invalid_argument("null WTF::CrossThreadTaskHandler");
    RequireReadableGuestBytes(self, sizeof(CrossThreadTaskHandler), function);
    const GuestArena::HostWrite writeGuard(self, sizeof(CrossThreadTaskHandler));
    if (!writeGuard.Open() || !GuestWritable_nid_no_patch(self, sizeof(CrossThreadTaskHandler)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    CrossThreadTaskHandlerState* state = nullptr;
    if (!GuestCopyIn_nid_no_patch(&state, reinterpret_cast<const unsigned char*>(self) + offsetof(CrossThreadTaskHandler, state), sizeof(state)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!state) throw std::invalid_argument("destroyed WTF::CrossThreadTaskHandler");
    VoidCallableWrapper* completionCallback = nullptr;
    if (!GuestCopyIn_nid_no_patch(
            &completionCallback,
            reinterpret_cast<const unsigned char*>(self) + offsetof(CrossThreadTaskHandler, completionCallback),
            sizeof(completionCallback))) {
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    ValidateCallableWrapper(completionCallback, false, function);

    CrossThreadTaskHandlerState* pinnedState = nullptr;
    {
        std::lock_guard liveLock(LiveStatesLock());
        const auto it = LiveStates().find(state);
        if (it == LiveStates().end() || it->second.owner != self)
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (it->second.retiring)
            throw std::invalid_argument("destroyed WTF::CrossThreadTaskHandler");
        it->second.retiring = true;
        ++state->references;
        pinnedState = state;
    }
    StateRef pinned(pinnedState);

    CrossThreadTaskHandlerState* nullState = nullptr;
    if (!GuestCopyOut_nid_no_patch(
            reinterpret_cast<unsigned char*>(self) + offsetof(CrossThreadTaskHandler, state),
            &nullState,
            sizeof(nullState))) {
        std::lock_guard liveLock(LiveStatesLock());
        const auto it = LiveStates().find(state);
        if (it != LiveStates().end()) it->second.retiring = false;
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    VoidCallableWrapper* nullCallback = nullptr;
    if (!GuestCopyOut_nid_no_patch(
            reinterpret_cast<unsigned char*>(self) + offsetof(CrossThreadTaskHandler, completionCallback),
            &nullCallback,
            sizeof(nullCallback))) {
        static_cast<void>(GuestCopyOut_nid_no_patch(
            reinterpret_cast<unsigned char*>(self) + offsetof(CrossThreadTaskHandler, state),
            &state,
            sizeof(state)));
        std::lock_guard liveLock(LiveStatesLock());
        const auto it = LiveStates().find(state);
        if (it != LiveStates().end()) it->second.retiring = false;
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    {
        std::lock_guard liveLock(LiveStatesLock());
        LiveStates().erase(state);
    }
    std::deque<VoidCallableWrapper*> tasks;
    std::deque<VoidCallableWrapper*> replies;
    {
        std::lock_guard lock(state->mutex);
        state->killed = true;
        state->destroyed = true;
        tasks.swap(state->tasks);
        replies.swap(state->replies);
        state->owner = nullptr;
    }
    state->condition.notify_all();
    Delete(completionCallback);
    for (VoidCallableWrapper* reply : replies) Delete(reply);
    for (VoidCallableWrapper* task : tasks) Delete(task);
    Release(state);
}

void PostTaskImpl(CrossThreadTaskHandler* self, VoidFunction* task, const char* function) {
    StateRef state = AcquireLiveState(self, function);
    if (!task) throw std::invalid_argument("null WTF::Function reference for WTF::CrossThreadTaskHandler");
    RequireReadableGuestBytes(task, sizeof(VoidFunction), function);
    const GuestArena::HostWrite taskWriteGuard(task, sizeof(VoidFunction));
    if (!taskWriteGuard.Open() || !GuestWritable_nid_no_patch(task, sizeof(VoidFunction)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    VoidCallableWrapper* callable = nullptr;
    if (!GuestCopyIn_nid_no_patch(&callable, task, sizeof(callable)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    ValidateCallableWrapper(callable, true, function);
    {
        std::lock_guard lock(state.ptr->mutex);
        if (state.ptr->destroyed)
            throw std::invalid_argument("destroyed WTF::CrossThreadTaskHandler");
        state.ptr->tasks.push_back(callable);
        VoidCallableWrapper* cleared = nullptr;
        if (!GuestCopyOut_nid_no_patch(task, &cleared, sizeof(cleared))) {
            state.ptr->tasks.pop_back();
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
    }
    state.ptr->condition.notify_one();
}

void PostTaskReplyImpl(CrossThreadTaskHandler* self, VoidFunction* task, const char* function) {
    StateRef state = AcquireLiveState(self, function);
    if (!task) throw std::invalid_argument("null WTF::Function reference for WTF::CrossThreadTaskHandler");
    RequireReadableGuestBytes(task, sizeof(VoidFunction), function);
    const GuestArena::HostWrite taskWriteGuard(task, sizeof(VoidFunction));
    if (!taskWriteGuard.Open() || !GuestWritable_nid_no_patch(task, sizeof(VoidFunction)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    VoidCallableWrapper* callable = nullptr;
    if (!GuestCopyIn_nid_no_patch(&callable, task, sizeof(callable)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!callable) throw std::invalid_argument("null WTF::Function for WTF::CrossThreadTaskHandler::postTaskReply");
    ValidateCallableWrapper(callable, true, function);
    std::unique_ptr<ReplyJob> job;
    {
        std::lock_guard lock(state.ptr->mutex);
        if (state.ptr->destroyed)
            throw std::invalid_argument("destroyed WTF::CrossThreadTaskHandler");
        const bool needSchedule = !state.ptr->replyScheduled;
        if (needSchedule) job = std::make_unique<ReplyJob>(ReplyJob{{&replyJobVtable}, state.ptr});
        state.ptr->replies.push_back(callable);
        VoidCallableWrapper* cleared = nullptr;
        if (!GuestCopyOut_nid_no_patch(task, &cleared, sizeof(cleared))) {
            state.ptr->replies.pop_back();
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        if (!needSchedule) return;
        state.ptr->replyScheduled = true;
        ++state.ptr->references;
    }
    ReplyJob* jobPtr = job.release();
    VoidFunction functionObj{&jobPtr->base};
    try {
        _ZN3WTF16callOnMainThreadEONS_8FunctionIFvvEEE(&functionObj);
    } catch (...) {
        if (functionObj.callableWrapper) {
            delete jobPtr;
            Release(state.ptr);
        }
        std::lock_guard lock(state.ptr->mutex);
        state.ptr->replyScheduled = false;
        if (!state.ptr->replies.empty() && state.ptr->replies.back() == callable) {
            state.ptr->replies.pop_back();
            static_cast<void>(GuestCopyOut_nid_no_patch(task, &callable, sizeof(callable)));
        }
        throw;
    }
}

void KillImpl(CrossThreadTaskHandler* self, const char* function) {
    StateRef state = AcquireLiveState(self, function);
    {
        std::lock_guard lock(state.ptr->mutex);
        state.ptr->killed = true;
    }
    state.ptr->condition.notify_all();
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerC1EPKcNS0_25AutodrainedPoolForRunLoopE(CrossThreadTaskHandler* self, const char*, int) {
    Construct(self, __func__);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerC2EPKcNS0_25AutodrainedPoolForRunLoopE(CrossThreadTaskHandler* self, const char*, int) {
    Construct(self, __func__);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerD1Ev(CrossThreadTaskHandler* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerD2Ev(CrossThreadTaskHandler* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerD0Ev(CrossThreadTaskHandler* self) {
    Destroy(self, __func__);
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE(CrossThreadTaskHandler* self, VoidFunction* task) {
    PostTaskImpl(self, task, __func__);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE(CrossThreadTaskHandler* self, VoidFunction* task) {
    PostTaskReplyImpl(self, task, __func__);
}

void APS5_VABI _ZN3WTF22CrossThreadTaskHandler4killEv(CrossThreadTaskHandler* self) {
    KillImpl(self, __func__);
}

}
