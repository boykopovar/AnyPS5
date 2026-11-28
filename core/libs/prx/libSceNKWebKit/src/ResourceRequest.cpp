/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/FormDataExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/PublicSuffixExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace NKWebKit::WebCoreAbi {

FormDataRefPtr::~FormDataRefPtr() { }

}

namespace {
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

double defaultTimeoutInterval = 0;

void RequireReadableGuestBytes(const void* pointer, std::size_t bytes, const char* function) {
    if (!bytes) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, bytes);
    if (!GuestReadable_nid_no_patch(pointer, bytes)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void CopyInGuest(void* destination, const void* source, std::size_t bytes, const char* function) {
    if (!bytes) return;
    RequireReadableGuestBytes(source, bytes, function);
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

class GuestWriteScope {
public:
    GuestWriteScope(void* pointer, std::size_t bytes, const char* function, const char* nullMessage, bool requireReadable = true)
        : guard(pointer ? pointer : nullptr, pointer ? bytes : 0) {
        if (!pointer) throw std::invalid_argument(nullMessage);
        if (!bytes) return;
        if (requireReadable) RequireReadableGuestBytes(pointer, bytes, function);
        if (!guard.Open() || !GuestWritable_nid_no_patch(pointer, bytes))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    GuestWriteScope(const GuestWriteScope&) = delete;
    GuestWriteScope& operator=(const GuestWriteScope&) = delete;

private:
    GuestArena::HostWrite guard;
};

template<typename T>
class GuestWriteRef : private GuestWriteScope {
public:
    GuestWriteRef(T* pointer, const char* function, const char* nullMessage, bool requireReadable = true)
        : GuestWriteScope(pointer, sizeof(T), function, nullMessage, requireReadable), ptr(pointer) {}
    T& Ref() const { return *ptr; }

private:
    T* ptr;
};

void ValidateGuestStringImpl(const NKWebKit::WtfAbi::StringImpl* impl, const char* function) {
    if (!impl) return;
    NKWebKit::WtfAbi::StringImpl header{};
    CopyInGuest(&header, impl, sizeof(header), function);
    if (!header.length) return;
    const bool is8Bit = (header.hashAndFlags & 4) != 0;
    const std::size_t unitSize = is8Bit ? 1 : sizeof(char16_t);
    if (header.length > std::numeric_limits<std::size_t>::max() / unitSize)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    RequireReadableGuestBytes(header.data, static_cast<std::size_t>(header.length) * unitSize, function);
}

GuestWriteRef<ResourceRequestBase> CheckedWrite(ResourceRequestBase* self, const char* function) {
    return GuestWriteRef<ResourceRequestBase>(self, function, "null WebCore::ResourceRequestBase");
}

const ResourceRequestBase& CheckedRead(const ResourceRequestBase* self, const char* function) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    RequireReadableGuestBytes(self, sizeof(*self), function);
    return *self;
}

GuestWriteRef<LegacyResourceRequestBase> CheckedWrite(LegacyResourceRequestBase* self, const char* function) {
    return GuestWriteRef<LegacyResourceRequestBase>(self, function, "null WebCore::ResourceRequestBase");
}

const LegacyResourceRequestBase& CheckedRead(const LegacyResourceRequestBase* self, const char* function) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    RequireReadableGuestBytes(self, sizeof(*self), function);
    return *self;
}

const URL& CheckedRead(const URL* url, const char* function) {
    if (!url) throw std::invalid_argument("null WTF::URL");
    URL copy{};
    CopyInGuest(&copy, url, sizeof(copy), function);
    ValidateGuestStringImpl(copy.impl, function);
    return *url;
}

const String& CheckedRead(const String* string, const char* function) {
    if (!string) throw std::invalid_argument("null WTF::String");
    NKWebKit::WtfAbi::StringImpl* impl = nullptr;
    CopyInGuest(&impl, string, sizeof(impl), function);
    ValidateGuestStringImpl(impl, function);
    return *string;
}

void UpdateResourceRequest(const ResourceRequestBase& request) {
    if (!request.resourceRequestUpdated) throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceRequest is not implemented");
}

void UpdateResourceRequest(const LegacyResourceRequestBase& request) {
    if (!request.resourceRequestUpdated) throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceRequest is not implemented");
}

void UpdateResourceRequestBody(const ResourceRequestBase& request) {
    if (!request.resourceRequestBodyUpdated) throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceHTTPBody is not implemented");
}

void Assign(String& target, const String& source, const char* function = "WebCore::ResourceRequestBase") {
    std::optional<GuestWriteScope> sourceWrite;
    if (source.impl)
        sourceWrite.emplace(source.impl, sizeof(*source.impl), function, "null WTF::StringImpl");
    std::optional<GuestWriteScope> targetWrite;
    if (target.impl && target.impl != source.impl)
        targetWrite.emplace(target.impl, sizeof(*target.impl), function, "null WTF::StringImpl");
    String copy(source);
    std::swap(target.impl, copy.impl);
}

void Assign(URL& target, const URL& source, const char* function = "WebCore::ResourceRequestBase") {
    std::optional<GuestWriteScope> sourceWrite;
    if (source.impl)
        sourceWrite.emplace(source.impl, sizeof(*source.impl), function, "null WTF::StringImpl");
    std::optional<GuestWriteScope> targetWrite;
    if (target.impl && target.impl != source.impl)
        targetWrite.emplace(target.impl, sizeof(*target.impl), function, "null WTF::StringImpl");
    String copy(source.impl);
    if (copy.impl) copy.impl->refCount += 2;
    std::swap(target.impl, copy.impl);
    target.bits = source.bits;
    target.userStart = source.userStart;
    target.userEnd = source.userEnd;
    target.passwordEnd = source.passwordEnd;
    target.hostEnd = source.hostEnd;
    target.pathAfterLastSlash = source.pathAfterLastSlash;
    target.pathEnd = source.pathEnd;
    target.queryEnd = source.queryEnd;
}

bool Equal(const URL& a, const URL& b) {
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.impl, b.impl);
}

bool Equal(const String& a, const String& b) {
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.impl, b.impl);
}

using NKWebKit::HttpAbi::HTTPHeaderName;

static_assert(sizeof(NKWebKit::WebCoreAbi::HTTPHeaderMap) == sizeof(NKWebKit::HttpAbi::HTTPHeaderMap));
static_assert(alignof(NKWebKit::WebCoreAbi::HTTPHeaderMap) == alignof(NKWebKit::HttpAbi::HTTPHeaderMap));

