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
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/ResourceResponseBase.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/HTTPParsers.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/RFC7230.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/CacheControlExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::ResourceResponseBase;

using NKWebKit::HttpAbi::HeaderVector;
using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::AllFields;
using NKWebKit::WebCoreAbi::CommonFieldsOnly;
using NKWebKit::WebCoreAbi::ResponseInitLevel;
using NKWebKit::WebCoreAbi::ResponseOptionalTime;
using NKWebKit::WebCoreAbi::ResponseSanitizationType;
using NKWebKit::WebCoreAbi::ResponseTainting;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

template<class Response>
Response& Checked(Response* self) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceResponseBase");
    return *self;
}

template<class Response>
Response& Initialized(Response* self, ResponseInitLevel level) {
    auto& response = Checked(self);
    _ZNK7WebCore20ResourceResponseBase8lazyInitENS0_9InitLevelE(&response, level);
    return response;
}

void MarkNotNull(ResourceResponseBase& response) {
    response.stateBits = static_cast<std::uint8_t>(response.stateBits & ~NKWebKit::WebCoreAbi::responseNullMask);
}

ResponseTainting Tainting(const ResourceResponseBase& response) {
    using namespace NKWebKit::WebCoreAbi;
    return static_cast<ResponseTainting>((response.taintingSourceBits >> responseTaintingShift) & responseTaintingMask);
}

void Assign(URL& target, const URL& source) {
    if (source.impl) source.impl->refCount += 2;
    String released(target.impl);
    target = source;
}

template<class Entry>
void Validate(const HeaderVector<Entry>& vector) {
    if (vector.size > vector.capacity) throw std::invalid_argument("invalid header vector size");
    if (vector.size && !vector.buffer) throw std::invalid_argument("null header vector storage");
}

template<class Entry, class Remove>
void RemoveAllMatching(HeaderVector<Entry>& vector, Remove remove) {
    Validate(vector);
    std::vector<bool> drop;
    for (unsigned index = 0; index < vector.size; ++index) drop.push_back(remove(vector.buffer[index]));
    unsigned kept = 0;
    for (unsigned index = 0; index < vector.size; ++index) {
        if (!drop[index] && kept != index) std::construct_at(vector.buffer + kept, std::move(vector.buffer[index]));
        if (drop[index] || kept != index) std::destroy_at(vector.buffer + index);
        if (!drop[index]) ++kept;
    }
    vector.size = kept;
}

constexpr HTTPHeaderName safeRedirectionHeaders[] = {
    HTTPHeaderName::Location, HTTPHeaderName::ReferrerPolicy, HTTPHeaderName::CacheControl, HTTPHeaderName::Date,
    HTTPHeaderName::Expires, HTTPHeaderName::ETag, HTTPHeaderName::LastModified, HTTPHeaderName::Age,
    HTTPHeaderName::Pragma, HTTPHeaderName::Refresh, HTTPHeaderName::Vary,
    HTTPHeaderName::CrossOriginOpenerPolicy, HTTPHeaderName::CrossOriginOpenerPolicyReportOnly,
    HTTPHeaderName::CrossOriginEmbedderPolicy, HTTPHeaderName::CrossOriginEmbedderPolicyReportOnly,
    HTTPHeaderName::AccessControlAllowCredentials, HTTPHeaderName::AccessControlAllowHeaders,
    HTTPHeaderName::AccessControlAllowMethods, HTTPHeaderName::AccessControlAllowOrigin,
    HTTPHeaderName::AccessControlExposeHeaders, HTTPHeaderName::AccessControlMaxAge,
    HTTPHeaderName::CrossOriginResourcePolicy, HTTPHeaderName::TimingAllowOrigin,
};

