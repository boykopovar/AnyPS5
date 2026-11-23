// Derived from Source/WebCore/dom/UserGestureIndicator.cpp@08b3d0cd20ab0329b6354a6e625c44660b6fb6d6
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

#include "prx/libSceNKWebKit/include/UserGestureExports.hpp"
#include "prx/libSceNKWebKit/include/UserGestureIndicatorExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

static_assert(sizeof(WTF::Seconds) == 8);
static_assert(alignof(WTF::Seconds) == 8);
static_assert(sizeof(WebCore::UserGestureToken*) == 8);

namespace {

using NKWebKit::WebCoreAbi::DefaultWeakPtrImpl;
using NKWebKit::WebCoreAbi::TokenCallableWrapper;
using NKWebKit::WebCoreAbi::TokenFunction;
using NKWebKit::WebCoreAbi::WeakHashSet;
using NKWebKit::WebCoreAbi::WeakPtrImplWithEventTargetData;
using WebCore::DOMPasteAccessPolicy;
using WebCore::GestureScope;
using WebCore::IsPropagatedFromFetch;
using WebCore::UserGestureIndicator;
using WebCore::UserGestureToken;

constexpr std::uint64_t WeakPtrFactoryPointerMask = (std::uint64_t{1} << 48) - 1;
constexpr std::uintptr_t HashTableDeletedBucket = static_cast<std::uintptr_t>(-1);
constexpr std::size_t HashTableMetadataSize = 16;

DefaultWeakPtrImpl* WeakPtrFactoryPointer(std::uint64_t data) {
    return static_cast<DefaultWeakPtrImpl*>(reinterpret_cast<void*>(static_cast<std::uintptr_t>(data & WeakPtrFactoryPointerMask)));
}

void RefToken(UserGestureToken* token) {
    if (token) ++token->refCount;
}

void DerefToken(UserGestureToken* token) {
    if (!token) return;
    unsigned remaining = token->refCount - 1;
    if (remaining) {
        token->refCount = remaining;
        return;
    }
    _ZN7WebCore16UserGestureTokenD1Ev(token);
    _ZN3WTF8fastFreeEPv(token);
}

void DerefDefaultWeakPtrImpl(DefaultWeakPtrImpl* impl) {
    if (!impl) return;
    std::atomic_ref<unsigned> refCount(impl->refCount);
    if (refCount.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
    refCount.store(1, std::memory_order_relaxed);
    _ZN3WTF8fastFreeEPv(impl);
}

bool ReleaseWeakHashSet(const WeakHashSet& set) {
    if (!set.table) return true;
    unsigned size = reinterpret_cast<unsigned*>(set.table)[-1];
    auto* buckets = static_cast<WeakPtrImplWithEventTargetData**>(set.table);
    for (unsigned i = 0; i < size; ++i) {
        WeakPtrImplWithEventTargetData* impl = buckets[i];
        if (!impl || reinterpret_cast<std::uintptr_t>(impl) == HashTableDeletedBucket) continue;
        std::atomic_ref<unsigned> refCount(impl->refCount);
        if (refCount.load(std::memory_order_acquire) == 1) return false;
    }
    for (unsigned i = 0; i < size; ++i) {
        WeakPtrImplWithEventTargetData* impl = buckets[i];
        if (!impl || reinterpret_cast<std::uintptr_t>(impl) == HashTableDeletedBucket) continue;
        std::atomic_ref<unsigned> refCount(impl->refCount);
        refCount.fetch_sub(1, std::memory_order_acq_rel);
    }
    _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(set.table) - HashTableMetadataSize);
    return true;
}

void DestroyObserverVector(TokenFunction* buffer, unsigned size) {
    if (!buffer) return;
    for (unsigned i = 0; i < size; ++i) {
        TokenCallableWrapper* wrapper = buffer[i].callableWrapper;
        if (wrapper) wrapper->vtable->deletingDestructor(wrapper);
    }
    _ZN3WTF8fastFreeEPv(buffer);
}

void DestroyToken(UserGestureToken& token, const char* thrower) {
    for (unsigned i = 0; i < token.destructionObserversSize; ++i) {
        TokenCallableWrapper* wrapper = token.destructionObserversBuffer[i].callableWrapper;
        if (!wrapper) APS5_INVALID_ARG_EX;
        wrapper->vtable->call(wrapper, token);
    }
    if (!ReleaseWeakHashSet(token.documentsImpactedByUserGesture)) {
        NotImplemented_nid_no_patch(thrower);
    }
    DestroyObserverVector(token.destructionObserversBuffer, token.destructionObserversSize);
    DefaultWeakPtrImpl* factoryImpl = WeakPtrFactoryPointer(token.weakPtrFactoryData);
    if (factoryImpl) factoryImpl->ptr = nullptr;
    DerefDefaultWeakPtrImpl(factoryImpl);
}

void DestroyIndicator(UserGestureIndicator& self) {
    UserGestureToken* previous = self.previousToken;
    if (_ZN3WTF12isMainThreadEv()) {
        UserGestureToken* current = NKWebKit::UserGesture::CurrentToken();
        if (current) {
            current->domPasteAccessPolicy = DOMPasteAccessPolicy::NotRequestedYet;
            current->scope = GestureScope::All;
            current->isPropagatedFromFetch = IsPropagatedFromFetch::No;
        }
        RefToken(previous);
        DerefToken(current);
        NKWebKit::UserGesture::CurrentToken() = previous;
    }
    DerefToken(previous);
}

void ConstructIndicator(UserGestureIndicator& self, const NKWebKit::WebCoreAbi::UserGestureTokenRef& tokenRef, GestureScope scope, IsPropagatedFromFetch isPropagatedFromFetch) {
    self.previousToken = nullptr;
    if (!_ZN3WTF12isMainThreadEv()) return;
    UserGestureToken* previous = NKWebKit::UserGesture::CurrentToken();
    RefToken(previous);
    self.previousToken = previous;
    UserGestureToken* token = tokenRef.ptr;
    if (token) {
        token->scope = scope;
        token->isPropagatedFromFetch = isPropagatedFromFetch;
        UserGestureToken* replaced = NKWebKit::UserGesture::CurrentToken();
        NKWebKit::UserGesture::CurrentToken() = token;
        RefToken(token);
        DerefToken(replaced);
    }
}

}  // namespace

