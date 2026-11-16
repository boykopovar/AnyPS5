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
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/CacheControlExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WebCoreAbi::CertificateInfo;
using NKWebKit::WebCoreAbi::ResourceResponse;
using NKWebKit::WebCoreAbi::ResourceResponseBase;

using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResponseOptionalTime;
using NKWebKit::WebCoreAbi::StringSlot;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

constexpr unsigned char stateNullBit = 1;
constexpr unsigned atomStringFlag = 1u << 4;

template<class Response>
Response& Checked(Response* self, const char* name = "WebCore::ResourceResponseBase") {
    if (!self) throw std::invalid_argument(std::string("null ") + name);
    return *self;
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

void Retain(StringImpl* impl) {
    if (impl) impl->refCount += 2;
}

std::atomic<unsigned>& MetricsBoxRefCount(void* box) {
    return *reinterpret_cast<std::atomic<unsigned>*>(box);
}

bool MetricsBoxHasPayload(void* box) {
    return *reinterpret_cast<void**>(static_cast<unsigned char*>(box) + 8) != nullptr;
}

void RetainMetricsBox(void* box) {
    if (box) ++MetricsBoxRefCount(box);
}

void ReleaseMetricsBox(void* box) {
    if (!box) return;
    if (MetricsBoxRefCount(box).load() == 1 && MetricsBoxHasPayload(box)) NotImplemented_nid_no_patch(__func__);
    if (--MetricsBoxRefCount(box) == 0) _ZN3WTF8fastFreeEPv(box);
}

void ConstructDefault(ResourceResponseBase& self) {
    new (&self) ResourceResponseBase{};
}

void ConstructFields(ResourceResponseBase& self, const NKWebKit::WtfAbi::URL& url, const String& mimeType, long long expectedContentLength, const String& textEncodingName) {
    if (mimeType.impl && !(mimeType.impl->hashAndFlags & atomStringFlag)) NotImplemented_nid_no_patch(__func__);
    if (textEncodingName.impl && !(textEncodingName.impl->hashAndFlags & atomStringFlag)) NotImplemented_nid_no_patch(__func__);
    ConstructDefault(self);
    self.url = url;
    Retain(self.url.impl);
    self.mimeType.impl = mimeType.impl;
    Retain(self.mimeType.impl);
    self.expectedContentLength = expectedContentLength;
    self.textEncodingName.impl = textEncodingName.impl;
    Retain(self.textEncodingName.impl);
    self.certificateInfo.engaged = true;
    self.stateBits = 0;
}

template<class TVector>
void ValidateVector(const TVector& vector) {
    if (vector.size > vector.capacity) throw std::invalid_argument("invalid response vector size");
    if (vector.size && !vector.buffer) throw std::invalid_argument("null response vector storage");
}

template<class TVector>
void ConstructVector(TVector& dst, const TVector& src) {
    using Entry = std::remove_reference_t<decltype(*dst.buffer)>;
    ValidateVector(src);
    const std::uint64_t bytes = std::uint64_t(src.size) * sizeof(Entry);
    if (bytes > std::numeric_limits<unsigned>::max()) throw std::length_error("response vector too long");
    if (!src.size) {
        dst.buffer = nullptr;
        dst.capacity = 0;
        dst.size = 0;
        return;
    }
    dst.buffer = static_cast<Entry*>(_ZN3WTF10fastMallocEm(static_cast<std::size_t>(bytes)));
    dst.capacity = src.size;
    dst.size = src.size;
    for (unsigned i = 0; i < src.size; ++i) new (dst.buffer + i) Entry(src.buffer[i]);
}

template<class TVector>
void DestroyVector(TVector& vector) {
    if (vector.buffer) {
        std::destroy_n(vector.buffer, vector.size);
        _ZN3WTF8fastFreeEPv(vector.buffer);
    }
}

template<class TVector>
void AssignVector(TVector& dst, const TVector& src);

template<class Entry>
void AssignEntry(Entry& dst, const Entry& src) {
    dst = src;
}

template<class T>
void AssignEntry(NKWebKit::WtfAbi::Vector<T>& dst, const NKWebKit::WtfAbi::Vector<T>& src) {
    AssignVector(dst, src);
}

template<class TVector>
void AssignVector(TVector& dst, const TVector& src) {
    using Entry = std::remove_reference_t<decltype(*dst.buffer)>;
    if (&dst == &src) return;
    ValidateVector(dst);
    ValidateVector(src);
    if (dst.size > src.size) {
        std::destroy_n(dst.buffer + src.size, dst.size - src.size);
        for (unsigned i = 0; i < src.size; ++i) AssignEntry(dst.buffer[i], src.buffer[i]);
        dst.size = src.size;
        return;
    }
    if (src.size > dst.capacity) {
        const std::uint64_t bytes = std::uint64_t(src.size) * sizeof(Entry);
        if (bytes > std::numeric_limits<unsigned>::max()) throw std::length_error("response vector too long");
        DestroyVector(dst);
        dst.buffer = static_cast<Entry*>(_ZN3WTF10fastMallocEm(static_cast<std::size_t>(bytes)));
        dst.capacity = src.size;
        for (unsigned i = 0; i < src.size; ++i) new (dst.buffer + i) Entry(src.buffer[i]);
        dst.size = src.size;
        return;
    }
    for (unsigned i = 0; i < dst.size; ++i) AssignEntry(dst.buffer[i], src.buffer[i]);
    for (unsigned i = dst.size; i < src.size; ++i) new (dst.buffer + i) Entry(src.buffer[i]);
    dst.size = src.size;
}

void AssignURL(NKWebKit::WtfAbi::URL& dst, const NKWebKit::WtfAbi::URL& src) {
    StringImpl* previous = dst.impl;
    dst = src;
    Retain(dst.impl);
    NKWebKit::WtfAbi::Release(previous);
}

void AssignSlot(StringSlot& dst, const StringSlot& src) {
    if (src.impl && !(src.impl->hashAndFlags & atomStringFlag)) NotImplemented_nid_no_patch(__func__);
    StringImpl* previous = dst.impl;
    dst.impl = src.impl;
    Retain(dst.impl);
    NKWebKit::WtfAbi::Release(previous);
}

void AssignCertificateInfo(NKWebKit::WebCoreAbi::OptionalCertificateInfo& dst, const NKWebKit::WebCoreAbi::OptionalCertificateInfo& src) {
    if (dst.engaged && src.engaged) {
        dst.value.verificationError = src.value.verificationError;
        AssignVector(dst.value.certificateChain, src.value.certificateChain);
        AssignVector(dst.value.privateKey, src.value.privateKey);
        dst.value.privateKeyPassword = src.value.privateKeyPassword;
        return;
    }
    if (dst.engaged) {
        dst.value.~CertificateInfo();
        dst.engaged = false;
        return;
    }
    if (src.engaged) {
        new (&dst.value) CertificateInfo(src.value);
        dst.engaged = true;
    }
}

void ConstructCopy(ResourceResponseBase& dst, const ResourceResponseBase& src) {
    ValidateVector(src.httpHeaderFields.common);
    ValidateVector(src.httpHeaderFields.uncommon);
    ConstructDefault(dst);
    AssignURL(dst.url, src.url);
    AssignSlot(dst.mimeType, src.mimeType);
    dst.expectedContentLength = src.expectedContentLength;
    AssignSlot(dst.textEncodingName, src.textEncodingName);
    AssignSlot(dst.httpStatusText, src.httpStatusText);
    AssignSlot(dst.httpVersion, src.httpVersion);
    ConstructVector(dst.httpHeaderFields.common, src.httpHeaderFields.common);
    ConstructVector(dst.httpHeaderFields.uncommon, src.httpHeaderFields.uncommon);
    dst.networkLoadMetrics = src.networkLoadMetrics;
    RetainMetricsBox(dst.networkLoadMetrics);
    if (src.certificateInfo.engaged) {
        dst.certificateInfo.value.~CertificateInfo();
        new (&dst.certificateInfo.value) CertificateInfo(src.certificateInfo.value);
        dst.certificateInfo.engaged = true;
    }
    dst.httpStatusCode = src.httpStatusCode;
    dst.stateBits = src.stateBits;
    dst.age = src.age;
    dst.date = src.date;
    dst.expires = src.expires;
    dst.lastModified = src.lastModified;
    dst.contentRange = src.contentRange;
    dst.cacheControlDirectives = src.cacheControlDirectives;
    dst.parsedBits = src.parsedBits;
    dst.taintBits = src.taintBits;
    dst.typeBits = src.typeBits;
}

ResourceResponseBase& CopyAssign(ResourceResponseBase& dst, const ResourceResponseBase& src) {
    AssignURL(dst.url, src.url);
    AssignSlot(dst.mimeType, src.mimeType);
    dst.expectedContentLength = src.expectedContentLength;
    AssignSlot(dst.textEncodingName, src.textEncodingName);
    AssignSlot(dst.httpStatusText, src.httpStatusText);
    AssignSlot(dst.httpVersion, src.httpVersion);
    AssignVector(dst.httpHeaderFields.common, src.httpHeaderFields.common);
    AssignVector(dst.httpHeaderFields.uncommon, src.httpHeaderFields.uncommon);
    if (dst.networkLoadMetrics != src.networkLoadMetrics) {
        ReleaseMetricsBox(dst.networkLoadMetrics);
        RetainMetricsBox(src.networkLoadMetrics);
        dst.networkLoadMetrics = src.networkLoadMetrics;
    }
    AssignCertificateInfo(dst.certificateInfo, src.certificateInfo);
    dst.httpStatusCode = src.httpStatusCode;
    dst.stateBits = src.stateBits;
    dst.age = src.age;
    dst.date = src.date;
    dst.expires = src.expires;
    dst.lastModified = src.lastModified;
    dst.contentRange = src.contentRange;
    dst.cacheControlDirectives = src.cacheControlDirectives;
    dst.parsedBits = src.parsedBits;
    dst.taintBits = src.taintBits;
    dst.typeBits = src.typeBits;
    return dst;
}

void Destroy(ResourceResponseBase& self) {
    ReleaseMetricsBox(self.networkLoadMetrics);
    NKWebKit::WtfAbi::Release(self.url.impl);
    NKWebKit::WtfAbi::Release(self.mimeType.impl);
    NKWebKit::WtfAbi::Release(self.textEncodingName.impl);
    NKWebKit::WtfAbi::Release(self.httpStatusText.impl);
    NKWebKit::WtfAbi::Release(self.httpVersion.impl);
    DestroyVector(self.httpHeaderFields.common);
    DestroyVector(self.httpHeaderFields.uncommon);
    if (self.certificateInfo.engaged) self.certificateInfo.value.~CertificateInfo();
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

void APS5_VABI _ZN7WebCore20ResourceResponseBase11setMimeTypeERKN3WTF6StringE(ResourceResponseBase* self, const String& mimeType) {
    auto& response = Checked(self);
    AssignSlot(response.mimeType, StringSlot{mimeType.impl});
    response.stateBits &= ~stateNullBit;
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase19setTextEncodingNameERKN3WTF6StringE(ResourceResponseBase* self, const String& textEncodingName) {
    auto& response = Checked(self);
    AssignSlot(response.textEncodingName, StringSlot{textEncodingName.impl});
    response.stateBits &= ~stateNullBit;
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase17setHTTPStatusTextERKN3WTF6StringE(ResourceResponseBase* self, const String& statusText) {
    AssignSlot(Checked(self).httpStatusText, StringSlot{statusText.impl});
}

void APS5_VABI _ZN7WebCore20ResourceResponseBase14setHTTPVersionERKN3WTF6StringE(ResourceResponseBase* self, const String& httpVersion) {
    AssignSlot(Checked(self).httpVersion, StringSlot{httpVersion.impl});
}

void APS5_VABI _ZN7WebCore20ResourceResponseBaseC1ERKN3WTF3URLERKNS1_6StringExS7_(ResourceResponseBase* self, const NKWebKit::WtfAbi::URL& url, const String& mimeType, long long expectedContentLength, const String& textEncodingName) {
    ConstructFields(Checked(self), url, mimeType, expectedContentLength, textEncodingName);
}

void APS5_VABI _ZN7WebCore20ResourceResponseBaseC2ERKN3WTF3URLERKNS1_6StringExS7_(ResourceResponseBase* self, const NKWebKit::WtfAbi::URL& url, const String& mimeType, long long expectedContentLength, const String& textEncodingName) {
    ConstructFields(Checked(self), url, mimeType, expectedContentLength, textEncodingName);
}

void APS5_VABI _ZN7WebCore16ResourceResponseC1Ev(ResourceResponse* self) {
    ConstructDefault(Checked(self, "WebCore::ResourceResponse"));
}

void APS5_VABI _ZN7WebCore16ResourceResponseC2Ev(ResourceResponse* self) {
    ConstructDefault(Checked(self, "WebCore::ResourceResponse"));
}

void APS5_VABI _ZN7WebCore16ResourceResponseC1ERKN3WTF3URLERKNS1_6StringExS7_(ResourceResponse* self, const NKWebKit::WtfAbi::URL& url, const String& mimeType, long long expectedContentLength, const String& textEncodingName) {
    ConstructFields(Checked(self, "WebCore::ResourceResponse"), url, mimeType, expectedContentLength, textEncodingName);
}

void APS5_VABI _ZN7WebCore16ResourceResponseC2ERKN3WTF3URLERKNS1_6StringExS7_(ResourceResponse* self, const NKWebKit::WtfAbi::URL& url, const String& mimeType, long long expectedContentLength, const String& textEncodingName) {
    ConstructFields(Checked(self, "WebCore::ResourceResponse"), url, mimeType, expectedContentLength, textEncodingName);
}

void APS5_VABI _ZN7WebCore16ResourceResponseC1ERKS0_(ResourceResponse* self, const ResourceResponse* other) {
    ConstructCopy(Checked(self, "WebCore::ResourceResponse"), Checked(other, "WebCore::ResourceResponse source"));
}

void APS5_VABI _ZN7WebCore16ResourceResponseC2ERKS0_(ResourceResponse* self, const ResourceResponse* other) {
    ConstructCopy(Checked(self, "WebCore::ResourceResponse"), Checked(other, "WebCore::ResourceResponse source"));
}

ResourceResponse& APS5_VABI _ZN7WebCore16ResourceResponseaSERKS0_(ResourceResponse* self, const ResourceResponse* other) {
    return static_cast<ResourceResponse&>(CopyAssign(Checked(self, "WebCore::ResourceResponse"), Checked(other, "WebCore::ResourceResponse source")));
}

void APS5_VABI _ZN7WebCore16ResourceResponseD1Ev(ResourceResponse* self) {
    Destroy(Checked(self, "WebCore::ResourceResponse"));
}

void APS5_VABI _ZN7WebCore16ResourceResponseD2Ev(ResourceResponse* self) {
    Destroy(Checked(self, "WebCore::ResourceResponse"));
}

NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(const ResourceResponse* self) {
    return Filename(Header(self, HTTPHeaderName::ContentDisposition));
}

}
