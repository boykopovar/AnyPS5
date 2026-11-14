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
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(WTF::Seconds) == 8);
static_assert(alignof(WTF::Seconds) == 8);
static_assert(sizeof(WebCore::UserGestureToken*) == 8);

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

}