namespace NKWebKit::UserGesture {

WebCore::UserGestureToken*& CurrentToken() {
    static WebCore::UserGestureToken* token = nullptr;
    return token;
}

WTF::Seconds& MaximumIntervalForUserGestureForwardingForFetch() {
    static WTF::Seconds interval{10.0};
    return interval;
}

}  // namespace NKWebKit::UserGesture

extern "C" {

void APS5_VABI _ZN7WebCore16UserGestureToken60setMaximumIntervalForUserGestureForwardingForFetchForTestingEN3WTF7SecondsE(WTF::Seconds value) {
    NKWebKit::UserGesture::MaximumIntervalForUserGestureForwardingForFetch() = value;
}

WebCore::UserGestureToken** APS5_VABI _ZN7WebCore20UserGestureIndicator18currentUserGestureEv(WebCore::UserGestureToken** result) {
    *result = nullptr;
    if (!_ZN3WTF12isMainThreadEv()) {
        return result;
    }
    if (NKWebKit::UserGesture::CurrentToken()) {
        NotImplemented_nid_no_patch(__func__);
    }
    return result;
}

bool APS5_VABI _ZN7WebCore20UserGestureIndicator21processingUserGestureEv() {
    if (!_ZN3WTF12isMainThreadEv()) {
        return false;
    }
    if (NKWebKit::UserGesture::CurrentToken()) {
        NotImplemented_nid_no_patch(__func__);
    }
    return false;
}

bool APS5_VABI _ZN7WebCore20UserGestureIndicator29processingUserGestureForMediaEv() {
    if (!_ZN3WTF12isMainThreadEv()) {
        return false;
    }
    if (NKWebKit::UserGesture::CurrentToken()) {
        NotImplemented_nid_no_patch(__func__);
    }
    return false;
}

void APS5_VABI _ZN7WebCore20UserGestureIndicatorC1EN3WTF6RefPtrINS_16UserGestureTokenENS1_13DumbPtrTraitsIS3_EEEENS3_12GestureScopeENS3_21IsPropagatedFromFetchE(UserGestureIndicator* self, const NKWebKit::WebCoreAbi::UserGestureTokenRef& token, GestureScope scope, IsPropagatedFromFetch isPropagatedFromFetch) {
    _ZN7WebCore20UserGestureIndicatorC2EN3WTF6RefPtrINS_16UserGestureTokenENS1_13DumbPtrTraitsIS3_EEEENS3_12GestureScopeENS3_21IsPropagatedFromFetchE(self, token, scope, isPropagatedFromFetch);
}

void APS5_VABI _ZN7WebCore20UserGestureIndicatorC2EN3WTF6RefPtrINS_16UserGestureTokenENS1_13DumbPtrTraitsIS3_EEEENS3_12GestureScopeENS3_21IsPropagatedFromFetchE(UserGestureIndicator* self, const NKWebKit::WebCoreAbi::UserGestureTokenRef& token, GestureScope scope, IsPropagatedFromFetch isPropagatedFromFetch) {
    ConstructIndicator(*self, token, scope, isPropagatedFromFetch);
}

void APS5_VABI _ZN7WebCore20UserGestureIndicatorD1Ev(UserGestureIndicator* self) {
    _ZN7WebCore20UserGestureIndicatorD2Ev(self);
}

void APS5_VABI _ZN7WebCore20UserGestureIndicatorD2Ev(UserGestureIndicator* self) {
    DestroyIndicator(*self);
}

void APS5_VABI _ZN7WebCore16UserGestureTokenD1Ev(UserGestureToken* self) {
    DestroyToken(*self, "_ZN7WebCore16UserGestureTokenD1Ev");
}

void APS5_VABI _ZN7WebCore16UserGestureTokenD2Ev(UserGestureToken* self) {
    DestroyToken(*self, "_ZN7WebCore16UserGestureTokenD2Ev");
}

}