NKWebKit::HttpAbi::HTTPHeaderMap& HeaderFields(ResourceRequestBase& request) {
    return *reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
}

const NKWebKit::HttpAbi::HTTPHeaderMap& HeaderFields(const ResourceRequestBase& request) {
    return *reinterpret_cast<const NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
}

String HeaderField(const ResourceRequestBase* self, HTTPHeaderName name, const char* function) {
    const auto& request = CheckedRead(self, function);
    UpdateResourceRequest(request);
    return _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(&HeaderFields(request), name);
}

void SetHeaderField(ResourceRequestBase* self, HTTPHeaderName name, const String* value, bool add, const char* function) {
    auto reqAccess = CheckedWrite(self, function);
    auto& request = reqAccess.Ref();
    const auto& text = CheckedRead(value, function);
    UpdateResourceRequest(request);
    NKWebKit::HttpAbi::UpdateCommonHeader_nid_no_patch(HeaderFields(request), name, text, add);
    request.platformRequestUpdated = false;
}

String DefaultHTTPMethod() {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("GET"), 3);
}

String IsolatedString(const String& source) {
    return _ZNKR3WTF6String12isolatedCopyEv(&source);
}

String IsolatedString(const URL& source) {
    return _ZNKR3WTF6String12isolatedCopyEv(reinterpret_cast<const String*>(&source));
}

void AssignIsolatedString(String& target, String& isolated) {
    String copy(isolated);
    std::swap(target.impl, copy.impl);
}

void AssignIsolatedURL(URL& target, String& isolated, const URL& source) {
    String copy(isolated);
    std::swap(target.impl, copy.impl);
    target.bits = source.bits;
    target.userStart = source.userStart;
    target.userEnd = source.userEnd;
    target.passwordEnd = source.passwordEnd;
    target.hostEnd = source.hostEnd;
    target.pathAfterLastSlash = source.pathAfterLastSlash;
    target.pathEnd = source.pathEnd;
    target.queryEnd = source.queryEnd;
}

template<class TVector>
void ValidateVector(const TVector& vector, const char* function = "WebCore::ResourceRequestBase") {
    if (vector.size > vector.capacity) throw std::invalid_argument("invalid vector size");
    if (vector.size && !vector.buffer) throw std::invalid_argument("null vector storage");
    if (vector.size) {
        using RawEntry = std::remove_pointer_t<decltype(vector.buffer)>;
        constexpr std::size_t entrySize = std::is_void_v<RawEntry> ? sizeof(String) : sizeof(std::conditional_t<std::is_void_v<RawEntry>, char, RawEntry>);
        if (vector.size > std::numeric_limits<std::size_t>::max() / entrySize)
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        RequireReadableGuestBytes(vector.buffer, static_cast<std::size_t>(vector.size) * entrySize, function);
    }
}

void ReleaseHeaderMap(NKWebKit::HttpAbi::HTTPHeaderMap& map, const char* function = "WebCore::ResourceRequestBase") {
    ValidateVector(map.common, function);
    ValidateVector(map.uncommon, function);
    for (unsigned i = 0; i < map.common.size; ++i) NKWebKit::WtfAbi::Release(map.common.buffer[i].value.impl);
    for (unsigned i = 0; i < map.uncommon.size; ++i) {
        NKWebKit::WtfAbi::Release(map.uncommon.buffer[i].key.impl);
        NKWebKit::WtfAbi::Release(map.uncommon.buffer[i].value.impl);
    }
    _ZN3WTF8fastFreeEPv(map.common.buffer);
    _ZN3WTF8fastFreeEPv(map.uncommon.buffer);
    map.common = {};
    map.uncommon = {};
}

void AssignIsolatedHeaderMap(NKWebKit::HttpAbi::HTTPHeaderMap& target, const NKWebKit::HttpAbi::HTTPHeaderMap& source, const char* function) {
    ValidateVector(source.common, function);
    ValidateVector(source.uncommon, function);
    for (unsigned i = 0; i < source.common.size; ++i) ValidateGuestStringImpl(source.common.buffer[i].value.impl, function);
    for (unsigned i = 0; i < source.uncommon.size; ++i) {
        ValidateGuestStringImpl(source.uncommon.buffer[i].key.impl, function);
        ValidateGuestStringImpl(source.uncommon.buffer[i].value.impl, function);
    }
    NKWebKit::HttpAbi::HTTPHeaderMap copy{};
    copy.common.buffer = static_cast<NKWebKit::HttpAbi::CommonHeader*>(source.common.size ? _ZN3WTF10fastMallocEm(source.common.size * sizeof(NKWebKit::HttpAbi::CommonHeader)) : nullptr);
    copy.common.capacity = source.common.size;
    for (unsigned i = 0; i < source.common.size; ++i) {
        String value = IsolatedString(source.common.buffer[i].value);
        String retained(value);
        std::construct_at(copy.common.buffer + i, NKWebKit::HttpAbi::CommonHeader{source.common.buffer[i].key, std::move(retained)});
        ++copy.common.size;
    }
    copy.uncommon.buffer = static_cast<NKWebKit::HttpAbi::UncommonHeader*>(source.uncommon.size ? _ZN3WTF10fastMallocEm(source.uncommon.size * sizeof(NKWebKit::HttpAbi::UncommonHeader)) : nullptr);
    copy.uncommon.capacity = source.uncommon.size;
    for (unsigned i = 0; i < source.uncommon.size; ++i) {
        String key = IsolatedString(source.uncommon.buffer[i].key);
        String value = IsolatedString(source.uncommon.buffer[i].value);
        String retainedKey(key);
        String retainedValue(value);
        std::construct_at(copy.uncommon.buffer + i, NKWebKit::HttpAbi::UncommonHeader{std::move(retainedKey), std::move(retainedValue)});
        ++copy.uncommon.size;
    }
    ReleaseHeaderMap(target, function);
    target.common = copy.common;
    target.uncommon = copy.uncommon;
    copy.common = {};
    copy.uncommon = {};
}

