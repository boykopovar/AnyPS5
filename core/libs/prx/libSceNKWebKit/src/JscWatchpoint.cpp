// Derived from neonmodder123/PS5-WebKit-11.x/Source/JavaScriptCore/bytecode/Watchpoint.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/JscWatchpointExports.hpp"

#include <atomic>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::JscAbi::DeferredWatchpointFire;
using NKWebKit::JscAbi::IsInvalidated;
using NKWebKit::JscAbi::NodeNext;
using NKWebKit::JscAbi::NodePrev;
using NKWebKit::JscAbi::SentinelAddress;
using NKWebKit::JscAbi::SentinelNext;
using NKWebKit::JscAbi::SentinelPrev;
using NKWebKit::JscAbi::SetNodeNext;
using NKWebKit::JscAbi::SetNodePrev;
using NKWebKit::JscAbi::SetSentinelNext;
using NKWebKit::JscAbi::SetSentinelPrev;
using NKWebKit::JscAbi::WatchpointSet;
using NKWebKit::JscAbi::WatchpointState;

WatchpointSet& Checked(WatchpointSet* self) {
    if (!self) throw std::invalid_argument("null JSC::WatchpointSet");
    return *self;
}

DeferredWatchpointFire& Checked(DeferredWatchpointFire* self) {
    if (!self) throw std::invalid_argument("null JSC::DeferredWatchpointFire");
    return *self;
}

void StoreStoreFence() {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

void RemoveListNode(void* node) {
    void* next = NodeNext(node);
    void* prev = NodePrev(node);
    SetNodeNext(prev, next);
    SetNodePrev(next, prev);
    SetNodeNext(node, nullptr);
    SetNodePrev(node, nullptr);
}

void TakeListFrom(WatchpointSet& target, WatchpointSet& other) {
    if (SentinelNext(other) == SentinelAddress(other)) return;
    void* previous = SentinelPrev(target);
    void* otherNext = SentinelNext(other);
    void* otherPrev = SentinelPrev(other);
    SetNodeNext(previous, otherNext);
    SetNodePrev(otherNext, previous);
    SetSentinelPrev(target, otherPrev);
    SetNodeNext(SentinelPrev(target), SentinelAddress(target));
    SetSentinelNext(other, SentinelAddress(other));
    SetSentinelPrev(other, SentinelAddress(other));
}

void TakeWatchpointsToFire(WatchpointSet& target, WatchpointSet& other) {
    TakeListFrom(target, other);
    target.setIsNotEmpty = other.setIsNotEmpty;
    target.state = other.state;
    other.setIsNotEmpty = 0;
}

void ConstructWatchpointSet(WatchpointSet* self, WatchpointState state) {
    Checked(self);
    self->refCount = 1;
    self->state = static_cast<std::int8_t>(state);
    self->setIsNotEmpty = 0;
    SetSentinelNext(*self, SentinelAddress(*self));
    SetSentinelPrev(*self, SentinelAddress(*self));
}

void DestroyWatchpointSet(WatchpointSet* self) {
    Checked(self);
    while (SentinelNext(*self) != SentinelAddress(*self)) {
        RemoveListNode(SentinelNext(*self));
    }
}

void FireAllSlowDeferred(WatchpointSet* self, void*, DeferredWatchpointFire* deferredWatchpoints) {
    Checked(self);
    Checked(deferredWatchpoints);
    StoreStoreFence();
    TakeWatchpointsToFire(deferredWatchpoints->watchpointsToFire, *self);
    self->state = static_cast<std::int8_t>(IsInvalidated);
    StoreStoreFence();
}

}

extern "C" {

void APS5_VABI _ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE(WatchpointSet* self, WatchpointState state) {
    ConstructWatchpointSet(self, state);
}

void APS5_VABI _ZN3JSC13WatchpointSetC2ENS_15WatchpointStateE(WatchpointSet* self, WatchpointState state) {
    ConstructWatchpointSet(self, state);
}

void APS5_VABI _ZN3JSC13WatchpointSetD1Ev(WatchpointSet* self) {
    DestroyWatchpointSet(self);
}

void APS5_VABI _ZN3JSC13WatchpointSetD2Ev(WatchpointSet* self) {
    DestroyWatchpointSet(self);
}

void APS5_VABI _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(WatchpointSet* self, void* vm, DeferredWatchpointFire* deferredWatchpoints) {
    FireAllSlowDeferred(self, vm, deferredWatchpoints);
}

}
