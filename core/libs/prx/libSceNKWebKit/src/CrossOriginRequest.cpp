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

/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

/*
 * Derived from Source/WebCore/loader/CrossOriginAccessControl.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * Derived from Source/WebCore/platform/network/HTTPParsers.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * Derived from Source/WebCore/platform/network/ParsedContentType.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * Derived from Source/WTF/wtf/text/StringToIntegerConversion.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/include/CrossOriginRequestExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreParsersExports.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::HttpAbi::HTTPHeaderName;

std::u16string CodeUnits(const String& value) {
    if (!value.impl) return {};
    if (value.impl->length && !value.impl->data)
        throw std::invalid_argument("null WTF::StringImpl characters");
    std::u16string result;
    result.reserve(value.impl->length);
    for (unsigned i = 0; i < value.impl->length; ++i) {
        result.push_back(value.impl->hashAndFlags & 4
            ? static_cast<const unsigned char*>(value.impl->data)[i]
            : static_cast<const char16_t*>(value.impl->data)[i]);
    }
    return result;
}

bool EqualIgnoringCase(std::u16string_view value, std::u16string_view expected) {
    if (value.size() != expected.size()) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        auto c = value[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != expected[i]) return false;
    }
    return true;
}

bool IsUnsafe(char16_t c) {
    return (c < 0x20 && c != '\t') || c == 0x7f
        || std::u16string_view(u"\"():<>?@[\\]{}").find(c) != std::u16string_view::npos;
}

bool IsAlphanumeric(char16_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool IsHTTPSpace(char16_t c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool IsIntegerSpace(char16_t c) {
    return c == ' ' || (c >= '\t' && c <= '\r');
}

std::optional<long long> ParseInteger(std::u16string_view value) {
    while (!value.empty() && IsIntegerSpace(value.front())) value.remove_prefix(1);
    bool negative = false;
    if (!value.empty() && (value.front() == '-' || value.front() == '+')) {
        negative = value.front() == '-';
        value.remove_prefix(1);
    }
    if (value.empty() || value.front() < '0' || value.front() > '9') return std::nullopt;
    std::uint64_t result = 0;
    auto limit = static_cast<std::uint64_t>(std::numeric_limits<long long>::max()) + (negative ? 1 : 0);
    while (!value.empty() && value.front() >= '0' && value.front() <= '9') {
        unsigned digit = value.front() - '0';
        if (result > (limit - digit) / 10) return std::nullopt;
        result = result * 10 + digit;
        value.remove_prefix(1);
    }
    while (!value.empty() && IsIntegerSpace(value.front())) value.remove_prefix(1);
    if (!value.empty()) return std::nullopt;
    if (negative && result == limit) return std::numeric_limits<long long>::min();
    return negative ? -static_cast<long long>(result) : static_cast<long long>(result);
}

bool IsSafeRange(std::u16string_view range) {
    for (auto c : range) if (c == ' ' || c == '\t') return false;
    if (range.size() < 5 || !EqualIgnoringCase(range.substr(0, 5), u"bytes")) return false;
    range.remove_prefix(5);
    while (!range.empty() && IsHTTPSpace(range.front())) range.remove_prefix(1);
    while (!range.empty() && IsHTTPSpace(range.back())) range.remove_suffix(1);
    if (range.empty() || range.front() != '=') return false;
    range.remove_prefix(1);
    auto dash = range.find('-');
    if (dash == range.npos || !dash) return false;
    auto start = ParseInteger(range.substr(0, dash));
    if (!start) return false;
    auto endText = range.substr(dash + 1);
    long long end = -1;
    if (!endText.empty()) {
        auto parsed = ParseInteger(endText);
        if (!parsed) return false;
        end = *parsed;
    }
    return *start >= 0 && (end == -1 || end >= *start);
}

bool IsSafeHeader(HTTPHeaderName name, const String& value) {
    if (value.impl && value.impl->length > 128) return false;
    auto text = CodeUnits(value);
    switch (name) {
    case HTTPHeaderName::Accept:
        for (auto c : text) if (IsUnsafe(c)) return false;
        return true;
    case HTTPHeaderName::AcceptLanguage:
    case HTTPHeaderName::ContentLanguage:
        for (auto c : text) {
            if (!IsAlphanumeric(c) && std::u16string_view(u" *,-.;=").find(c) == std::u16string_view::npos)
                return false;
        }
        return true;
    case HTTPHeaderName::ContentType: {
        for (auto c : text) if (IsUnsafe(c)) return false;
        if (!_ZN7WebCore18isValidContentTypeERKN3WTF6StringENS_4ModeE(value, WebCore::Mode::MimeSniff))
            return false;
        std::u16string_view mime(text);
        while (!mime.empty() && IsHTTPSpace(mime.front())) mime.remove_prefix(1);
        mime = mime.substr(0, mime.find(';'));
        while (!mime.empty() && IsHTTPSpace(mime.back())) mime.remove_suffix(1);
        return EqualIgnoringCase(mime, u"application/x-www-form-urlencoded")
            || EqualIgnoringCase(mime, u"multipart/form-data") || EqualIgnoringCase(mime, u"text/plain");
    }
    case HTTPHeaderName::Range:
        return IsSafeRange(text);
    default:
        return false;
    }
}

}  // namespace

extern "C" bool APS5_VABI _ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
    const String& method, const NKWebKit::HttpAbi::HTTPHeaderMap& headers) {
    auto text = CodeUnits(method);
    if (text != u"GET" && text != u"HEAD" && text != u"POST") return false;
    if (headers.common.size && !headers.common.buffer)
        throw std::invalid_argument("null HTTPHeaderMap common storage");
    for (std::uint32_t i = 0; i < headers.common.size; ++i) {
        const auto& header = headers.common.buffer[i];
        auto index = static_cast<int>(header.key);
        if (index < 0 || static_cast<std::size_t>(index) >= NKWebKit::HttpAbi::headerNames.size())
            throw std::invalid_argument("invalid HTTPHeaderName");
        if (!IsSafeHeader(header.key, header.value)) return false;
    }
    if (headers.uncommon.size && !headers.uncommon.buffer)
        throw std::invalid_argument("null HTTPHeaderMap uncommon storage");
    if (headers.uncommon.size) return false;
    return true;
}