void SetFallbackArray(ResourceRequestBase& request, const String* first, const String* second, const String* third, const char* function) {
    if (!first || !second || !third) throw std::invalid_argument("null WTF::String");
    const auto& firstValue = CheckedRead(first, function);
    const auto& secondValue = CheckedRead(second, function);
    const auto& thirdValue = CheckedRead(third, function);
    auto& vector = *reinterpret_cast<NKWebKit::WtfAbi::Vector<String>*>(&request.responseContentDispositionEncodingFallbackArray);
    if (vector.size > vector.capacity || (vector.size && !vector.buffer)) throw std::invalid_argument("invalid fallback array storage");
    if (vector.size) RequireReadableGuestBytes(vector.buffer, static_cast<std::size_t>(vector.size) * sizeof(String), function);
    UpdateResourceRequest(request);
    vector.clear();
    const unsigned count = (firstValue.impl != nullptr) + (secondValue.impl != nullptr) + (thirdValue.impl != nullptr);
    if (count) {
        vector.buffer = static_cast<String*>(_ZN3WTF10fastMallocEm(count * sizeof(String)));
        vector.capacity = count;
        vector.size = 0;
    }
    for (const String* text : {&firstValue, &secondValue, &thirdValue}) {
        if (!text->impl) continue;
        std::construct_at(vector.buffer + vector.size, *text);
        ++vector.size;
    }
    request.platformRequestUpdated = false;
}

char16_t ToASCIILower(char16_t unit) {
    return (unit >= u'A' && unit <= u'Z') ? static_cast<char16_t>(unit + (u'a' - u'A')) : unit;
}

bool EqualIgnoringASCIICase(const NKWebKit::WtfAbi::StringImpl* left, const NKWebKit::WtfAbi::StringImpl* right) {
    if (left == right) return true;
    if (!left || !right || left->length != right->length) return false;
    const auto leftView = NKWebKit::WtfAbi::ViewOf(left);
    const auto rightView = NKWebKit::WtfAbi::ViewOf(right);
    for (unsigned i = 0; i < leftView.length; ++i) {
        if (ToASCIILower(NKWebKit::WtfAbi::CodeUnitAt(leftView, i)) != ToASCIILower(NKWebKit::WtfAbi::CodeUnitAt(rightView, i)))
            return false;
    }
    return true;
}

bool EqualHeaderMaps(const NKWebKit::HttpAbi::HTTPHeaderMap& left, const NKWebKit::HttpAbi::HTTPHeaderMap& right, const char* function) {
    ValidateVector(left.common, function);
    ValidateVector(left.uncommon, function);
    ValidateVector(right.common, function);
    ValidateVector(right.uncommon, function);
    if (left.common.size != right.common.size || left.uncommon.size != right.uncommon.size) return false;
    for (unsigned j = 0; j < right.common.size; ++j)
        ValidateGuestStringImpl(right.common.buffer[j].value.impl, function);
    for (unsigned j = 0; j < right.uncommon.size; ++j) {
        ValidateGuestStringImpl(right.uncommon.buffer[j].key.impl, function);
        ValidateGuestStringImpl(right.uncommon.buffer[j].value.impl, function);
    }
    for (unsigned i = 0; i < left.common.size; ++i) {
        ValidateGuestStringImpl(left.common.buffer[i].value.impl, function);
        std::optional<GuestWriteScope> rightValueWrite;
        for (unsigned j = 0; j < right.common.size; ++j) {
            if (right.common.buffer[j].key == left.common.buffer[i].key) {
                if (auto* impl = right.common.buffer[j].value.impl)
                    rightValueWrite.emplace(impl, sizeof(*impl), function, "null WTF::StringImpl", false);
                break;
            }
        }
        String value = _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(&right, left.common.buffer[i].key);
        if (!_ZN3WTF5equalEPKNS_10StringImplES2_(value.impl, left.common.buffer[i].value.impl)) return false;
    }
    for (unsigned i = 0; i < left.uncommon.size; ++i) {
        ValidateGuestStringImpl(left.uncommon.buffer[i].key.impl, function);
        ValidateGuestStringImpl(left.uncommon.buffer[i].value.impl, function);
        std::optional<GuestWriteScope> rightValueWrite;
        for (unsigned j = 0; j < right.uncommon.size; ++j) {
            if (EqualIgnoringASCIICase(right.uncommon.buffer[j].key.impl, left.uncommon.buffer[i].key.impl)) {
                if (auto* impl = right.uncommon.buffer[j].value.impl)
                    rightValueWrite.emplace(impl, sizeof(*impl), function, "null WTF::StringImpl", false);
                break;
            }
        }
        String value = _ZNK7WebCore13HTTPHeaderMap17getUncommonHeaderERKN3WTF6StringE(&right, left.uncommon.buffer[i].key);
        if (!_ZN3WTF5equalEPKNS_10StringImplES2_(value.impl, left.uncommon.buffer[i].value.impl)) return false;
    }
    return true;
}

constexpr std::uint8_t kBytesElement = 0;
constexpr std::uint8_t kFileElement = 1;
constexpr std::uint8_t kBlobElement = 2;
constexpr std::uint8_t kValuelessElement = 0xFF;

void SetHTTPBody(ResourceRequestBase& request, NKWebKit::WebCoreAbi::FormData* adopted) {
    UpdateResourceRequest(request);
    auto* previous = static_cast<NKWebKit::WebCoreAbi::FormData*>(request.httpBody);
    request.httpBody = adopted;
    if (previous) previous->deref();
    request.resourceRequestBodyUpdated = true;
    request.platformRequestBodyUpdated = false;
}

bool EqualBodyBytes(const NKWebKit::WebCoreAbi::FormDataByteVector& left, const NKWebKit::WebCoreAbi::FormDataByteVector& right) {
    if (left.size != right.size) return false;
    if (!left.size) return true;
    if (!left.buffer || !right.buffer) throw std::invalid_argument("null WebCore::FormData byte storage");
    RequireReadableGuestBytes(left.buffer, left.size, __func__);
    RequireReadableGuestBytes(right.buffer, right.size, __func__);
    return std::memcmp(left.buffer, right.buffer, left.size) == 0;
}

const NKWebKit::WebCoreAbi::FormData* CheckedFormData(const void* pointer, const char* function) {
    if (!pointer) return nullptr;
    RequireReadableGuestBytes(pointer, sizeof(NKWebKit::WebCoreAbi::FormData), function);
    return static_cast<const NKWebKit::WebCoreAbi::FormData*>(pointer);
}

void ValidateFormDataForIsolatedCopy(const NKWebKit::WebCoreAbi::FormData& formData, const char* function) {
    ValidateVector(formData.elements, function);
    for (unsigned i = 0; i < formData.elements.size; ++i) {
        const auto& element = formData.elements.buffer[i];
        const auto kind = static_cast<std::uint8_t>(element.index & 0xFF);
        if (kind == kBytesElement) {
            ValidateVector(element.storage.bytes, function);
        } else if (kind == kFileElement) {
            ValidateGuestStringImpl(element.storage.file.filename, function);
        } else if (kind == kBlobElement) {
            ValidateGuestStringImpl(element.storage.blob.url.impl, function);
        }
    }
}

