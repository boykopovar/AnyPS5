// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/TrackPrivateBase.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Cable Television Labs, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#include "prx/libSceNKWebKit/include/TrackPrivateBaseExports.hpp"
#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstring>

namespace {

NKWebKit::WtfAbi::AtomString EmptyAtom() {
    return NKWebKit::WtfAbi::AtomString{NKWebKit::WtfAbi::String(NKWebKit::WtfAbi::EmptyStringImpl())};
}

struct TrackPrivateBaseVtable {
    void (APS5_VABI* completeDestructor)(void*);
    void (APS5_VABI* deletingDestructor)(void*);
    void (APS5_VABI* client)();
    NKWebKit::WtfAbi::AtomString (APS5_VABI* id)(const void*);
    NKWebKit::WtfAbi::AtomString (APS5_VABI* label)(const void*);
    NKWebKit::WtfAbi::AtomString (APS5_VABI* language)(const void*);
    int (APS5_VABI* trackIndex)();
    WTF::MediaTime (APS5_VABI* startTimeVariance)(const void*);
};

struct TrackPrivateBaseVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    TrackPrivateBaseVtable dispatch;
};

struct TrackPrivateBase {
    const TrackPrivateBaseVtable* vptr;
    std::uint32_t refCount;
};

static_assert(offsetof(TrackPrivateBaseVtableStorage, dispatch) == 16);
static_assert(sizeof(TrackPrivateBaseVtable) == 64);
static_assert(sizeof(TrackPrivateBase) == 16);
static_assert(offsetof(TrackPrivateBase, vptr) == 0);
static_assert(offsetof(TrackPrivateBase, refCount) == 8);

void APS5_VABI CompleteDestructor(void*) {
    _ZN7WebCore16TrackPrivateBaseD1Ev();
}

void APS5_VABI DeletingDestructor(void*) {
    _ZN7WebCore16TrackPrivateBaseD0Ev();
}

const TrackPrivateBaseVtableStorage TrackPrivateBaseVtableInstance = {
    0,
    nullptr,
    {
        &CompleteDestructor,
        &DeletingDestructor,
        &__cxa_pure_virtual_nid_postfix,
        &_ZNK7WebCore16TrackPrivateBase2idEv,
        &_ZNK7WebCore16TrackPrivateBase5labelEv,
        &_ZNK7WebCore16TrackPrivateBase8languageEv,
        &_ZNK7WebCore16TrackPrivateBase10trackIndexEv,
        &_ZNK7WebCore16TrackPrivateBase17startTimeVarianceEv,
    },
};

constexpr std::size_t kInitializedBytes = offsetof(TrackPrivateBase, refCount) + sizeof(std::uint32_t);
static_assert(kInitializedBytes == 12);

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const GuestArena::HostWrite guard(pointer, size);
    if (!guard.Open() || !GuestWritable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

}

extern "C" {

void APS5_VABI _ZN7WebCore16TrackPrivateBaseC2Ev(void* self) {
    if (self == nullptr) throw std::invalid_argument("WebCore::TrackPrivateBase::TrackPrivateBase: null object");
    RequireWritableGuestBytes(self, kInitializedBytes, __func__);
    unsigned char initialBytes[kInitializedBytes]{};
    const TrackPrivateBaseVtable* const vptr = &TrackPrivateBaseVtableInstance.dispatch;
    const std::uint32_t refCount = 1;
    std::memcpy(initialBytes + offsetof(TrackPrivateBase, vptr), &vptr, sizeof(vptr));
    std::memcpy(initialBytes + offsetof(TrackPrivateBase, refCount), &refCount, sizeof(refCount));
    if (!GuestCopyOut_nid_no_patch(self, initialBytes, kInitializedBytes))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

NKWebKit::WtfAbi::AtomString APS5_VABI _ZNK7WebCore16TrackPrivateBase2idEv(const void*) {
    return EmptyAtom();
}

NKWebKit::WtfAbi::AtomString APS5_VABI _ZNK7WebCore16TrackPrivateBase5labelEv(const void*) {
    return EmptyAtom();
}

NKWebKit::WtfAbi::AtomString APS5_VABI _ZNK7WebCore16TrackPrivateBase8languageEv(const void*) {
    return EmptyAtom();
}

WTF::MediaTime APS5_VABI _ZNK7WebCore16TrackPrivateBase17startTimeVarianceEv(const void*) {
    return _ZN3WTF9MediaTime8zeroTimeEv();
}

}
