// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/AsyncStackTrace.cpp
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/AsyncStackTrace.h
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

#include "prx/libSceNKWebKit/include/InspectorCallStackExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::InspectorAbi::AsyncStackTrace;
using NKWebKit::InspectorAbi::AsyncStackTraceState;
using NKWebKit::InspectorAbi::ScriptCallStack;

AsyncStackTrace& Checked(AsyncStackTrace* trace) {
    if (!trace) throw std::invalid_argument("null Inspector::AsyncStackTrace");
    return *trace;
}

const AsyncStackTrace& Checked(const AsyncStackTrace* trace) {
    if (!trace) throw std::invalid_argument("null Inspector::AsyncStackTrace");
    return *trace;
}

ScriptCallStack* Stack(AsyncStackTrace& trace) {
    if (!trace.callStack) throw std::invalid_argument("null Inspector::ScriptCallStack");
    return trace.callStack;
}

std::size_t Size(AsyncStackTrace& trace) {
    return Stack(trace)->frames.size;
}

bool IsLocked(const AsyncStackTrace& trace) {
    return trace.state == AsyncStackTraceState::Pending
        || trace.state == AsyncStackTraceState::Active
        || trace.childCount > 1;
}

AsyncStackTrace* Create(ScriptCallStack* callStack, bool singleShot, AsyncStackTrace* parent) {
    auto* trace = static_cast<AsyncStackTrace*>(_ZN3WTF10fastMallocEm(sizeof(AsyncStackTrace)));
    trace->refCount = 1;
    trace->callStack = callStack;
    trace->parent = parent;
    trace->childCount = 0;
    trace->state = AsyncStackTraceState::Pending;
    trace->truncated = false;
    trace->singleShot = singleShot;
    if (parent) ++parent->childCount;
    return trace;
}

void RequireRemovable(const AsyncStackTrace& trace) {
    AsyncStackTrace* parent = trace.parent;
    if (!parent) return;
    if (!trace.callStack) throw std::invalid_argument("null Inspector::ScriptCallStack");
    if (!parent->childCount) throw std::logic_error("Inspector::AsyncStackTrace: parent has no children");
}

void Remove(AsyncStackTrace& trace) {
    RequireRemovable(trace);
    AsyncStackTrace* parent = trace.parent;
    if (!parent) return;
    ScriptCallStack* callStack = trace.callStack;
    --parent->childCount;
    trace.parent = nullptr;
    NKWebKit::InspectorAbi::Release(parent);
    NKWebKit::InspectorAbi::Release(std::exchange(callStack->parentStackTrace, nullptr));
}

void Destroy(AsyncStackTrace& trace) {
    if (trace.parent) Remove(trace);
    Stack(trace);
    NKWebKit::InspectorAbi::Release(std::exchange(trace.callStack, nullptr));
}

void Truncate(AsyncStackTrace& trace, std::size_t maxDepth) {
    AsyncStackTrace* lastUnlockedAncestor = nullptr;
    std::size_t depth = 0;

    AsyncStackTrace* newStackTraceRoot = &trace;
    while (newStackTraceRoot) {
        depth += Size(*newStackTraceRoot);
        if (depth >= maxDepth) break;

        AsyncStackTrace* parent = newStackTraceRoot->parent;
        if (!lastUnlockedAncestor && parent && IsLocked(*parent)) lastUnlockedAncestor = newStackTraceRoot;

        newStackTraceRoot = parent;
    }

    if (!newStackTraceRoot || !newStackTraceRoot->parent) return;

    if (!lastUnlockedAncestor) {
        newStackTraceRoot->truncated = true;
        Remove(*newStackTraceRoot);
        return;
    }

    AsyncStackTrace* previousNode = lastUnlockedAncestor;

    AsyncStackTrace* sourceNode = lastUnlockedAncestor->parent;
    RequireRemovable(*lastUnlockedAncestor);
    NKWebKit::InspectorAbi::Retain(sourceNode);
    Remove(*lastUnlockedAncestor);

    while (sourceNode) {
        ScriptCallStack* callStack = Stack(*sourceNode);
        NKWebKit::InspectorAbi::Retain(callStack);
        AsyncStackTrace* clone = Create(callStack, true, nullptr);
        NKWebKit::InspectorAbi::Release(std::exchange(previousNode->parent, clone));
        clone->childCount = 1;
        previousNode = clone;

        if (sourceNode == newStackTraceRoot) break;

        AsyncStackTrace* next = sourceNode->parent;
        NKWebKit::InspectorAbi::Retain(next);
        NKWebKit::InspectorAbi::Release(sourceNode);
        sourceNode = next;
    }

    previousNode->truncated = true;
    NKWebKit::InspectorAbi::Release(sourceNode);
}

}

