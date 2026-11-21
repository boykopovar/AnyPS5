// Derived from Source/WebCore/platform/network/ProtectionSpaceBase.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "prx/libSceNKWebKit/include/LegacyProtectionSpaceBaseExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::LegacyProtectionSpaceBase;
using NKWebKit::WtfAbi::String;

constexpr std::uint32_t ServerHTTP = 1;
constexpr std::uint32_t AuthenticationSchemeDefault = 1;

const LegacyProtectionSpaceBase& Checked(const LegacyProtectionSpaceBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::ProtectionSpaceBase");
    return *self;
}

const String& Checked(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

String NonNullEmpty(const String& string) {
    if (string.impl && string.impl->length) return string;
    return _ZN3WTF10StringImpl6createEPKhj(nullptr, 0);
}

void Construct(LegacyProtectionSpaceBase* self, const String& host, std::int32_t port, std::uint32_t serverType, const String& realm, std::uint32_t authenticationScheme) {
    if (!self) throw std::invalid_argument("null WebCore::ProtectionSpaceBase");
    String hostCopy = NonNullEmpty(host);
    String realmCopy = NonNullEmpty(realm);
    self->host.impl = std::exchange(hostCopy.impl, nullptr);
    self->port = port;
    self->serverType = serverType;
    self->realm.impl = std::exchange(realmCopy.impl, nullptr);
    self->authenticationScheme = authenticationScheme;
    self->isHashTableDeletedValue = false;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore19ProtectionSpaceBaseC1Ev(LegacyProtectionSpaceBase* self) {
    Construct(self, String(), 0, ServerHTTP, String(), AuthenticationSchemeDefault);
}

void APS5_VABI _ZN7WebCore19ProtectionSpaceBaseC2Ev(LegacyProtectionSpaceBase* self) {
    _ZN7WebCore19ProtectionSpaceBaseC1Ev(self);
}

void APS5_VABI _ZN7WebCore19ProtectionSpaceBaseC1ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE(LegacyProtectionSpaceBase* self, const String* host, std::int32_t port, std::uint32_t serverType, const String* realm, std::uint32_t authenticationScheme) {
    Construct(self, Checked(host), port, serverType, Checked(realm), authenticationScheme);
}

void APS5_VABI _ZN7WebCore19ProtectionSpaceBaseC2ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE(LegacyProtectionSpaceBase* self, const String* host, std::int32_t port, std::uint32_t serverType, const String* realm, std::uint32_t authenticationScheme) {
    _ZN7WebCore19ProtectionSpaceBaseC1ERKN3WTF6StringEiNS_25ProtectionSpaceServerTypeES4_NS_35ProtectionSpaceAuthenticationSchemeE(self, host, port, serverType, realm, authenticationScheme);
}

const String& APS5_VABI _ZNK7WebCore19ProtectionSpaceBase4hostEv(const LegacyProtectionSpaceBase* self) {
    return Checked(self).host;
}

std::int32_t APS5_VABI _ZNK7WebCore19ProtectionSpaceBase4portEv(const LegacyProtectionSpaceBase* self) {
    return Checked(self).port;
}

std::uint32_t APS5_VABI _ZNK7WebCore19ProtectionSpaceBase10serverTypeEv(const LegacyProtectionSpaceBase* self) {
    return Checked(self).serverType;
}

const String& APS5_VABI _ZNK7WebCore19ProtectionSpaceBase5realmEv(const LegacyProtectionSpaceBase* self) {
    return Checked(self).realm;
}

std::uint32_t APS5_VABI _ZNK7WebCore19ProtectionSpaceBase20authenticationSchemeEv(const LegacyProtectionSpaceBase* self) {
    return Checked(self).authenticationScheme;
}

}