constexpr HTTPHeaderName safeCrossOriginHeaders[] = {
    HTTPHeaderName::AcceptRanges, HTTPHeaderName::AccessControlAllowCredentials,
    HTTPHeaderName::AccessControlAllowHeaders, HTTPHeaderName::AccessControlAllowMethods,
    HTTPHeaderName::AccessControlAllowOrigin, HTTPHeaderName::AccessControlExposeHeaders,
    HTTPHeaderName::AccessControlMaxAge, HTTPHeaderName::AccessControlRequestHeaders,
    HTTPHeaderName::AccessControlRequestMethod, HTTPHeaderName::Age, HTTPHeaderName::CacheControl,
    HTTPHeaderName::ContentDisposition, HTTPHeaderName::ContentEncoding, HTTPHeaderName::ContentLanguage,
    HTTPHeaderName::ContentLength, HTTPHeaderName::ContentRange, HTTPHeaderName::ContentSecurityPolicy,
    HTTPHeaderName::ContentSecurityPolicyReportOnly, HTTPHeaderName::ContentType,
    HTTPHeaderName::CrossOriginResourcePolicy, HTTPHeaderName::Date, HTTPHeaderName::ETag,
    HTTPHeaderName::Expires, HTTPHeaderName::IcyMetaInt, HTTPHeaderName::IcyMetadata,
    HTTPHeaderName::LastEventID, HTTPHeaderName::LastModified, HTTPHeaderName::Link, HTTPHeaderName::Location,
    HTTPHeaderName::Pragma, HTTPHeaderName::Range, HTTPHeaderName::ReferrerPolicy, HTTPHeaderName::Refresh,
    HTTPHeaderName::ServerTiming, HTTPHeaderName::SourceMap, HTTPHeaderName::XSourceMap,
    HTTPHeaderName::TimingAllowOrigin, HTTPHeaderName::Trailer, HTTPHeaderName::Vary,
    HTTPHeaderName::XContentTypeOptions, HTTPHeaderName::XDNSPrefetchControl, HTTPHeaderName::XFrameOptions,
    HTTPHeaderName::XXSSProtection,
};

template<std::size_t count>
bool IsOneOf(const HTTPHeaderName (&names)[count], HTTPHeaderName name) {
    return std::find(std::begin(names), std::end(names), name) != std::end(names);
}

bool IsHTTPSpace(char16_t character) {
    return character == ' ' || character == '\n' || character == '\t' || character == '\r';
}

bool IsTokenCharacter(char16_t character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')
        || std::u16string_view(u"!#$%&'*+-.^_`|~").find(character) != std::u16string_view::npos;
}

std::u16string Widen(std::string_view text) {
    return {text.begin(), text.end()};
}

std::u16string Widen(const String& value);

bool EqualIgnoringCase(const std::u16string& a, const std::u16string& b) {
    const auto fold = [](char16_t character) { return character >= 'A' && character <= 'Z' ? char16_t(character + ('a' - 'A')) : character; };
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [&](char16_t x, char16_t y) { return fold(x) == fold(y); });
}

bool AddToken(const std::u16string& text, std::size_t start, std::size_t end, std::vector<std::u16string>& list) {
    while (start <= end && IsHTTPSpace(text[start])) ++start;
    if (start > end) return true;
    while (end && IsHTTPSpace(text[end])) --end;
    const auto token = text.substr(start, end - start + 1);
    if (!std::all_of(token.begin(), token.end(), IsTokenCharacter)) return false;
    list.push_back(token);
    return true;
}

std::vector<std::u16string> AccessControlAllowList(const std::u16string& text) {
    std::vector<std::u16string> list;
    std::size_t start = 0;
    std::size_t end;
    while ((end = text.find(u',', start)) != text.npos) {
        if (start != end && !AddToken(text, start, end - 1, list)) return {};
        start = end + 1;
    }
    if (start != text.size() && !AddToken(text, start, text.size() - 1, list)) return {};
    return list;
}

bool Contains(const std::vector<std::u16string>& list, const std::u16string& name) {
    return std::any_of(list.begin(), list.end(), [&](const auto& item) { return EqualIgnoringCase(item, name); });
}

bool IsEmpty(const String& value) {
    return !value.impl || !value.impl->length;
}

String Header(const ResourceResponseBase* self, HTTPHeaderName name) {
    if (static_cast<unsigned>(name) >= NKWebKit::HttpAbi::headerNames.size())
        throw std::out_of_range("invalid HTTPHeaderName");
    return _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(&Checked(self).httpHeaderFields, name);
}

NKWebKit::HttpAbi::CacheControlDirectives CacheControl(const ResourceResponseBase* self) {
    return _ZN7WebCore27parseCacheControlDirectivesERKNS_13HTTPHeaderMapE(Checked(self).httpHeaderFields);
}

ResponseOptionalTime OptionalTime(double seconds) {
    return std::isnan(seconds) ? ResponseOptionalTime{} : ResponseOptionalTime{true, seconds};
}

