// Derived from Source/JavaScriptCore/runtime/PropertyDescriptor.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

// Derived from Source/JavaScriptCore/runtime/PropertyDescriptor.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/PropertyDescriptorExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

using NKWebKit::JscAbi::PropertyDescriptor;

namespace {

PropertyDescriptor Load(const PropertyDescriptor* self, const char* name) {
    if (self == nullptr) throw std::invalid_argument(std::string(name) + ": null object");
    PropertyDescriptor descriptor;
    if (!GuestCopyIn_nid_no_patch(&descriptor, self, sizeof(descriptor))) throw std::invalid_argument(std::string(name) + ": unreadable object");
    return descriptor;
}

void Store(PropertyDescriptor* self, const PropertyDescriptor& descriptor, const char* name) {
    if (!GuestCopyOut_nid_no_patch(self, &descriptor, sizeof(descriptor))) throw std::invalid_argument(std::string(name) + ": unwritable object");
}

void SetAttribute(PropertyDescriptor* self, std::uint32_t attribute, bool clear, std::uint32_t present, const char* name) {
    auto descriptor = Load(self, name);
    if (clear)
        descriptor.attributes &= ~attribute;
    else
        descriptor.attributes |= attribute;
    descriptor.seenAttributes |= present;
    Store(self, descriptor, name);
}

void SetAccessor(PropertyDescriptor* self, std::uint64_t PropertyDescriptor::*accessor, std::uint64_t value, const char* name) {
    auto descriptor = Load(self, name);
    descriptor.*accessor = value;
    descriptor.attributes |= NKWebKit::JscAbi::Accessor;
    descriptor.attributes &= ~static_cast<std::uint32_t>(NKWebKit::JscAbi::ReadOnly);
    Store(self, descriptor, name);
}

}

extern "C" {

bool APS5_VABI _ZNK3JSC18PropertyDescriptor8writableEv(const PropertyDescriptor* self) {
    return !(Load(self, __func__).attributes & NKWebKit::JscAbi::ReadOnly);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor10enumerableEv(const PropertyDescriptor* self) {
    return !(Load(self, __func__).attributes & NKWebKit::JscAbi::DontEnum);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor12configurableEv(const PropertyDescriptor* self) {
    return !(Load(self, __func__).attributes & NKWebKit::JscAbi::DontDelete);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor16isDataDescriptorEv(const PropertyDescriptor* self) {
    const auto descriptor = Load(self, __func__);
    return descriptor.value != 0 || (descriptor.seenAttributes & NKWebKit::JscAbi::WritablePresent);
}

bool APS5_VABI _ZNK3JSC18PropertyDescriptor20isAccessorDescriptorEv(const PropertyDescriptor* self) {
    const auto descriptor = Load(self, __func__);
    return descriptor.getter != 0 || descriptor.setter != 0;
}

void APS5_VABI _ZN3JSC18PropertyDescriptor11setWritableEb(PropertyDescriptor* self, bool writable) {
    SetAttribute(self, NKWebKit::JscAbi::ReadOnly, writable, NKWebKit::JscAbi::WritablePresent, __func__);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor13setEnumerableEb(PropertyDescriptor* self, bool enumerable) {
    SetAttribute(self, NKWebKit::JscAbi::DontEnum, enumerable, NKWebKit::JscAbi::EnumerablePresent, __func__);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor15setConfigurableEb(PropertyDescriptor* self, bool configurable) {
    SetAttribute(self, NKWebKit::JscAbi::DontDelete, configurable, NKWebKit::JscAbi::ConfigurablePresent, __func__);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor12setUndefinedEv(PropertyDescriptor* self) {
    auto descriptor = Load(self, __func__);
    descriptor.value = NKWebKit::JscAbi::EncodedJSUndefined;
    descriptor.attributes = NKWebKit::JscAbi::ReadOnly | NKWebKit::JscAbi::DontDelete | NKWebKit::JscAbi::DontEnum;
    Store(self, descriptor, __func__);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor9setGetterENS_7JSValueE(PropertyDescriptor* self, std::uint64_t getter) {
    SetAccessor(self, &PropertyDescriptor::getter, getter, __func__);
}

void APS5_VABI _ZN3JSC18PropertyDescriptor9setSetterENS_7JSValueE(PropertyDescriptor* self, std::uint64_t setter) {
    SetAccessor(self, &PropertyDescriptor::setter, setter, __func__);
}

}
