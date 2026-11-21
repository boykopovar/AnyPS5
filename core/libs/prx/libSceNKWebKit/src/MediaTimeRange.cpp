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
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/MediaTime.cpp

#include "prx/libSceNKWebKit/include/MediaTimeRangeExports.hpp"
#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::MediaAbi::MediaTimeRange;
using WTF::MediaTime;

struct ErrnoGuard {
    int saved{errno};
    ~ErrnoGuard() { errno = saved; }
};

std::string JsonNumber(double value) {
    return std::isfinite(value) ? NKWebKit::WtfAbi::NumberToString(value) : "null";
}

std::string TimeObject(const MediaTime& time) {
    const auto flags = time.m_timeFlags;
    const auto value = _ZNK3WTF9MediaTime8toDoubleEv(&time);
    if (flags & MediaTime::DoubleValue) return "{\"value\":" + JsonNumber(value) + "}";
    std::string result = "{";
    if (!(flags & MediaTime::Valid)) result += "\"invalid\":true";
    else if (flags & MediaTime::Indefinite) result += "\"value\":\"NaN\"";
    else if (flags & MediaTime::PositiveInfinite) result += "\"value\":\"POSITIVE_INFINITY\"";
    else if (flags & MediaTime::NegativeInfinite) result += "\"value\":\"NEGATIVE_INFINITY\"";
    else result += "\"value\":" + JsonNumber(value);
    result += ",\"numerator\":" + JsonNumber(static_cast<double>(time.m_timeValue));
    result += ",\"denominator\":" + std::to_string(static_cast<std::int32_t>(time.m_timeScale));
    result += ",\"flags\":" + std::to_string(flags) + "}";
    return result;
}

} // namespace

extern "C" NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF14MediaTimeRange12toJSONStringEv(const MediaTimeRange* self) {
    ErrnoGuard guard;
    MediaTimeRange range;
    const auto address = reinterpret_cast<std::uintptr_t>(self);
    constexpr auto endOffset = offsetof(MediaTimeRange, end);
    constexpr auto timeBytes = offsetof(MediaTime, m_timeFlags) + sizeof(std::uint8_t);
    if (address > std::numeric_limits<std::uintptr_t>::max() - endOffset
        || !GuestCopyIn_nid_no_patch(&range.start, self, timeBytes)
        || !GuestCopyIn_nid_no_patch(&range.end, reinterpret_cast<const void*>(address + endOffset), timeBytes))
        throw std::invalid_argument("MediaTimeRange::toJSONString: unreadable range");
    const auto json = "{\"start\":" + TimeObject(range.start) + ",\"end\":" + TimeObject(range.end) + "}";
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(json.data()), json.size());
}