bool FormDataElementsEqual(const NKWebKit::WebCoreAbi::FormData& left, const NKWebKit::WebCoreAbi::FormData& right, const char* function) {
    ValidateVector(left.elements, function);
    ValidateVector(right.elements, function);
    if (left.elements.size != right.elements.size) return false;
    for (unsigned i = 0; i < left.elements.size; ++i) {
        const auto& first = left.elements.buffer[i];
        const auto& second = right.elements.buffer[i];
        const auto kind = static_cast<std::uint8_t>(first.index & 0xFF);
        const auto otherKind = static_cast<std::uint8_t>(second.index & 0xFF);
        if (kind != otherKind) return false;
        if (kind == kBytesElement) {
            if (!EqualBodyBytes(first.storage.bytes, second.storage.bytes)) return false;
        } else if (kind == kFileElement) {
            ValidateGuestStringImpl(first.storage.file.filename, function);
            ValidateGuestStringImpl(second.storage.file.filename, function);
            if (!_ZN3WTF5equalEPKNS_10StringImplES2_(first.storage.file.filename, second.storage.file.filename)) return false;
            if (first.storage.file.fileStart != second.storage.file.fileStart || first.storage.file.fileLength != second.storage.file.fileLength) return false;
            if (first.storage.file.hasExpectedFileModificationTime != second.storage.file.hasExpectedFileModificationTime) return false;
            if (first.storage.file.hasExpectedFileModificationTime && first.storage.file.expectedFileModificationTime != second.storage.file.expectedFileModificationTime) return false;
        } else if (kind == kBlobElement) {
            ValidateGuestStringImpl(first.storage.blob.url.impl, function);
            ValidateGuestStringImpl(second.storage.blob.url.impl, function);
            if (!Equal(first.storage.blob.url, second.storage.blob.url)) return false;
        } else if (kind == kValuelessElement) {
            continue;
        } else {
            throw std::invalid_argument("invalid WebCore::FormDataElement index");
        }
    }
    return true;
}

void SetIsolatedCopy(ResourceRequestBase& to, const ResourceRequestBase& from, const char* function) {
    UpdateResourceRequest(from);
    ValidateGuestStringImpl(from.url.impl, function);
    ValidateGuestStringImpl(from.firstPartyForCookies.impl, function);
    ValidateGuestStringImpl(from.httpMethod.impl, function);
    ValidateGuestStringImpl(from.initiatorIdentifier.impl, function);

    String url = IsolatedString(from.url);
    String firstParty = IsolatedString(from.firstPartyForCookies);
    String method = IsolatedString(from.httpMethod);
    String initiator = IsolatedString(from.initiatorIdentifier);

    UpdateResourceRequest(to);
    AssignIsolatedURL(to.url, url, from.url);
    to.platformRequestUpdated = false;
    if (to.cachePolicy != from.cachePolicy) {
        to.cachePolicy = from.cachePolicy;
        to.platformRequestUpdated = false;
    }
    if (to.timeoutInterval != from.timeoutInterval) {
        to.timeoutInterval = from.timeoutInterval;
        to.platformRequestUpdated = false;
    }
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(to.firstPartyForCookies.impl, from.firstPartyForCookies.impl)) {
        AssignIsolatedURL(to.firstPartyForCookies, firstParty, from.firstPartyForCookies);
        to.platformRequestUpdated = false;
    }
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(to.httpMethod.impl, from.httpMethod.impl)) {
        AssignIsolatedString(to.httpMethod, method);
        to.platformRequestUpdated = false;
    }
    if (to.priority != from.priority) {
        to.priority = from.priority;
        to.platformRequestUpdated = false;
    }
    to.requester = from.requester;
    AssignIsolatedString(to.initiatorIdentifier, initiator);
    if (from.inspectorInitiatorNodeIdentifier) to.inspectorInitiatorNodeIdentifier = from.inspectorInitiatorNodeIdentifier;
    if (from.sameSiteDisposition != static_cast<std::uint8_t>(NKWebKit::WebCoreAbi::Unspecified) && to.sameSiteDisposition != from.sameSiteDisposition) {
        to.sameSiteDisposition = from.sameSiteDisposition;
        to.platformRequestUpdated = false;
    }
    if (to.isTopSite != from.isTopSite) {
        to.isTopSite = from.isTopSite;
        to.platformRequestUpdated = false;
    }
    AssignIsolatedHeaderMap(HeaderFields(to), HeaderFields(from), function);
    const auto& source = *reinterpret_cast<const NKWebKit::WtfAbi::Vector<String>*>(&from.responseContentDispositionEncodingFallbackArray);
    if (source.size > 0) {
        if (source.size > source.capacity || !source.buffer) throw std::invalid_argument("invalid fallback array storage");
        RequireReadableGuestBytes(source.buffer, static_cast<std::size_t>(source.size) * sizeof(String), function);
        ValidateGuestStringImpl(source.buffer[0].impl, function);
        String first = IsolatedString(source.buffer[0]);
        String second;
        String third;
        if (source.size > 1) {
            ValidateGuestStringImpl(source.buffer[1].impl, function);
            second = IsolatedString(source.buffer[1]);
            if (source.size > 2) {
                ValidateGuestStringImpl(source.buffer[2].impl, function);
                third = IsolatedString(source.buffer[2]);
            }
        }
        SetFallbackArray(to, &first, &second, &third, function);
    }
    if (from.httpBody) {
        auto* body = CheckedFormData(from.httpBody, function);
        ValidateFormDataForIsolatedCopy(*body, function);
        auto isolated = _ZNK7WebCore8FormData12isolatedCopyEv(body);
        SetHTTPBody(to, std::exchange(isolated.m_ptr, nullptr));
    }
    if (to.allowCookies != from.allowCookies) {
        to.allowCookies = from.allowCookies;
        to.platformRequestUpdated = false;
    }
    if (to.isAppInitiated != from.isAppInitiated) {
        to.isAppInitiated = from.isAppInitiated;
        to.platformRequestUpdated = false;
    }
    if (to.privacyProxyFailClosedForUnreachableNonMainHosts != from.privacyProxyFailClosedForUnreachableNonMainHosts) {
        to.privacyProxyFailClosedForUnreachableNonMainHosts = from.privacyProxyFailClosedForUnreachableNonMainHosts;
        to.platformRequestUpdated = false;
    }
    if (to.useAdvancedPrivacyProtections != from.useAdvancedPrivacyProtections) {
        to.useAdvancedPrivacyProtections = from.useAdvancedPrivacyProtections;
        to.platformRequestUpdated = false;
    }
}

}

