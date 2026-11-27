// Derived from Source/WTF/wtf/MediaTime.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WTF/wtf/JSONValues.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WTF/wtf/JSONValues.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/WtfMediaTimeRangeExports.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>

namespace {

using NKWebKit::MediaAbi::MediaTimeRange;
using NKWebKit::WtfAbi::String;
using WTF::MediaTime;

bool HasFlag(const MediaTime& time, unsigned flag) {
    return time.m_timeFlags & flag;
}

std::string JSONNumber(double value) {
    return std::isfinite(value) ? NKWebKit::WtfAbi::NumberToString(value) : "null";
}

std::string JSONObject(const MediaTime& time) {
    if (HasFlag(time, MediaTime::DoubleValue)) return "{\"value\":" + JSONNumber(_ZNK3WTF9MediaTime8toDoubleEv(&time)) + "}";
    std::string object = "{";
    if (!HasFlag(time, MediaTime::Valid))
        object += "\"invalid\":true,";
    else if (HasFlag(time, MediaTime::Indefinite))
        object += "\"value\":\"NaN\",";
    else if (HasFlag(time, MediaTime::PositiveInfinite))
        object += "\"value\":\"POSITIVE_INFINITY\",";
    else if (HasFlag(time, MediaTime::NegativeInfinite))
        object += "\"value\":\"NEGATIVE_INFINITY\",";
    else
        object += "\"value\":" + JSONNumber(_ZNK3WTF9MediaTime8toDoubleEv(&time)) + ",";
    object += "\"numerator\":" + JSONNumber(static_cast<double>(time.m_timeValue));
    object += ",\"denominator\":" + JSONNumber(static_cast<int>(time.m_timeScale));
    object += ",\"flags\":" + JSONNumber(time.m_timeFlags);
    return object + "}";
}

String Make(const std::string& text) {
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(text.size()), true, characters));
    std::memcpy(characters, text.data(), text.size());
    return result;
}

MediaTime UnpackMediaTime(const unsigned char* bytes) {
    std::int64_t rawValue = 0;
    std::uint32_t scale = 0;
    std::uint8_t flags = 0;
    std::memcpy(&rawValue, bytes + offsetof(MediaTime, m_timeValue), sizeof(rawValue));
    std::memcpy(&scale, bytes + offsetof(MediaTime, m_timeScale), sizeof(scale));
    std::memcpy(&flags, bytes + offsetof(MediaTime, m_timeFlags), sizeof(flags));
    if (flags & MediaTime::DoubleValue) {
        double doubleValue = 0.0;
        std::memcpy(&doubleValue, bytes + offsetof(MediaTime, m_timeValueAsDouble), sizeof(doubleValue));
        MediaTime time(0, scale, flags);
        time.m_timeValueAsDouble = doubleValue;
        return time;
    }
    return MediaTime(rawValue, scale, flags);
}

MediaTimeRange ReadGuestRange(const MediaTimeRange* self, const char* function) {
    unsigned char raw[sizeof(MediaTimeRange)]{};
    if (!GuestCopyIn_nid_no_patch(raw, self, sizeof(raw)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return MediaTimeRange{
        UnpackMediaTime(raw + offsetof(MediaTimeRange, start)),
        UnpackMediaTime(raw + offsetof(MediaTimeRange, end)),
    };
}

}

extern "C" {

String APS5_VABI _ZNK3WTF14MediaTimeRange12toJSONStringEv(const MediaTimeRange* self) {
    if (!self) throw std::invalid_argument("null WTF::MediaTimeRange");
    const MediaTimeRange range = ReadGuestRange(self, __func__);
    return Make("{\"start\":" + JSONObject(range.start) + ",\"end\":" + JSONObject(range.end) + "}");
}

}
