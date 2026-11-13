// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/loader/CrossOriginAccessControl.cpp
/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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
 *
 */

#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/include/CrossOriginAccessControlExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler;
using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisablerVtableStorage;

CrossOriginAccessControlCheckDisabler& Checked(CrossOriginAccessControlCheckDisabler* self) {
    if (!self) throw std::invalid_argument("null WebCore::CrossOriginAccessControlCheckDisabler");
    return *self;
}

const CrossOriginAccessControlCheckDisabler& Checked(const CrossOriginAccessControlCheckDisabler* self) {
    if (!self) throw std::invalid_argument("null WebCore::CrossOriginAccessControlCheckDisabler");
    return *self;
}

const CrossOriginAccessControlCheckDisablerVtableStorage Vtable = {
    0,
    nullptr,
    {
        &_ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev,
        &_ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev,
        &_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv,
    },
};

}

extern "C" {

CrossOriginAccessControlCheckDisabler& APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv() {
    static CrossOriginAccessControlCheckDisabler disabler{&Vtable.dispatch, true};
    return disabler;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(CrossOriginAccessControlCheckDisabler* self, bool enabled) {
    Checked(self).accessControlCheckEnabled = enabled;
}

bool APS5_VABI _ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv(const CrossOriginAccessControlCheckDisabler* self) {
    return Checked(self).accessControlCheckEnabled;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev(CrossOriginAccessControlCheckDisabler* self) {
    auto& disabler = Checked(self);
    disabler.vptr = &Vtable.dispatch;
    disabler.accessControlCheckEnabled = true;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(CrossOriginAccessControlCheckDisabler* self) {
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev(self);
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other) {
    auto& disabler = Checked(self);
    const auto& source = Checked(other);
    disabler.vptr = &Vtable.dispatch;
    disabler.accessControlCheckEnabled = source.accessControlCheckEnabled;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other) {
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_(self, other);
}

CrossOriginAccessControlCheckDisabler& APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_(CrossOriginAccessControlCheckDisabler* self, const CrossOriginAccessControlCheckDisabler* other) {
    auto& disabler = Checked(self);
    disabler.accessControlCheckEnabled = Checked(other).accessControlCheckEnabled;
    return disabler;
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD2Ev(CrossOriginAccessControlCheckDisabler*) {
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev(CrossOriginAccessControlCheckDisabler* self) {
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerD2Ev(self);
}

void APS5_VABI _ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev(CrossOriginAccessControlCheckDisabler* self) {
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerD1Ev(self);
    _ZdlPv_nid_postfix(self);
}

}