extern "C" {

const URL& APS5_VABI _ZNK7WebCore19ResourceRequestBase3urlEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return request.url;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase6setURLERKN3WTF3URLE(ResourceRequestBase* self, const URL* url) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    const auto& value = CheckedRead(url, __func__);
    UpdateResourceRequest(request);
    Assign(request.url, value, __func__);
    request.platformRequestUpdated = false;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase6isNullEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return !request.url.impl;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase7isEmptyEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    ValidateGuestStringImpl(request.url.impl, __func__);
    return !request.url.impl || !request.url.impl->length;
}

const URL& APS5_VABI _ZNK7WebCore19ResourceRequestBase20firstPartyForCookiesEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return request.firstPartyForCookies;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase23setFirstPartyForCookiesERKN3WTF3URLE(ResourceRequestBase* self, const URL* url) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    const auto& value = CheckedRead(url, __func__);
    UpdateResourceRequest(request);
    ValidateGuestStringImpl(request.firstPartyForCookies.impl, __func__);
    if (Equal(request.firstPartyForCookies, value)) return;
    Assign(request.firstPartyForCookies, value, __func__);
    request.platformRequestUpdated = false;
}

const String& APS5_VABI _ZNK7WebCore19ResourceRequestBase10httpMethodEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return request.httpMethod;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(ResourceRequestBase* self, const String* method) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    const auto& value = CheckedRead(method, __func__);
    UpdateResourceRequest(request);
    ValidateGuestStringImpl(request.httpMethod.impl, __func__);
    if (Equal(request.httpMethod, value)) return;
    Assign(request.httpMethod, value, __func__);
    request.platformRequestUpdated = false;
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(const ResourceRequestBase* self, HTTPHeaderName name) {
    return HeaderField(self, name, __func__);
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(const ResourceRequestBase* self, HTTPHeaderName name) {
    return _ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&HeaderFields(CheckedRead(self, __func__)), name);
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase12httpReferrerEv(const ResourceRequestBase* self) {
    return HeaderField(self, HTTPHeaderName::Referer, __func__);
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(const ResourceRequestBase* self) {
    return HeaderField(self, HTTPHeaderName::ContentType, __func__);
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase13httpUserAgentEv(const ResourceRequestBase* self) {
    return HeaderField(self, HTTPHeaderName::UserAgent, __func__);
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase10httpOriginEv(const ResourceRequestBase* self) {
    return HeaderField(self, HTTPHeaderName::Origin, __func__);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(ResourceRequestBase* self, HTTPHeaderName name, const String* value) {
    SetHeaderField(self, name, value, false, __func__);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(ResourceRequestBase* self, const String* name, const String* value) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    const auto& key = CheckedRead(name, __func__);
    const auto& text = CheckedRead(value, __func__);
    UpdateResourceRequest(request);
    _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&HeaderFields(request), key, text);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(ResourceRequestBase* self, HTTPHeaderName name, const String* value) {
    SetHeaderField(self, name, value, true, __func__);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldERKN3WTF6StringES4_(ResourceRequestBase* self, const String* name, const String* value) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    const auto& key = CheckedRead(name, __func__);
    const auto& text = CheckedRead(value, __func__);
    UpdateResourceRequest(request);
    _ZN7WebCore13HTTPHeaderMap3addERKN3WTF6StringES4_(&HeaderFields(request), key, text);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase30addHTTPHeaderFieldIfNotPresentENS_14HTTPHeaderNameERKN3WTF6StringE(ResourceRequestBase* self, HTTPHeaderName name, const String* value) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    const auto& text = CheckedRead(value, __func__);
    UpdateResourceRequest(request);
    if (_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&HeaderFields(request), name)) return;
    NKWebKit::HttpAbi::UpdateCommonHeader_nid_no_patch(HeaderFields(request), name, text, false);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase15setHTTPReferrerERKN3WTF6StringE(ResourceRequestBase* self, const String* referrer) {
    const auto& text = CheckedRead(referrer, __func__);
    if (text.impl && text.impl->length > 4096) NotImplemented_nid_no_patch(__func__);
    SetHeaderField(self, HTTPHeaderName::Referer, referrer, false, __func__);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase16setHTTPUserAgentERKN3WTF6StringE(ResourceRequestBase* self, const String* userAgent) {
    SetHeaderField(self, HTTPHeaderName::UserAgent, userAgent, false, __func__);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase18setHTTPContentTypeERKN3WTF6StringE(ResourceRequestBase* self, const String* contentType) {
    SetHeaderField(self, HTTPHeaderName::ContentType, contentType, false, __func__);
}

double APS5_VABI _ZNK7WebCore19ResourceRequestBase15timeoutIntervalEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return request.timeoutInterval;
}

std::uint8_t APS5_VABI _ZNK7WebCore19ResourceRequestBase11cachePolicyEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return request.cachePolicy;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase14setCachePolicyENS_26ResourceRequestCachePolicyE(ResourceRequestBase* self, std::uint8_t cachePolicy) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    if (request.cachePolicy == cachePolicy) return;
    request.cachePolicy = cachePolicy;
    request.platformRequestUpdated = false;
}

std::uint8_t APS5_VABI _ZNK7WebCore19ResourceRequestBase8priorityEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return request.priority;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase11setPriorityENS_20ResourceLoadPriorityE(ResourceRequestBase* self, std::uint8_t priority) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    if (request.priority == priority) return;
    request.priority = priority;
    request.platformRequestUpdated = false;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase10isSameSiteEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return request.sameSiteDisposition == NKWebKit::WebCoreAbi::SameSite;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase13setIsSameSiteEb(ResourceRequestBase* self, bool isSameSite) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    auto disposition = isSameSite ? NKWebKit::WebCoreAbi::SameSite : NKWebKit::WebCoreAbi::CrossSite;
    if (request.sameSiteDisposition == disposition) return;
    request.sameSiteDisposition = disposition;
    request.platformRequestUpdated = false;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase9isTopSiteEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return request.isTopSite;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase12setIsTopSiteEb(ResourceRequestBase* self, bool isTopSite) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    if (request.isTopSite == isTopSite) return;
    request.isTopSite = isTopSite;
    request.platformRequestUpdated = false;
}

double APS5_VABI _ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() {
    return defaultTimeoutInterval;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase25setDefaultTimeoutIntervalEd(double timeoutInterval) {
    defaultTimeoutInterval = timeoutInterval;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase20clearHTTPContentTypeEv(ResourceRequestBase* self) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    auto* headers = reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(headers, HTTPHeaderName::ContentType);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase22clearHTTPAuthorizationEv(ResourceRequestBase* self) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    auto* headers = reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    if (!_ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Authorization))
        return;
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(ResourceRequestBase* self) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    auto* headers = reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Origin);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17makeUnconditionalEv(ResourceRequestBase* self) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    auto* headers = reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    constexpr HTTPHeaderName conditionalHeaders[] = {
        HTTPHeaderName::IfMatch,
        HTTPHeaderName::IfModifiedSince,
        HTTPHeaderName::IfNoneMatch,
        HTTPHeaderName::IfRange,
        HTTPHeaderName::IfUnmodifiedSince,
    };
    for (auto header : conditionalHeaders)
        _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(headers, header);
}

void APS5_VABI WKURLRequestSetDefaultTimeoutInterval(double timeoutInterval) {
    _ZN7WebCore19ResourceRequestBase25setDefaultTimeoutIntervalEd(timeoutInterval);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(LegacyResourceRequestBase* self, NKWebKit::WebCoreAbi::FormData** httpBody) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    if (!httpBody) throw std::invalid_argument("null WTF::RefPtr");
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    GuestWriteScope bodyPtrAccess(httpBody, sizeof(*httpBody), __func__, "null WTF::RefPtr");
    NKWebKit::WebCoreAbi::FormData* adopted = nullptr;
    CopyInGuest(&adopted, httpBody, sizeof(adopted), __func__);
    UpdateResourceRequest(request);
    auto* previous = static_cast<NKWebKit::WebCoreAbi::FormData*>(request.httpBody);
    std::optional<GuestWriteScope> previousAccess;
    if (previous)
        previousAccess.emplace(previous, sizeof(NKWebKit::WebCoreAbi::FormData), __func__, "null WebCore::FormData");
    NKWebKit::WebCoreAbi::FormData* cleared = nullptr;
    if (!GuestCopyOut_nid_no_patch(httpBody, &cleared, sizeof(cleared)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    request.httpBody = adopted;
    if (previous) previous->deref();
    request.resourceRequestBodyUpdated = true;
    request.platformRequestBodyUpdated = false;
}

String APS5_VABI _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldERKN3WTF6StringE(const LegacyResourceRequestBase* self, const String* name) {
    const auto& request = CheckedRead(self, __func__);
    const auto& key = CheckedRead(name, __func__);
    UpdateResourceRequest(request);
    return _ZNK7WebCore13HTTPHeaderMap3getERKN3WTF6StringE(reinterpret_cast<const NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields), key);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase12clearPurposeEv(ResourceRequestBase* self) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&HeaderFields(request), HTTPHeaderName::Purpose);
    request.platformRequestUpdated = false;
}

String APS5_VABI _ZN7WebCore19ResourceRequestBase13partitionNameERKN3WTF6StringE(const String* domain) {
    if (!domain) throw std::invalid_argument("ResourceRequestBase::partitionName: null domain");
    NKWebKit::WtfAbi::StringImpl* impl = nullptr;
    CopyInGuest(&impl, domain, sizeof(impl), __func__);
    if (!impl) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    ValidateGuestStringImpl(impl, __func__);
    String highLevel = _ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(domain);
    if (!highLevel.impl) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    return highLevel;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv(ResourceRequestBase* self) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&HeaderFields(request), HTTPHeaderName::Referer);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17removeCredentialsEv(ResourceRequestBase* self) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    UpdateResourceRequest(request);
    if (!(request.url.passwordEnd > request.url.userStart)) return;
    if (request.url.bits & 1) NotImplemented_nid_no_patch(__func__);
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17setAsIsolatedCopyERKNS_15ResourceRequestE(ResourceRequestBase* self, const ResourceRequestBase* other) {
    auto reqAccess = CheckedWrite(self, __func__);
    const auto& source = CheckedRead(other, __func__);
    SetIsolatedCopy(reqAccess.Ref(), source, __func__);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase17setCachePartitionERKN3WTF6StringE(ResourceRequestBase*, const String*) {
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase19setHTTPHeaderFieldsENS_13HTTPHeaderMapE(ResourceRequestBase* self, NKWebKit::WebCoreAbi::HTTPHeaderMap* headerFields) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    if (!headerFields) throw std::invalid_argument("null WebCore::HTTPHeaderMap");
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    GuestWriteRef<NKWebKit::HttpAbi::HTTPHeaderMap> sourceAccess(
        reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(headerFields), __func__, "null WebCore::HTTPHeaderMap");
    auto& source = sourceAccess.Ref();
    ValidateVector(source.common, __func__);
    ValidateVector(source.uncommon, __func__);
    UpdateResourceRequest(request);
    ReleaseHeaderMap(HeaderFields(request), __func__);
    auto& map = HeaderFields(request);
    map.common = std::move(source.common);
    map.uncommon = std::move(source.uncommon);
    source.common = {};
    source.uncommon = {};
    request.platformRequestUpdated = false;
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase37setExistingHTTPReferrerToOriginStringEv(ResourceRequestBase* self) {
    auto reqAccess = CheckedWrite(self, __func__);
    auto& request = reqAccess.Ref();
    if (!_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&HeaderFields(request), HTTPHeaderName::Referer)) return;
    UpdateResourceRequest(request);
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(ResourceRequestBase* self, const String* first, const String* second, const String* third) {
    auto reqAccess = CheckedWrite(self, __func__);
    SetFallbackArray(reqAccess.Ref(), first, second, third, __func__);
}

bool APS5_VABI _ZN7WebCore19ResourceRequestBase5equalERKNS_15ResourceRequestES3_(const ResourceRequestBase* left, const ResourceRequestBase* right) {
    const auto& first = CheckedRead(left, __func__);
    const auto& second = CheckedRead(right, __func__);
    UpdateResourceRequest(first);
    UpdateResourceRequest(second);
    ValidateGuestStringImpl(first.url.impl, __func__);
    ValidateGuestStringImpl(second.url.impl, __func__);
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(first.url.impl, second.url.impl)) return false;
    if (first.cachePolicy != second.cachePolicy) return false;
    if (first.timeoutInterval != second.timeoutInterval) return false;
    ValidateGuestStringImpl(first.firstPartyForCookies.impl, __func__);
    ValidateGuestStringImpl(second.firstPartyForCookies.impl, __func__);
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(first.firstPartyForCookies.impl, second.firstPartyForCookies.impl)) return false;
    if ((first.sameSiteDisposition == NKWebKit::WebCoreAbi::SameSite) != (second.sameSiteDisposition == NKWebKit::WebCoreAbi::SameSite)) return false;
    if (first.isTopSite != second.isTopSite) return false;
    ValidateGuestStringImpl(first.httpMethod.impl, __func__);
    ValidateGuestStringImpl(second.httpMethod.impl, __func__);
    if (!_ZN3WTF5equalEPKNS_10StringImplES2_(first.httpMethod.impl, second.httpMethod.impl)) return false;
    if (first.allowCookies != second.allowCookies) return false;
    if (first.priority != second.priority) return false;
    if (first.requester != second.requester) return false;
    UpdateResourceRequestBody(first);
    UpdateResourceRequestBody(second);
    if (first.httpBody != second.httpBody) {
        if (!first.httpBody || !second.httpBody) return false;
        const auto* leftBody = CheckedFormData(first.httpBody, __func__);
        const auto* rightBody = CheckedFormData(second.httpBody, __func__);
        if (!FormDataElementsEqual(*leftBody, *rightBody, __func__)) return false;
    }
    return EqualHeaderMaps(HeaderFields(first), HeaderFields(second), __func__);
}

ResourceRequestBase APS5_VABI _ZNK7WebCore19ResourceRequestBase12isolatedCopyEv(const ResourceRequestBase* self) {
    const auto& source = CheckedRead(self, __func__);
    ResourceRequestBase request{};
    request.timeoutInterval = defaultTimeoutInterval;
    request.priority = 1;
    request.allowCookies = true;
    request.isAppInitiated = true;
    request.httpMethod = DefaultHTTPMethod();
    request.cachePartition.impl = NKWebKit::WtfAbi::EmptyStringImpl();
    request.resourceRequestUpdated = true;
    request.resourceRequestBodyUpdated = true;
    SetIsolatedCopy(request, source, __func__);
    return request;
}

const NKWebKit::WebCoreAbi::HTTPHeaderMap& APS5_VABI _ZNK7WebCore19ResourceRequestBase16httpHeaderFieldsEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    return reinterpret_cast<const NKWebKit::WebCoreAbi::HTTPHeaderMap&>(HeaderFields(request));
}

NKWebKit::WebCoreAbi::FormDataRefPtr APS5_VABI _ZNK7WebCore19ResourceRequestBase8httpBodyEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    UpdateResourceRequestBody(request);
    NKWebKit::WebCoreAbi::FormDataRefPtr result;
    result.ptr = static_cast<NKWebKit::WebCoreAbi::FormData*>(request.httpBody);
    if (result.ptr) {
        GuestWriteScope bodyAccess(result.ptr, sizeof(NKWebKit::WebCoreAbi::FormData), __func__, "null WebCore::FormData");
        result.ptr->ref();
    }
    return result;
}

bool APS5_VABI _ZNK7WebCore19ResourceRequestBase9hasUploadEv(const ResourceRequestBase* self) {
    const auto& request = CheckedRead(self, __func__);
    UpdateResourceRequest(request);
    UpdateResourceRequestBody(request);
    const auto* body = CheckedFormData(request.httpBody, __func__);
    if (!body) return false;
    ValidateVector(body->elements, __func__);
    for (unsigned i = 0; i < body->elements.size; ++i) {
        const auto kind = static_cast<std::uint8_t>(body->elements.buffer[i].index & 0xFF);
        if (kind == kFileElement || kind == kBlobElement) return true;
        if (kind != kBytesElement && kind != kValuelessElement) throw std::invalid_argument("invalid WebCore::FormDataElement index");
    }
    return false;
}

}