namespace NKWebKit::InspectorAbi {

void Retain(AsyncStackTrace* trace) {
    if (!trace) return;
    std::atomic_ref<unsigned> refCount(trace->refCount);
    unsigned count = refCount.load(std::memory_order_relaxed);
    for (;;) {
        if (count == std::numeric_limits<unsigned>::max())
            throw std::logic_error("Inspector::AsyncStackTrace reference count overflow");
        if (refCount.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
            return;
    }
}

void Release(AsyncStackTrace* trace) {
    if (!trace) return;
    std::atomic_ref<unsigned> refCount(trace->refCount);
    unsigned count = refCount.load(std::memory_order_relaxed);
    for (;;) {
        if (!count) throw std::logic_error("Inspector::AsyncStackTrace released at reference count 0");
        if (count != 1) {
            if (refCount.compare_exchange_weak(count, count - 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                return;
            continue;
        }
        if (refCount.compare_exchange_weak(count, 0, std::memory_order_acq_rel, std::memory_order_relaxed))
            break;
    }
    Destroy(*trace);
    _ZN3WTF8fastFreeEPv(trace);
}

}

extern "C" {

void APS5_VABI _ZN9Inspector15AsyncStackTrace18didCancelAsyncCallEv(AsyncStackTrace* self) {
    auto& trace = Checked(self);
    if (trace.state == AsyncStackTraceState::Canceled) return;
    if (trace.state == AsyncStackTraceState::Pending && !trace.childCount) Remove(trace);
    trace.state = AsyncStackTraceState::Canceled;
}

void APS5_VABI _ZN9Inspector15AsyncStackTrace20didDispatchAsyncCallEv(AsyncStackTrace* self) {
    auto& trace = Checked(self);
    if (trace.state == AsyncStackTraceState::Active && !trace.singleShot) {
        trace.state = AsyncStackTraceState::Pending;
        return;
    }
    trace.state = AsyncStackTraceState::Dispatched;
    if (!trace.childCount) Remove(trace);
}

void APS5_VABI _ZN9Inspector15AsyncStackTrace21willDispatchAsyncCallEm(AsyncStackTrace* self, std::size_t maxDepth) {
    auto& trace = Checked(self);
    trace.state = AsyncStackTraceState::Active;
    Truncate(trace, maxDepth);
}

void APS5_VABI _ZN9Inspector15AsyncStackTrace6removeEv(AsyncStackTrace* self) {
    Remove(Checked(self));
}

void APS5_VABI _ZN9Inspector15AsyncStackTrace8truncateEm(AsyncStackTrace* self, std::size_t maxDepth) {
    Truncate(Checked(self), maxDepth);
}

void APS5_VABI _ZN9Inspector15AsyncStackTraceD1Ev(AsyncStackTrace* self) {
    Destroy(Checked(self));
}

void APS5_VABI _ZN9Inspector15AsyncStackTraceD2Ev(AsyncStackTrace* self) {
    Destroy(Checked(self));
}

bool APS5_VABI _ZNK9Inspector15AsyncStackTrace8isLockedEv(const AsyncStackTrace* self) {
    return IsLocked(Checked(self));
}

bool APS5_VABI _ZNK9Inspector15AsyncStackTrace9isPendingEv(const AsyncStackTrace* self) {
    return Checked(self).state == AsyncStackTraceState::Pending;
}

}
