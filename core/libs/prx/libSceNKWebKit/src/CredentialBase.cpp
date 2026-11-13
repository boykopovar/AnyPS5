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

#include "prx/libSceNKWebKit/include/CredentialBaseExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::CredentialBase;
using NKWebKit::WtfAbi::String;

const CredentialBase& Checked(const CredentialBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::CredentialBase");
    return *self;
}

const String& Checked(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

bool IsEmpty(const String& string) {
    return !string.impl || !string.impl->length;
}

String NonNullEmpty(const String& string) {
    if (!IsEmpty(string)) return string;
    return _ZN3WTF10StringImpl6createEPKhj(nullptr, 0);
}

void Construct(CredentialBase* self, const String& user, const String& password, std::uint32_t persistence) {
    if (!self) throw std::invalid_argument("null WebCore::CredentialBase");
    String userCopy = NonNullEmpty(user);
    String passwordCopy = NonNullEmpty(password);
    self->user.impl = std::exchange(userCopy.impl, nullptr);
    self->password.impl = std::exchange(passwordCopy.impl, nullptr);
    self->persistence = static_cast<NKWebKit::WebCoreAbi::CredentialPersistence>(persistence);
}

}

extern "C" {

bool APS5_VABI _ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(const CredentialBase* a, const CredentialBase* b) {
    const auto& left = Checked(a);
    const auto& right = Checked(b);
    if (left.persistence != right.persistence) return false;
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(left.user.impl, right.user.impl)) return false;
    return _ZN3WTF5equalEPKNS_10StringImplES2_(left.password.impl, right.password.impl);
}

void APS5_VABI _ZN7WebCore14CredentialBaseC1ERKN3WTF6StringES4_NS_21CredentialPersistenceE(CredentialBase* self, const String* user, const String* password, std::uint32_t persistence) {
    Construct(self, Checked(user), Checked(password), persistence);
}

void APS5_VABI _ZN7WebCore14CredentialBaseC1Ev(CredentialBase* self) {
    Construct(self, String(), String(), NKWebKit::WebCoreAbi::CredentialPersistenceNone);
}

void APS5_VABI _ZN7WebCore14CredentialBaseC2ERKN3WTF6StringES4_NS_21CredentialPersistenceE(CredentialBase* self, const String* user, const String* password, std::uint32_t persistence) {
    Construct(self, Checked(user), Checked(password), persistence);
}

void APS5_VABI _ZN7WebCore14CredentialBaseC2Ev(CredentialBase* self) {
    Construct(self, String(), String(), NKWebKit::WebCoreAbi::CredentialPersistenceNone);
}

bool APS5_VABI _ZNK7WebCore14CredentialBase11hasPasswordEv(const CredentialBase* self) {
    return !IsEmpty(Checked(self).password);
}

std::uint32_t APS5_VABI _ZNK7WebCore14CredentialBase11persistenceEv(const CredentialBase* self) {
    return Checked(self).persistence;
}

const String& APS5_VABI _ZNK7WebCore14CredentialBase4userEv(const CredentialBase* self) {
    return Checked(self).user;
}

bool APS5_VABI _ZNK7WebCore14CredentialBase7isEmptyEv(const CredentialBase* self) {
    const auto& credential = Checked(self);
    return IsEmpty(credential.user) && IsEmpty(credential.password);
}

const String& APS5_VABI _ZNK7WebCore14CredentialBase8passwordEv(const CredentialBase* self) {
    return Checked(self).password;
}

}