namespace {

using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WebCoreAbi::ResourceResponseStatusLayout;

constexpr int kMovedPermanently = 301;
constexpr int kFound = 302;
constexpr int kSeeOther = 303;

NKWebKit::HttpAbi::HTTPHeaderMap& HeaderFields(LegacyResourceRequestBase& request) {
    return *reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
}

const NKWebKit::HttpAbi::HTTPHeaderMap& HeaderFields(const LegacyResourceRequestBase& request) {
    return *reinterpret_cast<const NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
}

template<class TVector>
void ValidateCopySource(const TVector& source, const char* function = "WebCore::ResourceRequestBase") {
    if (source.size > source.capacity) throw std::invalid_argument("invalid WTF::Vector size in a WebCore::ResourceRequest");
    if (source.size && !source.buffer) throw std::invalid_argument("null WTF::Vector storage in a WebCore::ResourceRequest");
    if (source.size) {
        using RawEntry = std::remove_pointer_t<decltype(source.buffer)>;
        constexpr std::size_t entrySize = std::is_void_v<RawEntry> ? sizeof(String) : sizeof(std::conditional_t<std::is_void_v<RawEntry>, char, RawEntry>);
        if (source.size > std::numeric_limits<std::size_t>::max() / entrySize)
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        RequireReadableGuestBytes(source.buffer, static_cast<std::size_t>(source.size) * entrySize, function);
    }
}

template<class TEntry, class TVector>
void CopyVector(TVector& target, const TVector& source) {
    target = {};
    if (!source.capacity) return;
    auto* buffer = static_cast<TEntry*>(_ZN3WTF10fastMallocEm(std::size_t{source.capacity} * sizeof(TEntry)));
    const auto* entries = static_cast<const TEntry*>(source.buffer);
    for (unsigned index = 0; index < source.size; ++index) new (&buffer[index]) TEntry(entries[index]);
    target.buffer = buffer;
    target.capacity = source.capacity;
    target.size = source.size;
}

LegacyResourceRequestBase CopyRequest(const LegacyResourceRequestBase& source) {
    const auto& sourceHeaders = HeaderFields(source);
    ValidateCopySource(sourceHeaders.common, __func__);
    ValidateCopySource(sourceHeaders.uncommon, __func__);
    ValidateCopySource(source.responseContentDispositionEncodingFallbackArray, __func__);
    ValidateGuestStringImpl(source.firstPartyForCookies.impl, __func__);
    ValidateGuestStringImpl(source.httpMethod.impl, __func__);
    ValidateGuestStringImpl(source.initiatorIdentifier.impl, __func__);
    ValidateGuestStringImpl(source.cachePartition.impl, __func__);
    LegacyResourceRequestBase copy{};
    copy.timeoutInterval = source.timeoutInterval;
    Assign(copy.firstPartyForCookies, source.firstPartyForCookies);
    copy.httpMethod = source.httpMethod;
    copy.initiatorIdentifier = source.initiatorIdentifier;
    copy.cachePartition = source.cachePartition;
    auto& headers = HeaderFields(copy);
    CopyVector<NKWebKit::HttpAbi::CommonHeader>(headers.common, sourceHeaders.common);
    CopyVector<NKWebKit::HttpAbi::UncommonHeader>(headers.uncommon, sourceHeaders.uncommon);
    CopyVector<String>(copy.responseContentDispositionEncodingFallbackArray, source.responseContentDispositionEncodingFallbackArray);
    copy.httpBody = source.httpBody;
    if (copy.httpBody) {
        GuestWriteScope bodyAccess(copy.httpBody, sizeof(NKWebKit::WebCoreAbi::FormData), __func__, "null WebCore::FormData");
        static_cast<NKWebKit::WebCoreAbi::FormData*>(copy.httpBody)->ref();
    }
    copy.cachePolicy = source.cachePolicy;
    copy.sameSiteDisposition = source.sameSiteDisposition;
    copy.priority = source.priority;
    copy.requester = source.requester;
    copy.inspectorInitiatorNodeIdentifier = source.inspectorInitiatorNodeIdentifier;
    copy.allowCookies = source.allowCookies;
    copy.resourceRequestBodyUpdated = source.resourceRequestBodyUpdated;
    copy.platformRequestBodyUpdated = source.platformRequestBodyUpdated;
    copy.hiddenFromInspector = source.hiddenFromInspector;
    copy.isTopSite = source.isTopSite;
    return copy;
}

bool EqualLettersIgnoringASCIICase(const String& string, std::string_view lowerCaseLetters) {
    if (!string.impl || string.impl->length != lowerCaseLetters.size()) return false;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (unsigned index = 0; index < view.length; ++index) {
        if ((NKWebKit::WtfAbi::CodeUnitAt(view, index) | 0x20) != static_cast<unsigned char>(lowerCaseLetters[index])) return false;
    }
    return true;
}

bool ShouldUseGet(const String& method, int statusCode) {
    if (EqualLettersIgnoringASCIICase(method, "get") || EqualLettersIgnoringASCIICase(method, "head")) return false;
    if (statusCode == kMovedPermanently || statusCode == kFound) return EqualLettersIgnoringASCIICase(method, "post");
    return statusCode == kSeeOther;
}

bool IsASCIIAlphaCaselessEqual(char16_t unit, char lowerCaseLetter) {
    return (unit | 0x20) == static_cast<unsigned char>(lowerCaseLetter);
}

bool ProtocolIs(const String& string, std::string_view protocol) {
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    bool isLeading = true;
    std::size_t position = 0;
    for (unsigned index = 0; index < view.length; ++index) {
        const char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, index);
        if (isLeading) {
            if (unit <= u' ') continue;
            isLeading = false;
        } else if (unit == u'\t' || unit == u'\r' || unit == u'\n') {
            continue;
        }
        if (position == protocol.size()) return unit == u':';
        if (!IsASCIIAlphaCaselessEqual(unit, protocol[position++])) return false;
    }
    return false;
}

}

