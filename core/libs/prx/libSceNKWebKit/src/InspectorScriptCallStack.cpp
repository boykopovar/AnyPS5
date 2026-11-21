// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/ScriptCallStack.cpp
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (c) 2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/ScriptCallStack.h
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (c) 2008, 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
using NKWebKit::InspectorAbi::ScriptCallFrame;
using NKWebKit::InspectorAbi::ScriptCallStack;
using NKWebKit::InspectorAbi::ScriptCallStackRef;

ScriptCallStack& Checked(ScriptCallStack* stack) {
    if (!stack) throw std::invalid_argument("null Inspector::ScriptCallStack");
    return *stack;
}

const ScriptCallStack& Checked(const ScriptCallStack* stack) {
    if (!stack) throw std::invalid_argument("null Inspector::ScriptCallStack");
    return *stack;
}

void CheckFrames(const ScriptCallStack& stack) {
    if (stack.frames.size && !stack.frames.buffer) throw std::invalid_argument("null Inspector::ScriptCallFrame buffer");
}

void Construct(ScriptCallStack& stack) {
    stack.refCount = 1;
    stack.frames.buffer = nullptr;
    stack.frames.capacity = 0;
    stack.frames.size = 0;
    stack.truncated = false;
    stack.parentStackTrace = nullptr;
}

void Destroy(ScriptCallStack& stack) {
    CheckFrames(stack);
    NKWebKit::InspectorAbi::Release(std::exchange(stack.parentStackTrace, nullptr));
    for (unsigned i = 0; i < stack.frames.size; ++i)
        _ZN9Inspector15ScriptCallFrameD1Ev(&stack.frames.buffer[i]);
    if (stack.frames.buffer) _ZN3WTF8fastFreeEPv(stack.frames.buffer);
}

}

namespace NKWebKit::InspectorAbi {

void Retain(ScriptCallStack* stack) {
    if (!stack) return;
    std::atomic_ref<unsigned> refCount(stack->refCount);
    unsigned count = refCount.load(std::memory_order_relaxed);
    for (;;) {
        if (count == std::numeric_limits<unsigned>::max())
            throw std::logic_error("Inspector::ScriptCallStack reference count overflow");
        if (refCount.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
            return;
    }
}

void Release(ScriptCallStack* stack) {
    if (!stack) return;
    std::atomic_ref<unsigned> refCount(stack->refCount);
    unsigned count = refCount.load(std::memory_order_relaxed);
    for (;;) {
        if (!count) throw std::logic_error("Inspector::ScriptCallStack released at reference count 0");
        if (count != 1) {
            if (refCount.compare_exchange_weak(count, count - 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                return;
            continue;
        }
        if (refCount.compare_exchange_weak(count, 0, std::memory_order_acq_rel, std::memory_order_relaxed))
            break;
    }
    Destroy(*stack);
    _ZN3WTF8fastFreeEPv(stack);
}

ScriptCallStackRef::~ScriptCallStackRef() {
    Release(ptr);
}

}

extern "C" {

ScriptCallStackRef APS5_VABI _ZN9Inspector15ScriptCallStack6createEv() {
    auto* stack = static_cast<ScriptCallStack*>(_ZN3WTF10fastMallocEm(sizeof(ScriptCallStack)));
    Construct(*stack);
    return ScriptCallStackRef(stack);
}

void APS5_VABI _ZN9Inspector15ScriptCallStackC1Ev(ScriptCallStack* self) {
    Construct(Checked(self));
}

void APS5_VABI _ZN9Inspector15ScriptCallStackC2Ev(ScriptCallStack* self) {
    Construct(Checked(self));
}

void APS5_VABI _ZN9Inspector15ScriptCallStackD1Ev(ScriptCallStack* self) {
    Destroy(Checked(self));
}

void APS5_VABI _ZN9Inspector15ScriptCallStackD2Ev(ScriptCallStack* self) {
    Destroy(Checked(self));
}

const ScriptCallFrame& APS5_VABI _ZNK9Inspector15ScriptCallStack2atEm(const ScriptCallStack* self, std::size_t index) {
    const auto& stack = Checked(self);
    CheckFrames(stack);
    if (index >= stack.frames.size) throw std::out_of_range("Inspector::ScriptCallStack::at");
    return stack.frames.buffer[index];
}

const ScriptCallFrame* APS5_VABI _ZNK9Inspector15ScriptCallStack23firstNonNativeCallFrameEv(const ScriptCallStack* self) {
    const auto& stack = Checked(self);
    CheckFrames(stack);
    for (unsigned i = 0; i < stack.frames.size; ++i) {
        if (!NKWebKit::InspectorAbi::IsNative(stack.frames.buffer[i])) return &stack.frames.buffer[i];
    }
    return nullptr;
}

bool APS5_VABI _ZNK9Inspector15ScriptCallStack7isEqualEPS0_(const ScriptCallStack* self, ScriptCallStack* other) {
    const auto& stack = Checked(self);
    CheckFrames(stack);
    if (!other) return false;
    CheckFrames(*other);
    if (other->frames.size != stack.frames.size) return false;
    for (std::size_t i = 0; i < stack.frames.size; ++i) {
        if (!NKWebKit::InspectorAbi::FramesEqual(stack.frames.buffer[i], other->frames.buffer[i])) return false;
    }
    return true;
}

}
