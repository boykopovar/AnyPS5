/*
 * Copyright (C) 2005, 2006, 2008, 2011, 2014 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/history/HistoryItem.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/HistoryItemURLExports.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::HistoryItem;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

const HistoryItem& Checked(const HistoryItem* self) {
    if (!self) throw std::invalid_argument("null WebCore::HistoryItem");
    return *self;
}

URL ParseWithoutBase(const String& string) {
    const URL base{};
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &string, nullptr);
    return url;
}

}

extern "C" {

URL APS5_VABI _ZNK7WebCore11HistoryItem3urlEv(const HistoryItem* self) {
    return ParseWithoutBase(Checked(self).urlString);
}

URL APS5_VABI _ZNK7WebCore11HistoryItem11originalURLEv(const HistoryItem* self) {
    return ParseWithoutBase(Checked(self).originalURLString);
}

}
