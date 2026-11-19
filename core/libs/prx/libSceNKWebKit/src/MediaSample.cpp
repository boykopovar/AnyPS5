/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
// Adapted from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/MediaSample.h

/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
// Adapted from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/MediaTime.cpp

#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"
#include "WebCoreTypes.hpp"
#include "WtfTextAbi.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::String;
using WebCore::FloatSize;
using WebCore::MediaSample;
using WTF::MediaTime;

constexpr unsigned IsSync = 1u << 0;
constexpr unsigned IsNonDisplaying = 1u << 1;
constexpr unsigned HasAlpha = 1u << 2;

using TimeGetter = MediaTime (APS5_VABI*)(const MediaSample*);
using SizeGetter = FloatSize (APS5_VABI*)(const MediaSample*);
using FlagsGetter = unsigned (APS5_VABI*)(const MediaSample*);

struct MediaSampleVtable {
    const void* destructors[2];
    TimeGetter presentationTime;
    TimeGetter decodeTime;
    TimeGetter duration;
    const void* trackID;
    const void* sizeInBytes;
    SizeGetter presentationSize;
    const void* offsetTimestampsBy;
    const void* setTimestamps;
    const void* isDivisable;
    const void* divide;
    const void* createNonDisplayingCopy;
    FlagsGetter flags;
};

static_assert(offsetof(MediaSampleVtable, presentationTime) == 2 * sizeof(void*));
static_assert(offsetof(MediaSampleVtable, presentationSize) == 7 * sizeof(void*));
static_assert(offsetof(MediaSampleVtable, flags) == 13 * sizeof(void*));

MediaSampleVtable ReadVtable(const MediaSample* self) {
    const MediaSampleVtable* address{};
    MediaSampleVtable vtable{};
    if (!GuestCopyIn_nid_no_patch(&address, self, sizeof(address))
        || !GuestCopyIn_nid_no_patch(&vtable, address, sizeof(vtable))) {
        throw std::invalid_argument("unreadable WebCore::MediaSample vtable");
    }
    return vtable;
}

unsigned SampleFlags(const MediaSample* self) {
    const auto vtable = ReadVtable(self);
    if (!vtable.flags) throw std::invalid_argument("missing WebCore::MediaSample flags callback");
    return vtable.flags(self);
}

void AppendJsonNumber(std::string& output, double number) {
    output += std::isfinite(number) ? NKWebKit::WtfAbi::NumberToString(number) : "null";
}

void AppendMediaTime(std::string& output, const MediaTime& time) {
    const auto flags = time.m_timeFlags;
    if (flags & MediaTime::DoubleValue) {
        output += "{\"value\":";
        AppendJsonNumber(output, _ZNK3WTF9MediaTime8toDoubleEv(&time));
        output += '}';
        return;
    }

    if (!(flags & MediaTime::Valid)) {
        output += "{\"invalid\":true";
    } else {
        output += "{\"value\":";
        if (flags & MediaTime::Indefinite) output += "\"NaN\"";
        else if (flags & MediaTime::PositiveInfinite) output += "\"POSITIVE_INFINITY\"";
        else if (flags & MediaTime::NegativeInfinite) output += "\"NEGATIVE_INFINITY\"";
        else AppendJsonNumber(output, _ZNK3WTF9MediaTime8toDoubleEv(&time));
    }

    output += ",\"numerator\":";
    AppendJsonNumber(output, static_cast<double>(time.m_timeValue));
    output += ",\"denominator\":";
    output += std::to_string(static_cast<int>(time.m_timeScale));
    output += ",\"flags\":";
    output += std::to_string(flags);
    output += '}';
}

String FinishJson(const std::string& output) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(output.data()),
        static_cast<unsigned>(output.size()));
}

} // namespace

bool APS5_VABI _ZNK7WebCore11MediaSample8hasAlphaEv(const MediaSample* self) {
    return (SampleFlags(self) & HasAlpha) != 0;
}

bool APS5_VABI _ZNK7WebCore11MediaSample15isNonDisplayingEv(const MediaSample* self) {
    return (SampleFlags(self) & IsNonDisplaying) != 0;
}

bool APS5_VABI _ZNK7WebCore11MediaSample6isSyncEv(const MediaSample* self) {
    return (SampleFlags(self) & IsSync) != 0;
}

String APS5_VABI _ZNK7WebCore11MediaSample12toJSONStringEv(const MediaSample* self) {
    const auto vtable = ReadVtable(self);
    if (!vtable.presentationTime || !vtable.decodeTime || !vtable.duration
        || !vtable.flags || !vtable.presentationSize) {
        throw std::invalid_argument("missing WebCore::MediaSample JSON callback");
    }

    std::string output = "{\"pts\":";
    AppendMediaTime(output, vtable.presentationTime(self));
    output += ",\"dts\":";
    AppendMediaTime(output, vtable.decodeTime(self));
    output += ",\"duration\":";
    AppendMediaTime(output, vtable.duration(self));
    output += ",\"flags\":";
    output += std::to_string(static_cast<int>(vtable.flags(self)));
    const auto size = vtable.presentationSize(self);
    output += ",\"presentationSize\":{\"width\":";
    AppendJsonNumber(output, size.m_width);
    output += ",\"height\":";
    AppendJsonNumber(output, size.m_height);
    output += "}}";
    return FinishJson(output);
}

String APS5_VABI _ZNK3WTF9MediaTime12toJSONStringEv(const MediaTime* self) {
    MediaTime time;
    if (!GuestCopyIn_nid_no_patch(&time, self, sizeof(time))) {
        throw std::invalid_argument("unreadable WTF::MediaTime");
    }
    std::string output;
    AppendMediaTime(output, time);
    return FinishJson(output);
}
