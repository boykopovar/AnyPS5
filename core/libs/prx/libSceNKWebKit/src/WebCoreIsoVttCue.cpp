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

namespace {

using NKWebKit::IsoAbi::WebVTTCue;
using namespace NKWebKit::WtfAbi;

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

    JSONBuilder json;
    auto& builder = json.builder;
    AppendCharacter(builder, '{');
    AppendMember(builder, "text", self->cueText);
    AppendMember(builder, "sourceId", _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(self->sourceID));
    AppendMember(builder, "id", _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(self->identifier));
    AppendMember(builder, "originalStartTime", _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(self->originalStartTime));
    AppendMember(builder, "settings", _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(self->settings));
    AppendMember(builder, "presentationTime", self->presentationTime);
    AppendMember(builder, "duration", self->duration);
    AppendCharacter(builder, '}');
    return ToString(builder);
}

}
