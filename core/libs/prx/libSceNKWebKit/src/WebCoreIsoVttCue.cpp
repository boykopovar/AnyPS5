// Derived from Source/WebCore/platform/graphics/iso/ISOVTTCue.h@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/iso/ISOVTTCue.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#include "prx/libSceNKWebKit/include/IsoBoxExports.hpp"
#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cmath>
#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace {

using NKWebKit::IsoAbi::WebVTTCue;
using namespace NKWebKit::WtfAbi;

static_assert(std::is_trivially_copyable_v<StringImpl>);

constexpr unsigned kStringImplFlag8Bit = 4u;

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

NKWebKit::MediaAbi::MediaTime UnpackMediaTime(const unsigned char* bytes) {
    std::int64_t rawValue = 0;
    std::uint32_t scale = 0;
    std::uint8_t flags = 0;
    std::memcpy(&rawValue, bytes + offsetof(NKWebKit::MediaAbi::MediaTime, m_timeValue), sizeof(rawValue));
    std::memcpy(&scale, bytes + offsetof(NKWebKit::MediaAbi::MediaTime, m_timeScale), sizeof(scale));
    std::memcpy(&flags, bytes + offsetof(NKWebKit::MediaAbi::MediaTime, m_timeFlags), sizeof(flags));
    if (flags & NKWebKit::MediaAbi::MediaTime::DoubleValue) {
        double doubleValue = 0.0;
        std::memcpy(&doubleValue, bytes + offsetof(NKWebKit::MediaAbi::MediaTime, m_timeValueAsDouble), sizeof(doubleValue));
        NKWebKit::MediaAbi::MediaTime time(0, scale, flags);
        time.m_timeValueAsDouble = doubleValue;
        return time;
    }
    return NKWebKit::MediaAbi::MediaTime(rawValue, scale, flags);
}

String CopyGuestString(const StringImpl* guestImpl, const char* function) {
    if (!guestImpl) return {};
    StringImpl header{};
    if (!GuestCopyIn_nid_no_patch(&header, guestImpl, sizeof(header)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!header.length) return {};
    if (!header.data) throw std::invalid_argument("invalid WebCore::ISOWebVTTCue string");
    const bool is8Bit = (header.hashAndFlags & kStringImplFlag8Bit) != 0;
    const std::size_t unitSize = is8Bit ? 1u : sizeof(char16_t);
    if (header.length > std::numeric_limits<std::size_t>::max() / unitSize)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * unitSize;
    RequireReadableGuestBytes(header.data, byteLength, function);
    void* characters = nullptr;
    String copy(CreateUninitialized(header.length, is8Bit, characters));
    if (!GuestCopyIn_nid_no_patch(characters, header.data, byteLength))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return copy;
}

const StringImpl* UnpackStringImplPointer(const unsigned char* raw, std::size_t fieldOffset) {
    const StringImpl* pointer = nullptr;
    std::memcpy(&pointer, raw + fieldOffset, sizeof(pointer));
    return pointer;
}

struct JSONBuilder {
    StringBuilder builder{};

    JSONBuilder() { builder.is8Bit = true; }
    ~JSONBuilder() { Clear(builder); }
};

void AppendKey(StringBuilder& builder, const char* name) {
    if (builder.length > 1) AppendCharacter(builder, ',');
    AppendCharacter(builder, '"');
    AppendCString(builder, name);
    AppendCString(builder, "\":");
}

void AppendMember(StringBuilder& builder, const char* name, const String& string) {
    if (string.impl && string.impl->length && !string.impl->data)
        throw std::invalid_argument("invalid WebCore::ISOWebVTTCue string");
    AppendKey(builder, name);
    _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&builder, string);
}

void AppendMember(StringBuilder& builder, const char* name, const NKWebKit::MediaAbi::MediaTime& time) {
    const double value = _ZNK3WTF9MediaTime8toDoubleEv(&time);
    AppendKey(builder, name);
    AppendCString(builder, std::isfinite(value) ? NumberToString(value).c_str() : "null");
}

}  // namespace

extern "C" {

WebCore::FourCC APS5_VABI _ZN7WebCore12ISOWebVTTCue11boxTypeNameEv() {
    return WebCore::FourCC("vttc");
}

String APS5_VABI _ZNK7WebCore12ISOWebVTTCue12toJSONStringEv(const WebVTTCue* self) {
    if (!self) throw std::invalid_argument("null WebCore::ISOWebVTTCue");

    unsigned char raw[sizeof(WebVTTCue)]{};
    if (!GuestCopyIn_nid_no_patch(raw, self, sizeof(raw)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);

    const String cueText = CopyGuestString(UnpackStringImplPointer(raw, offsetof(WebVTTCue, cueText)), __func__);
    const String sourceID = CopyGuestString(UnpackStringImplPointer(raw, offsetof(WebVTTCue, sourceID)), __func__);
    const String identifier = CopyGuestString(UnpackStringImplPointer(raw, offsetof(WebVTTCue, identifier)), __func__);
    const String originalStartTime = CopyGuestString(UnpackStringImplPointer(raw, offsetof(WebVTTCue, originalStartTime)), __func__);
    const String settings = CopyGuestString(UnpackStringImplPointer(raw, offsetof(WebVTTCue, settings)), __func__);
    const auto presentationTime = UnpackMediaTime(raw + offsetof(WebVTTCue, presentationTime));
    const auto duration = UnpackMediaTime(raw + offsetof(WebVTTCue, duration));

    JSONBuilder json;
    auto& builder = json.builder;
    AppendCharacter(builder, '{');
    AppendMember(builder, "text", cueText);
    AppendMember(builder, "sourceId", _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(sourceID));
    AppendMember(builder, "id", _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(identifier));
    AppendMember(builder, "originalStartTime", _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(originalStartTime));
    AppendMember(builder, "settings", _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(settings));
    AppendMember(builder, "presentationTime", presentationTime);
    AppendMember(builder, "duration", duration);
    AppendCharacter(builder, '}');
    return ToString(builder);
}

}