extern "C" LegacyResourceRequestBase APS5_VABI _ZNK7WebCore19ResourceRequestBase17redirectedRequestERKNS_16ResourceResponseEb(
    const LegacyResourceRequestBase* self, const ResourceResponseBase* redirectResponse, bool shouldClearReferrerOnHTTPSToHTTPRedirect) {
    const auto& source = CheckedRead(self, __func__);
    if (!redirectResponse) throw std::invalid_argument("null WebCore::ResourceResponseBase");
    RequireReadableGuestBytes(redirectResponse, sizeof(ResourceResponseStatusLayout), __func__);
    if (!source.resourceRequestUpdated) {
        GuestWriteScope sourceWrite(const_cast<LegacyResourceRequestBase*>(self), sizeof(*self), __func__, "null WebCore::ResourceRequestBase");
        source.resourceRequestUpdated = true;
    }
    int statusCode = 0;
    CopyInGuest(&statusCode,
        reinterpret_cast<const unsigned char*>(redirectResponse) + offsetof(ResourceResponseStatusLayout, httpStatusCode),
        sizeof(statusCode), __func__);
    const String location = _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(redirectResponse, HTTPHeaderName::Location);
    const URL& responseURL = _ZNK7WebCore20ResourceResponseBase3urlEv(redirectResponse);
    URL target{};
    if (location.impl && location.impl->length) _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&target, &responseURL, &location, nullptr);
    const String ownURL(std::exchange(target.impl, nullptr));
    target.impl = ownURL.impl;

    LegacyResourceRequestBase request = CopyRequest(source);
    auto& headers = HeaderFields(request);
    request.resourceRequestUpdated = true;
    Assign(request.url, target, __func__);
    if (ShouldUseGet(source.httpMethod, statusCode)) {
        request.httpMethod = DefaultHTTPMethod();
        if (auto* body = static_cast<NKWebKit::WebCoreAbi::FormData*>(std::exchange(request.httpBody, nullptr))) body->deref();
        request.resourceRequestBodyUpdated = true;
        request.platformRequestBodyUpdated = false;
        for (const HTTPHeaderName name : {HTTPHeaderName::ContentLength, HTTPHeaderName::ContentLanguage, HTTPHeaderName::ContentEncoding,
                                          HTTPHeaderName::ContentLocation, HTTPHeaderName::ContentType})
            _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&headers, name);
    }
    if (shouldClearReferrerOnHTTPSToHTTPRedirect && !_ZNK3WTF3URL10protocolIsEPKc(&request.url, "https")) {
        const String referrer = _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(&headers, HTTPHeaderName::Referer);
        if (ProtocolIs(referrer, "https")) _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&headers, HTTPHeaderName::Referer);
    }
    if (!_ZN3WTF27protocolHostAndPortAreEqualERKNS_3URLES2_(&request.url, &responseURL))
        _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&headers, HTTPHeaderName::Origin);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&headers, HTTPHeaderName::Authorization);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&headers, HTTPHeaderName::ProxyAuthorization);
    return request;
}
