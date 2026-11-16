// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/InspectorFrontendRouter.cpp
/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/InspectorFrontendRouter.h
/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/InspectorFrontendChannel.h
/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/InspectorFrontendRouterExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace {

using NKWebKit::InspectorAbi::ConnectionType;
using NKWebKit::InspectorAbi::FrontendChannel;
using NKWebKit::InspectorAbi::FrontendRouter;
using NKWebKit::InspectorAbi::FrontendRouterRef;
using NKWebKit::WtfAbi::String;

constexpr unsigned InlineCapacity = 2;
constexpr unsigned MinimumCapacity = 16;
constexpr std::size_t MaximumCapacity = std::numeric_limits<unsigned>::max() / sizeof(FrontendChannel*);

FrontendRouter& Checked(FrontendRouter* self) {
    if (!self) throw std::invalid_argument("null Inspector::FrontendRouter");
    return *self;
}

const FrontendRouter& Checked(const FrontendRouter* self) {
    if (!self) throw std::invalid_argument("null Inspector::FrontendRouter");
    return *self;
}

FrontendChannel& CheckedConnection(FrontendChannel* connection) {
    if (!connection) throw std::invalid_argument("null Inspector::FrontendChannel");
    return *connection;
}

ConnectionType ConnectionTypeOf(FrontendChannel& connection) {
    using Function = ConnectionType (APS5_VABI*)(const FrontendChannel*);
    return reinterpret_cast<Function>(const_cast<void*>(connection.vtable[2]))(&connection);
}

void SendMessageToFrontend(FrontendChannel& connection, const String* message) {
    using Function = void (APS5_VABI*)(FrontendChannel*, const String*);
    reinterpret_cast<Function>(const_cast<void*>(connection.vtable[3]))(&connection, message);
}

bool Contains(const FrontendRouter& router, const FrontendChannel* connection) {
    for (unsigned i = 0; i < router.size; ++i) {
        if (router.buffer[i] == connection) return true;
    }
    return false;
}

void Grow(FrontendRouter& router) {
    const std::size_t capacity = router.capacity;
    const std::size_t grown = capacity + capacity / 4 + 1;
    const std::size_t minimum = grown < MinimumCapacity ? MinimumCapacity : grown;
    const std::size_t needed = static_cast<std::size_t>(router.size) + 1;
    const std::size_t newCapacity = minimum < needed ? needed : minimum;
    if (newCapacity > MaximumCapacity) throw std::length_error("Inspector::FrontendRouter connection capacity");
    auto** newBuffer = static_cast<FrontendChannel**>(_ZN3WTF10fastMallocEm(newCapacity * sizeof(FrontendChannel*)));
    for (unsigned i = 0; i < router.size; ++i) newBuffer[i] = router.buffer[i];
    if (router.buffer != router.inlineBuffer) _ZN3WTF8fastFreeEPv(router.buffer);
    router.buffer = newBuffer;
    router.capacity = static_cast<unsigned>(newCapacity);
}

void Append(FrontendRouter& router, FrontendChannel* connection) {
    if (router.size == router.capacity) Grow(router);
    router.buffer[router.size] = connection;
    ++router.size;
}

void RemoveFirst(FrontendRouter& router, const FrontendChannel* connection) {
    for (unsigned i = 0; i < router.size; ++i) {
        if (router.buffer[i] != connection) continue;
        for (unsigned j = i + 1; j < router.size; ++j) router.buffer[j - 1] = router.buffer[j];
        --router.size;
        return;
    }
}

void CheckChannels(const FrontendRouter& router) {
    for (unsigned i = 0; i < router.size; ++i) CheckedConnection(router.buffer[i]);
}

void Clear(FrontendRouter& router) {
    if (router.buffer != router.inlineBuffer) _ZN3WTF8fastFreeEPv(router.buffer);
    router.buffer = router.inlineBuffer;
    router.capacity = InlineCapacity;
    router.size = 0;
}

void Construct(FrontendRouter& router) {
    router.refCount = 1;
    router.buffer = router.inlineBuffer;
    router.capacity = InlineCapacity;
    router.size = 0;
}

void Destroy(FrontendRouter& router) {
    if (router.buffer != router.inlineBuffer) _ZN3WTF8fastFreeEPv(router.buffer);
}

void Release(FrontendRouter* router) {
    if (!router) return;
    std::atomic_ref<unsigned> refCount(router->refCount);
    if (refCount.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
    Destroy(*router);
    _ZN3WTF8fastFreeEPv(router);
}

}

namespace NKWebKit::InspectorAbi {

FrontendRouterRef::~FrontendRouterRef() {
    Release(ptr);
}

}

extern "C" {

FrontendRouterRef APS5_VABI _ZN9Inspector14FrontendRouter6createEv() {
    auto* router = static_cast<FrontendRouter*>(_ZN3WTF10fastMallocEm(sizeof(FrontendRouter)));
    Construct(*router);
    return FrontendRouterRef(router);
}

void APS5_VABI _ZN9Inspector14FrontendRouterC1Ev(FrontendRouter* self) {
    Construct(Checked(self));
}

void APS5_VABI _ZN9Inspector14FrontendRouterC2Ev(FrontendRouter* self) {
    Construct(Checked(self));
}

void APS5_VABI _ZN9Inspector14FrontendRouterD1Ev(FrontendRouter* self) {
    Destroy(Checked(self));
}

void APS5_VABI _ZN9Inspector14FrontendRouterD2Ev(FrontendRouter* self) {
    Destroy(Checked(self));
}

void APS5_VABI _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(FrontendRouter* self, FrontendChannel* connection) {
    FrontendRouter& router = Checked(self);
    FrontendChannel& channel = CheckedConnection(connection);
    if (Contains(router, &channel)) return;
    Append(router, &channel);
}

void APS5_VABI _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(FrontendRouter* self, FrontendChannel* connection) {
    FrontendRouter& router = Checked(self);
    FrontendChannel& channel = CheckedConnection(connection);
    if (!Contains(router, &channel)) return;
    RemoveFirst(router, &channel);
}

void APS5_VABI _ZN9Inspector14FrontendRouter22disconnectAllFrontendsEv(FrontendRouter* self) {
    Clear(Checked(self));
}

bool APS5_VABI _ZNK9Inspector14FrontendRouter12hasFrontendsEv(const FrontendRouter* self) {
    return Checked(self).size != 0;
}

bool APS5_VABI _ZNK9Inspector14FrontendRouter16hasLocalFrontendEv(const FrontendRouter* self) {
    const FrontendRouter& router = Checked(self);
    CheckChannels(router);
    FrontendChannel* const* buffer = router.buffer;
    const unsigned size = router.size;
    for (unsigned i = 0; i < size; ++i) {
        if (ConnectionTypeOf(CheckedConnection(buffer[i])) == ConnectionType::Local) return true;
    }
    return false;
}

bool APS5_VABI _ZNK9Inspector14FrontendRouter17hasRemoteFrontendEv(const FrontendRouter* self) {
    const FrontendRouter& router = Checked(self);
    CheckChannels(router);
    FrontendChannel* const* buffer = router.buffer;
    const unsigned size = router.size;
    for (unsigned i = 0; i < size; ++i) {
        if (ConnectionTypeOf(CheckedConnection(buffer[i])) == ConnectionType::Remote) return true;
    }
    return false;
}

void APS5_VABI _ZNK9Inspector14FrontendRouter9sendEventERKN3WTF6StringE(const FrontendRouter* self, const String* message) {
    const FrontendRouter& router = Checked(self);
    CheckChannels(router);
    FrontendChannel* const* buffer = router.buffer;
    const unsigned size = router.size;
    for (unsigned i = 0; i < size; ++i)
        SendMessageToFrontend(CheckedConnection(buffer[i]), message);
}

void APS5_VABI _ZNK9Inspector14FrontendRouter12sendResponseERKN3WTF6StringE(const FrontendRouter* self, const String* message) {
    const FrontendRouter& router = Checked(self);
    CheckChannels(router);
    FrontendChannel* const* buffer = router.buffer;
    const unsigned size = router.size;
    for (unsigned i = 0; i < size; ++i)
        SendMessageToFrontend(CheckedConnection(buffer[i]), message);
}

}