ResponseOptionalTime HeaderDate(const ResourceResponseBase* self, HTTPHeaderName name) {
    const auto value = Header(self, name);
    if (IsEmpty(value)) return {};
    const auto utf8 = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&value, 0);
    if (!utf8.buffer) return {};
    const auto milliseconds = _ZN3WTF37parseDateFromNullTerminatedCharactersEPKc(
        reinterpret_cast<const char*>(utf8.buffer + 1));
    if (!std::isfinite(milliseconds)) return {};
    return {true, milliseconds / 1000};
}

template<class Operation>
auto VisitString(const StringImpl* impl, Operation operation) {
    if (!impl || !impl->length) return operation(std::string_view{});
    if (!impl->data) throw std::invalid_argument("null response string storage");
    if (impl->hashAndFlags & 4) return operation(std::string_view(static_cast<const char*>(impl->data), impl->length));
    return operation(std::u16string_view(static_cast<const char16_t*>(impl->data), impl->length));
}

bool IsSpace(char16_t value) {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f';
}

template<class Char>
std::basic_string_view<Char> Trim(std::basic_string_view<Char> value) {
    while (!value.empty() && IsSpace(value.front())) value.remove_prefix(1);
    while (!value.empty() && IsSpace(value.back())) value.remove_suffix(1);
    return value;
}

template<class Char>
bool MatchesAscii(std::basic_string_view<Char> value, std::string_view expected, bool ignoreCase = false) {
    if (value.size() != expected.size()) return false;
    for (std::size_t index = 0; index < value.size(); ++index) {
        auto character = value[index];
        if (ignoreCase && character >= 'A' && character <= 'Z') character += 'a' - 'A';
        if (character != expected[index]) return false;
    }
    return true;
}

std::u16string Widen(const String& value) {
    return VisitString(value.impl, [](auto view) {
        std::u16string text;
        for (auto character : view) text.push_back(static_cast<char16_t>(static_cast<std::make_unsigned_t<decltype(character)>>(character)));
        return text;
    });
}

bool IsAttachment(const String& disposition) {
    return VisitString(disposition.impl, [](auto value) {
        return MatchesAscii(Trim(value.substr(0, value.find(';'))), "attachment", true);
    });
}

template<class Char>
String DecodeFilename(std::basic_string_view<Char> value) {
    if (value.size() > 0x7fffffff) throw std::length_error("response string too long");
    if (value.empty()) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    if constexpr (sizeof(Char) == 1)
        return _ZN3WTF6String26fromUTF8WithLatin1FallbackEPKhm(reinterpret_cast<const unsigned char*>(value.data()), value.size());
    else
        return _ZN3WTF10StringImpl6createEPKDsj(value.data(), static_cast<unsigned>(value.size()));
}

template<class Char>
std::optional<std::basic_string_view<Char>> FilenameParameter(std::basic_string_view<Char> remaining) {
    while (!remaining.empty()) {
        const auto semicolon = remaining.find(';');
        const auto field = remaining.substr(0, semicolon);
        const auto equal = field.find('=');
        if (equal != field.npos && MatchesAscii(Trim(field.substr(0, equal)), "filename")) {
            auto value = Trim(field.substr(equal + 1));
            if (value.size() > 1 && value.front() == '"') value = value.substr(1, value.size() - 2);
            return value;
        }
        if (semicolon == remaining.npos) break;
        remaining.remove_prefix(semicolon + 1);
    }
    return std::nullopt;
}

template<class Char>
String ParseFilename(std::basic_string_view<Char> disposition) {
    const auto filename = FilenameParameter(disposition);
    return filename ? DecodeFilename(*filename) : String();
}

String Filename(const String& disposition) {
    return VisitString(disposition.impl, [](auto value) { return ParseFilename(value); });
}

