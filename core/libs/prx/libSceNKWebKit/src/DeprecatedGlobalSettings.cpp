// Derived from Source/WebCore/page/DeprecatedGlobalSettings.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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

// Derived from Source/WebCore/page/DeprecatedGlobalSettings.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"
#include "prx/libSceNKWebKit/src/DeprecatedGlobalSettings.hpp"

namespace NKWebKit {

DeprecatedGlobalSettings& SharedDeprecatedGlobalSettings() {
    static DeprecatedGlobalSettings settings;
    return settings;
}

}  // namespace NKWebKit

extern "C" {

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings24setMockScrollbarsEnabledEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().mockScrollbarsEnabled = flag;
}

bool APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings21mockScrollbarsEnabledEv() {
    return NKWebKit::SharedDeprecatedGlobalSettings().mockScrollbarsEnabled;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings24setUsesOverlayScrollbarsEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().usesOverlayScrollbars = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings25setUsesMockScrollAnimatorEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().mockScrollAnimatorEnabled = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings45setShouldRespectPriorityInCSSAttributeSettersEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().shouldRespectPriorityInCSSAttributeSetters = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings38setLowPowerVideoAudioBufferSizeEnabledEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().lowPowerVideoAudioBufferSizeEnabled = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings32setResourceLoadStatisticsEnabledEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().resourceLoadStatisticsEnabled = flag;
}

void APS5_VABI _ZN7WebCore24DeprecatedGlobalSettings26setAllowsAnySSLCertificateEb(bool flag) {
    NKWebKit::SharedDeprecatedGlobalSettings().allowsAnySSLCertificate = flag;
}

}
