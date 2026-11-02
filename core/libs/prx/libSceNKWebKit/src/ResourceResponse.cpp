/*
 * Copyright (C) 2006, 2008, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::ResourceResponseBase;

const ResourceResponseBase& Checked(const ResourceResponseBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceResponseBase");
    return *self;
}

}

extern "C" {

const NKWebKit::WtfAbi::URL& APS5_VABI _ZNK7WebCore20ResourceResponseBase3urlEv(const ResourceResponseBase* self) {
    return Checked(self).url;
}

const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase8mimeTypeEv(const ResourceResponseBase* self) {
    return Checked(self).mimeType;
}

long long APS5_VABI _ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(const ResourceResponseBase* self) {
    return Checked(self).expectedContentLength;
}

const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase16textEncodingNameEv(const ResourceResponseBase* self) {
    return Checked(self).textEncodingName;
}

const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase14httpStatusTextEv(const ResourceResponseBase* self) {
    return Checked(self).httpStatusText;
}

const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase11httpVersionEv(const ResourceResponseBase* self) {
    return Checked(self).httpVersion;
}

const NKWebKit::WebCoreAbi::ResponseHeaderStorage& APS5_VABI _ZNK7WebCore20ResourceResponseBase16httpHeaderFieldsEv(const ResourceResponseBase* self) {
    return Checked(self).httpHeaderFields;
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(const ResourceResponseBase* self) {
    const auto& url = Checked(self).url;
    if (!url.impl || !(url.bits & 1)) return false;
    NKWebKit::WtfAbi::StringView view{url.impl->data, url.impl->length, !!(url.impl->hashAndFlags & 4)};
    return _ZN3WTF22protocolIsInHTTPFamilyENS_10StringViewE(view);
}

}
