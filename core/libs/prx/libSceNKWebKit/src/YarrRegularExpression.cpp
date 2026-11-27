// Derived from Source/JavaScriptCore/yarr/RegularExpression.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2011 Peter Varga (pvarga@webkit.org), University of Szeged
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

#include "prx/libSceNKWebKit/include/YarrRegularExpressionExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace {

using Jsc::YarrAbi::RegularExpression;
using Jsc::YarrAbi::RegularExpressionPrivate;

static_assert(std::is_trivially_copyable_v<RegularExpression>);
static_assert(std::is_trivially_copyable_v<RegularExpressionPrivate>);

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template<typename T>
void RequireWritableGuestValue(T* pointer, const char* function) {
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, sizeof(T));
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, sizeof(T));
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(T)));
    return false;
}

template<typename T>
struct GuestRollbackGuard {
    T* target;
    T previous;
    bool committed = false;

    ~GuestRollbackGuard() {
        if (!committed && target) {
            try {
                static_cast<void>(GuestCopyOut_nid_no_patch(target, &previous, sizeof(T)));
            } catch (...) {
            }
        }
    }
};

void ValidatePrivateWritable(RegularExpressionPrivate* target, const char* function) {
    if (!target) return;
    static_cast<void>(ReadGuestValue(target, function));
    RequireWritableGuestValue(&target->refCount, function);
}

void DestroyRegularExpression(RegularExpression* self, const char* function) {
    if (!self) throw std::invalid_argument("JSC::Yarr::RegularExpression::~RegularExpression: null this");
    const RegularExpression local = ReadGuestValue(self, function);
    ValidatePrivateWritable(local.d, function);
    JscRegularExpressionRelease_nid_no_patch(local.d);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3JSC4Yarr17RegularExpressionD1Ev(RegularExpression* self) {
    DestroyRegularExpression(self, __func__);
}

void APS5_VABI _ZN3JSC4Yarr17RegularExpressionD2Ev(RegularExpression* self) {
    DestroyRegularExpression(self, __func__);
}

RegularExpression* APS5_VABI _ZN3JSC4Yarr17RegularExpressionaSERKS1_(RegularExpression* self, const RegularExpression* other) {
    if (!self) throw std::invalid_argument("JSC::Yarr::RegularExpression::operator=: null this");
    if (!other) throw std::invalid_argument("JSC::Yarr::RegularExpression::operator=: null source");
    const RegularExpression source = ReadGuestValue(other, __func__);
    const RegularExpression target = ReadGuestValue(self, __func__);
    RequireWritableGuestValue(self, __func__);
    ValidatePrivateWritable(source.d, __func__);
    if (target.d != source.d) ValidatePrivateWritable(target.d, __func__);
    if (source.d == target.d) return self;
    GuestRollbackGuard<std::uint32_t> refRollback{nullptr, 0};
    if (source.d) {
        const std::uint32_t previousRefCount = ReadGuestValue(&source.d->refCount, __func__);
        const std::uint32_t nextRefCount = previousRefCount + 1;
        if (!TryPublishGuestValue(&source.d->refCount, nextRefCount, previousRefCount))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        refRollback.target = &source.d->refCount;
        refRollback.previous = previousRefCount;
    }
    const RegularExpression updated{source.d};
    if (!TryPublishGuestValue(self, updated, target))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    refRollback.committed = true;
    JscRegularExpressionRelease_nid_no_patch(target.d);
    return self;
}

}
