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
 *
 * Derived from Source/WTF/wtf/MediaTime.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/WTF/wtf/MediaTime.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfHelperExports.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::MediaTime;

bool IsInvalid(const MediaTime& time) {
    return !(time.timeFlags & MediaTime::Valid);
}

bool IsPositiveInfinite(const MediaTime& time) {
    return time.timeFlags & MediaTime::PositiveInfinite;
}

bool IsNegativeInfinite(const MediaTime& time) {
    return time.timeFlags & MediaTime::NegativeInfinite;
}

bool IsIndefinite(const MediaTime& time) {
    return time.timeFlags & MediaTime::Indefinite;
}

bool HasDoubleValue(const MediaTime& time) {
    return time.timeFlags & MediaTime::DoubleValue;
}

double ToDouble(const MediaTime& time) {
    if (IsInvalid(time) || IsIndefinite(time))
        return std::numeric_limits<double>::quiet_NaN();
    if (IsPositiveInfinite(time))
        return std::numeric_limits<double>::infinity();
    if (IsNegativeInfinite(time))
        return -std::numeric_limits<double>::infinity();
    if (HasDoubleValue(time))
        return time.timeValueAsDouble;
    return static_cast<double>(time.timeValue) / time.timeScale;
}

std::int64_t WrappingMultiply(std::int64_t value, std::uint32_t scale) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(value) * scale);
}

MediaTime::ComparisonFlags Compare(const MediaTime& lhs, const MediaTime& rhs) {
    auto andFlags = lhs.timeFlags & rhs.timeFlags;
    if (andFlags & (MediaTime::PositiveInfinite | MediaTime::NegativeInfinite | MediaTime::Indefinite))
        return MediaTime::EqualTo;

    auto orFlags = lhs.timeFlags | rhs.timeFlags;
    if (!(orFlags & MediaTime::Valid))
        return MediaTime::EqualTo;

    if (!(andFlags & MediaTime::Valid))
        return IsInvalid(lhs) ? MediaTime::GreaterThan : MediaTime::LessThan;

    if (orFlags & MediaTime::NegativeInfinite)
        return IsNegativeInfinite(lhs) ? MediaTime::LessThan : MediaTime::GreaterThan;

    if (orFlags & MediaTime::PositiveInfinite)
        return IsPositiveInfinite(lhs) ? MediaTime::GreaterThan : MediaTime::LessThan;

    if (orFlags & MediaTime::Indefinite)
        return IsIndefinite(lhs) ? MediaTime::GreaterThan : MediaTime::LessThan;

    if (andFlags & MediaTime::DoubleValue) {
        if (lhs.timeValueAsDouble == rhs.timeValueAsDouble)
            return MediaTime::EqualTo;

        return lhs.timeValueAsDouble < rhs.timeValueAsDouble ? MediaTime::LessThan : MediaTime::GreaterThan;
    }

    if (orFlags & MediaTime::DoubleValue) {
        double a = ToDouble(lhs);
        double b = ToDouble(rhs);
        if (a > b)
            return MediaTime::GreaterThan;
        if (a < b)
            return MediaTime::LessThan;
        return MediaTime::EqualTo;
    }

    if ((lhs.timeValue < 0) != (rhs.timeValue < 0))
        return lhs.timeValue < 0 ? MediaTime::LessThan : MediaTime::GreaterThan;

    if (!lhs.timeValue && !rhs.timeValue)
        return MediaTime::EqualTo;

    if (lhs.timeScale == rhs.timeScale) {
        if (lhs.timeValue == rhs.timeValue)
            return MediaTime::EqualTo;
        return lhs.timeValue < rhs.timeValue ? MediaTime::LessThan : MediaTime::GreaterThan;
    }

    if (lhs.timeValue == rhs.timeValue)
        return lhs.timeScale < rhs.timeScale ? MediaTime::GreaterThan : MediaTime::LessThan;

    if (lhs.timeValue >= 0) {
        if (lhs.timeValue < rhs.timeValue && lhs.timeScale > rhs.timeScale)
            return MediaTime::LessThan;

        if (lhs.timeValue > rhs.timeValue && lhs.timeScale < rhs.timeScale)
            return MediaTime::GreaterThan;
    } else {
        if (lhs.timeValue < rhs.timeValue && lhs.timeScale < rhs.timeScale)
            return MediaTime::LessThan;

        if (lhs.timeValue > rhs.timeValue && lhs.timeScale > rhs.timeScale)
            return MediaTime::GreaterThan;
    }

    std::int64_t lhsFactor;
    std::int64_t rhsFactor;
    if (!__builtin_mul_overflow(lhs.timeValue, static_cast<std::int64_t>(rhs.timeScale), &lhsFactor)
        && !__builtin_mul_overflow(rhs.timeValue, static_cast<std::int64_t>(lhs.timeScale), &rhsFactor)) {
        if (lhsFactor == rhsFactor)
            return MediaTime::EqualTo;
        return lhsFactor < rhsFactor ? MediaTime::LessThan : MediaTime::GreaterThan;
    }

    if (!lhs.timeScale || !rhs.timeScale)
        throw std::invalid_argument("WTF::MediaTime::compare: zero time scale");

    std::int64_t rhsWhole = rhs.timeValue / rhs.timeScale;
    std::int64_t lhsWhole = lhs.timeValue / lhs.timeScale;
    if (lhsWhole > rhsWhole)
        return MediaTime::GreaterThan;
    if (lhsWhole < rhsWhole)
        return MediaTime::LessThan;

    std::int64_t rhsRemain = rhs.timeValue % rhs.timeScale;
    std::int64_t lhsRemain = lhs.timeValue % lhs.timeScale;
    lhsFactor = WrappingMultiply(lhsRemain, rhs.timeScale);
    rhsFactor = WrappingMultiply(rhsRemain, lhs.timeScale);

    if (lhsFactor == rhsFactor)
        return MediaTime::EqualTo;
    return lhsFactor > rhsFactor ? MediaTime::GreaterThan : MediaTime::LessThan;
}

} // namespace

extern "C" {

bool APS5_VABI _ZNK3WTF9MediaTimeneERKS0_(const MediaTime* self, const MediaTime* rhs) {
    if (!self || !rhs) throw std::invalid_argument("WTF::MediaTime::operator!=: null object");
    return Compare(*self, *rhs) != MediaTime::EqualTo;
}

}