template<class Char>
String SanitizeFilename(std::basic_string_view<Char> value) {
    constexpr std::string_view prefix = "attachment; filename=\"";
    std::basic_string<Char> escaped(prefix.begin(), prefix.end());
    for (auto character : value) {
        if (character == '\\' || character == '"') escaped.push_back('\\');
        escaped.push_back(character);
    }
    escaped.push_back('"');
    if (escaped.size() > 0x7fffffff) throw std::length_error("content disposition too long");
    return ParseFilename(std::basic_string_view<Char>(escaped));
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

void APS5_VABI _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(ResourceResponseBase* self, HTTPHeaderName name, const String& value) {
    UpdateCommonHeader_nid_no_patch(Checked(self).httpHeaderFields, name, value, true);
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldERKN3WTF6StringES4_(ResourceResponseBase* self, const String& name, const String& value) {
    _ZN7WebCore13HTTPHeaderMap3addERKN3WTF6StringES4_(&Checked(self).httpHeaderFields, name, value);
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(ResourceResponseBase* self, HTTPHeaderName name, const String& value) {
    UpdateCommonHeader_nid_no_patch(Checked(self).httpHeaderFields, name, value, false);
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldERKN3WTF6StringES4_(ResourceResponseBase* self, const String& name, const String& value) {
    _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&Checked(self).httpHeaderFields, name, value);
}

String APS5_VABI _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(const ResourceResponseBase* self, HTTPHeaderName name) {
    return Header(self, name);
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase8isHTTP09Ev(const ResourceResponseBase* self) {
    return VisitString(Checked(self).httpVersion.impl, [](auto value) { return MatchesAscii(value.substr(0, 8), "HTTP/0.9"); });
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase12isAttachmentEv(const ResourceResponseBase* self) {
    return IsAttachment(Header(self, HTTPHeaderName::ContentDisposition));
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase24isAttachmentWithFilenameEv(const ResourceResponseBase* self) {
    const auto disposition = Header(self, HTTPHeaderName::ContentDisposition);
    return IsAttachment(disposition)
        && VisitString(disposition.impl, [](auto value) { return FilenameParameter(value).has_value(); });
}

String APS5_VABI _ZNK7WebCore20ResourceResponseBase17suggestedFilenameEv(const ResourceResponseBase* self) {
    return Filename(Header(self, HTTPHeaderName::ContentDisposition));
}

String APS5_VABI _ZN7WebCore20ResourceResponseBase25sanitizeSuggestedFilenameERKN3WTF6StringE(const String& filename) {
    if (IsEmpty(filename)) return filename;
    return VisitString(filename.impl, [](auto value) { return SanitizeFilename(value); });
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase23hasCacheValidatorFieldsEv(const ResourceResponseBase* self) {
    return !IsEmpty(Header(self, HTTPHeaderName::LastModified)) || !IsEmpty(Header(self, HTTPHeaderName::ETag));
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoCacheEv(const ResourceResponseBase* self) {
    return CacheControl(self).noCache;
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoStoreEv(const ResourceResponseBase* self) {
    return CacheControl(self).noStore;
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase29cacheControlContainsImmutableEv(const ResourceResponseBase* self) {
    return CacheControl(self).immutable;
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase34cacheControlContainsMustRevalidateEv(const ResourceResponseBase* self) {
    return CacheControl(self).mustRevalidate;
}

ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase18cacheControlMaxAgeEv(const ResourceResponseBase* self) {
    return OptionalTime(CacheControl(self).maxAge);
}

ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase32cacheControlStaleWhileRevalidateEv(const ResourceResponseBase* self) {
    return OptionalTime(CacheControl(self).staleWhileRevalidate);
}

ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase4dateEv(const ResourceResponseBase* self) {
    return HeaderDate(self, HTTPHeaderName::Date);
}

ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase7expiresEv(const ResourceResponseBase* self) {
    return HeaderDate(self, HTTPHeaderName::Expires);
}

ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase12lastModifiedEv(const ResourceResponseBase* self) {
    return HeaderDate(self, HTTPHeaderName::LastModified);
}

ResponseOptionalTime APS5_VABI _ZNK7WebCore20ResourceResponseBase3ageEv(const ResourceResponseBase* self) {
    const auto age = Header(self, HTTPHeaderName::Age);
    return VisitString(age.impl, [](auto value) {
        if (value.empty()) return ResponseOptionalTime{};
        bool ok;
        double seconds;
        if constexpr (sizeof(typename decltype(value)::value_type) == 1)
            seconds = _ZN3WTF18charactersToDoubleEPKhmPb(reinterpret_cast<const unsigned char*>(value.data()), value.size(), &ok);
        else
            seconds = _ZN3WTF18charactersToDoubleEPKDsmPb(value.data(), value.size(), &ok);
        return ok ? OptionalTime(seconds) : ResponseOptionalTime{};
    });
}

void APS5_VABI _ZN7WebCore20ResourceResponseBaseC1Ev(ResourceResponseBase* self) {
    std::construct_at(&Checked(self));
}

void APS5_VABI _ZN7WebCore20ResourceResponseBaseC2Ev(ResourceResponseBase* self) {
    std::construct_at(&Checked(self));
}

int APS5_VABI _ZNK7WebCore20ResourceResponseBase14httpStatusCodeEv(const ResourceResponseBase* self) {
    return Initialized(self, CommonFieldsOnly).httpStatusCode;
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase17setHTTPStatusCodeEi(ResourceResponseBase* self, int statusCode) {
    auto& response = Initialized(self, CommonFieldsOnly);
    response.httpStatusCode = static_cast<short>(statusCode);
    MarkNotNull(response);
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase12isSuccessfulEv(const ResourceResponseBase* self) {
    const int code = _ZNK7WebCore20ResourceResponseBase14httpStatusCodeEv(self);
    return code >= 200 && code < 300;
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase13isRedirectionEv(const ResourceResponseBase* self) {
    const int code = Checked(self).httpStatusCode;
    return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase6setURLERKN3WTF3URLE(ResourceResponseBase* self, const URL& url) {
    auto& response = Initialized(self, CommonFieldsOnly);
    MarkNotNull(response);
    Assign(response.url, url);
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase24setExpectedContentLengthEx(ResourceResponseBase* self, long long expectedContentLength) {
    auto& response = Initialized(self, CommonFieldsOnly);
    MarkNotNull(response);
    response.expectedContentLength = expectedContentLength;
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase7setTypeENS0_4TypeE(ResourceResponseBase* self, NKWebKit::WebCoreAbi::ResponseType type) {
    using namespace NKWebKit::WebCoreAbi;
    auto& response = Checked(self);
    MarkNotNull(response);
    response.typeBits = static_cast<std::uint8_t>((response.typeBits & ~responseTypeMask) | (static_cast<unsigned>(type) & responseTypeMask));
}

NKWebKit::WebCoreAbi::ResponseSource APS5_VABI _ZNK7WebCore20ResourceResponseBase6sourceEv(const ResourceResponseBase* self) {
    using namespace NKWebKit::WebCoreAbi;
    const auto& response = Initialized(self, AllFields);
    return static_cast<ResponseSource>((response.taintingSourceBits >> responseSourceShift) & responseSourceMask);
}

void APS5_VABI _ZNK7WebCore20ResourceResponseBase22includeCertificateInfoEv(const ResourceResponseBase* self) {
    auto& certificate = const_cast<ResourceResponseBase&>(Checked(self)).certificateInfo;
    if (certificate.hasValue) return;
    certificate = {{}, true};
}

void APS5_VABI _ZNK7WebCore20ResourceResponseBase8lazyInitENS0_9InitLevelE(const ResourceResponseBase* self, ResponseInitLevel) {
    Checked(self);
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase24sanitizeHTTPHeaderFieldsENS0_16SanitizationTypeE(ResourceResponseBase* self, ResponseSanitizationType type) {
    auto& response = Initialized(self, AllFields);
    if (static_cast<unsigned>(type) > static_cast<unsigned>(ResponseSanitizationType::CrossOriginSafe))
        throw std::invalid_argument("invalid SanitizationType");
    auto& headers = response.httpHeaderFields;
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&headers, HTTPHeaderName::SetCookie);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&headers, HTTPHeaderName::SetCookie2);
    if (type == ResponseSanitizationType::RemoveCookies) return;
    const auto removeAll = [](const auto&) { return true; };
    if (type == ResponseSanitizationType::Redirection) {
        RemoveAllMatching(headers.common, [](const auto& header) { return !IsOneOf(safeRedirectionHeaders, header.key); });
        RemoveAllMatching(headers.uncommon, removeAll);
        return;
    }
    switch (Tainting(response)) {
    case ResponseTainting::Basic:
        return;
    case ResponseTainting::Cors: {
        const auto allowed = AccessControlAllowList(Widen(Header(&response, HTTPHeaderName::AccessControlExposeHeaders)));
        if (Contains(allowed, u"*")) return;
        RemoveAllMatching(headers.common, [&](const auto& header) {
            return !IsOneOf(safeCrossOriginHeaders, header.key)
                && !Contains(allowed, Widen(std::string_view(_ZN7WebCore20httpHeaderNameStringENS_14HTTPHeaderNameE(header.key).characters)));
        });
        RemoveAllMatching(headers.uncommon, [&](const auto& header) { return !Contains(allowed, Widen(header.key)); });
        return;
    }
    case ResponseTainting::Opaque:
    case ResponseTainting::Opaqueredirect:
        RemoveAllMatching(headers.common, [](const auto& header) { return !IsOneOf(safeCrossOriginHeaders, header.key); });
        RemoveAllMatching(headers.uncommon, removeAll);
        return;
    }
}

}
