#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace {

uint32_t greatestCommonDivisor(uint32_t a, uint32_t b) {
    while (b) {
        uint32_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

bool leastCommonMultiple(uint32_t a, uint32_t b, uint32_t& result) {
    if (!a || !b) {
        return false;
    }
    uint32_t gcd = greatestCommonDivisor(a, b);
    uint32_t bDivGcd = b / gcd;
    return !__builtin_mul_overflow(a, bDivGcd, &result);
}

int64_t signum(int64_t val) {
    return (0 < val) - (val < 0);
}

constexpr double maxPlusOneInt64 = 9223372036854775808.0;

bool mediaTimeIsValid(const WTF::MediaTime& t) {
    return (t.m_timeFlags & WTF::MediaTime::Valid) != 0;
}

bool mediaTimeIsInvalid(const WTF::MediaTime& t) {
    return !mediaTimeIsValid(t);
}

bool mediaTimeHasBeenRounded(const WTF::MediaTime& t) {
    return (t.m_timeFlags & WTF::MediaTime::HasBeenRounded) != 0;
}

bool mediaTimeHasDoubleValue(const WTF::MediaTime& t) {
    return (t.m_timeFlags & WTF::MediaTime::DoubleValue) != 0;
}

bool mediaTimeIsPositiveInfinite(const WTF::MediaTime& t) {
    return (t.m_timeFlags & WTF::MediaTime::PositiveInfinite) != 0;
}

bool mediaTimeIsNegativeInfinite(const WTF::MediaTime& t) {
    return (t.m_timeFlags & WTF::MediaTime::NegativeInfinite) != 0;
}

bool mediaTimeIsIndefinite(const WTF::MediaTime& t) {
    return (t.m_timeFlags & WTF::MediaTime::Indefinite) != 0;
}

double mediaTimeToDouble(const WTF::MediaTime& t) {
    if (mediaTimeIsInvalid(t) || mediaTimeIsIndefinite(t)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (mediaTimeIsPositiveInfinite(t)) {
        return std::numeric_limits<double>::infinity();
    }
    if (mediaTimeIsNegativeInfinite(t)) {
        return -std::numeric_limits<double>::infinity();
    }
    if (mediaTimeHasDoubleValue(t)) {
        return t.m_timeValueAsDouble;
    }
    return static_cast<double>(t.m_timeValue) / t.m_timeScale;
}

float mediaTimeToFloat(const WTF::MediaTime& t) {
    if (mediaTimeIsInvalid(t) || mediaTimeIsIndefinite(t)) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (mediaTimeIsPositiveInfinite(t)) {
        return std::numeric_limits<float>::infinity();
    }
    if (mediaTimeIsNegativeInfinite(t)) {
        return -std::numeric_limits<float>::infinity();
    }
    if (mediaTimeHasDoubleValue(t)) {
        return static_cast<float>(t.m_timeValueAsDouble);
    }
    return static_cast<float>(t.m_timeValue) / t.m_timeScale;
}

bool rangeIsPointInRange(const WebCore::PlatformTimeRanges::Range& r, const WTF::MediaTime& point) {
    return _ZNK3WTF9MediaTimeleERKS0_(&r.start, point) && _ZNK3WTF9MediaTimeltERKS0_(&point, r.end);
}

bool rangeIsOverlappingRange(const WebCore::PlatformTimeRanges::Range& a, const WebCore::PlatformTimeRanges::Range& b) {
    return rangeIsPointInRange(a, b.start) || rangeIsPointInRange(a, b.end) || rangeIsPointInRange(b, a.start);
}

bool rangeIsContiguousWithRange(const WebCore::PlatformTimeRanges::Range& a, const WebCore::PlatformTimeRanges::Range& b) {
    return _ZNK3WTF9MediaTimeeqERKS0_(&a.end, b.start) || _ZNK3WTF9MediaTimeeqERKS0_(&b.end, a.start);
}

WebCore::PlatformTimeRanges::Range rangeUnionWithOverlappingOrContiguousRange(const WebCore::PlatformTimeRanges::Range& a, const WebCore::PlatformTimeRanges::Range& b) {
    WebCore::PlatformTimeRanges::Range ret;
    ret.start = _ZNK3WTF9MediaTimeltERKS0_(&a.start, b.start) ? a.start : b.start;
    ret.end = _ZNK3WTF9MediaTimegtERKS0_(&a.end, b.end) ? a.end : b.end;
    return ret;
}

bool rangeIsBeforeRange(const WebCore::PlatformTimeRanges::Range& a, const WebCore::PlatformTimeRanges::Range& b) {
    return _ZNK3WTF9MediaTimegeERKS0_(&b.start, a.end);
}

} // namespace

void WebCore::TimeRanges::deref() const {
    if (--m_refCount == 0) {
        this->~TimeRanges();
        _ZN3WTF8fastFreeEPv(const_cast<TimeRanges*>(this));
    }
}

extern "C" {

const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime11invalidTimeEv() {
    static const WTF::MediaTime time{-1, 1, 0};
    return time;
}

const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime8zeroTimeEv() {
    static const WTF::MediaTime time{0, 1, WTF::MediaTime::Valid};
    return time;
}

const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime20positiveInfiniteTimeEv() {
    static const WTF::MediaTime time{0, 1, WTF::MediaTime::PositiveInfinite | WTF::MediaTime::Valid};
    return time;
}

const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime20negativeInfiniteTimeEv() {
    static const WTF::MediaTime time{-1, 1, WTF::MediaTime::NegativeInfinite | WTF::MediaTime::Valid};
    return time;
}

const WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTime14indefiniteTimeEv() {
    static const WTF::MediaTime time{0, 1, WTF::MediaTime::Indefinite | WTF::MediaTime::Valid};
    return time;
}

WTF::MediaTime APS5_VABI _ZN3WTF9MediaTime15createWithFloatEf(float floatTime) {
    if (std::isnan(floatTime)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (std::isinf(floatTime)) {
        return std::signbit(floatTime) ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    WTF::MediaTime value{};
    value.m_timeValueAsDouble = floatTime;
    value.m_timeScale = WTF::MediaTime::DefaultTimeScale;
    value.m_timeFlags = WTF::MediaTime::Valid | WTF::MediaTime::DoubleValue;
    return value;
}

WTF::MediaTime APS5_VABI _ZN3WTF9MediaTime15createWithFloatEfj(float floatTime, uint32_t timeScale) {
    if (std::isnan(floatTime)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (std::isinf(floatTime)) {
        return std::signbit(floatTime) ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (floatTime >= maxPlusOneInt64) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (floatTime < -maxPlusOneInt64) {
        return _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (!timeScale) {
        return std::signbit(floatTime) ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    while (floatTime * timeScale >= maxPlusOneInt64) {
        timeScale /= 2;
    }
    WTF::MediaTime value{};
    value.m_timeValue = static_cast<int64_t>(floatTime * timeScale);
    value.m_timeScale = timeScale;
    value.m_timeFlags = WTF::MediaTime::Valid;
    return value;
}

WTF::MediaTime APS5_VABI _ZN3WTF9MediaTime16createWithDoubleEd(double doubleTime) {
    if (std::isnan(doubleTime)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (std::isinf(doubleTime)) {
        return std::signbit(doubleTime) ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    WTF::MediaTime value{};
    value.m_timeValueAsDouble = doubleTime;
    value.m_timeScale = WTF::MediaTime::DefaultTimeScale;
    value.m_timeFlags = WTF::MediaTime::Valid | WTF::MediaTime::DoubleValue;
    return value;
}

WTF::MediaTime APS5_VABI _ZN3WTF9MediaTime16createWithDoubleEdj(double doubleTime, uint32_t timeScale) {
    if (std::isnan(doubleTime)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (std::isinf(doubleTime)) {
        return std::signbit(doubleTime) ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (doubleTime >= maxPlusOneInt64) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (doubleTime < -maxPlusOneInt64) {
        return _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (!timeScale) {
        return std::signbit(doubleTime) ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    while (doubleTime * timeScale >= maxPlusOneInt64) {
        timeScale /= 2;
    }
    WTF::MediaTime value{};
    value.m_timeValue = static_cast<int64_t>(std::round(doubleTime * timeScale));
    value.m_timeScale = timeScale;
    value.m_timeFlags = WTF::MediaTime::Valid;
    return value;
}

void APS5_VABI _ZN3WTF9MediaTimeC1Ev(WTF::MediaTime* self) {
    self->m_timeValue = 0;
    self->m_timeScale = WTF::MediaTime::DefaultTimeScale;
    self->m_timeFlags = WTF::MediaTime::Valid;
}

void APS5_VABI _ZN3WTF9MediaTimeC2Ev(WTF::MediaTime* self) {
    _ZN3WTF9MediaTimeC1Ev(self);
}

void APS5_VABI _ZN3WTF9MediaTimeC1ERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    self->m_timeValue = rhs.m_timeValue;
    self->m_timeScale = rhs.m_timeScale;
    self->m_timeFlags = rhs.m_timeFlags;
}

void APS5_VABI _ZN3WTF9MediaTimeC2ERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    _ZN3WTF9MediaTimeC1ERKS0_(self, rhs);
}

void APS5_VABI _ZN3WTF9MediaTimeC1Eljh(WTF::MediaTime* self, int64_t value, uint32_t scale, uint8_t flags) {
    self->m_timeValue = value;
    self->m_timeScale = scale;
    self->m_timeFlags = flags;
    if (scale || !(flags & WTF::MediaTime::Valid)) {
        return;
    }
    if (value < 0) {
        self->m_timeValue = -1;
        self->m_timeScale = 1;
        self->m_timeFlags = WTF::MediaTime::NegativeInfinite | WTF::MediaTime::Valid;
    } else {
        self->m_timeValue = 0;
        self->m_timeScale = 1;
        self->m_timeFlags = WTF::MediaTime::PositiveInfinite | WTF::MediaTime::Valid;
    }
}

void APS5_VABI _ZN3WTF9MediaTimeC2Eljh(WTF::MediaTime* self, int64_t value, uint32_t scale, uint8_t flags) {
    _ZN3WTF9MediaTimeC1Eljh(self, value, scale, flags);
}

WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTimeaSERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    self->m_timeValue = rhs.m_timeValue;
    self->m_timeScale = rhs.m_timeScale;
    self->m_timeFlags = rhs.m_timeFlags;
    return *self;
}

void APS5_VABI _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(WTF::MediaTime* self, uint32_t timeScale, WTF::MediaTime::RoundingFlags flags) {
    if (mediaTimeHasDoubleValue(*self)) {
        *self = _ZN3WTF9MediaTime16createWithDoubleEdj(self->m_timeValueAsDouble, timeScale);
        return;
    }

    if (!timeScale) {
        *self = self->m_timeValue < 0 ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
        return;
    }

    if (timeScale == self->m_timeScale) {
        return;
    }

    timeScale = std::min(WTF::MediaTime::MaximumTimeScale, timeScale);

    __int128_t newValue = static_cast<__int128_t>(self->m_timeValue) * timeScale;
    int64_t remainder = static_cast<int64_t>(newValue % self->m_timeScale);
    newValue = newValue / self->m_timeScale;

    if (newValue < std::numeric_limits<int64_t>::min()) {
        *self = _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
        return;
    }

    if (newValue > std::numeric_limits<int64_t>::max()) {
        *self = _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
        return;
    }

    self->m_timeValue = static_cast<int64_t>(newValue);
    std::swap(self->m_timeScale, timeScale);

    if (!remainder) {
        return;
    }

    self->m_timeFlags |= WTF::MediaTime::HasBeenRounded;
    switch (flags) {
    case WTF::MediaTime::RoundingFlags::HalfAwayFromZero:
        if (static_cast<int64_t>(std::abs(remainder)) * 2 >= static_cast<int64_t>(timeScale)) {
            if (remainder < 0) {
                self->m_timeValue--;
            } else {
                self->m_timeValue++;
            }
        }
        break;

    case WTF::MediaTime::RoundingFlags::TowardZero:
        break;

    case WTF::MediaTime::RoundingFlags::AwayFromZero:
        if (remainder < 0) {
            self->m_timeValue--;
        } else {
            self->m_timeValue++;
        }
        break;

    case WTF::MediaTime::RoundingFlags::TowardPositiveInfinity:
        if (remainder > 0) {
            self->m_timeValue++;
        }
        break;

    case WTF::MediaTime::RoundingFlags::TowardNegativeInfinity:
        if (remainder < 0) {
            self->m_timeValue--;
        }
        break;
    }
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(const WTF::MediaTime* self, uint32_t scale, WTF::MediaTime::RoundingFlags flags) {
    WTF::MediaTime result = *self;
    _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&result, scale, flags);
    return result;
}

WTF::MediaTime::ComparisonFlags APS5_VABI _ZNK3WTF9MediaTime7compareERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    auto andFlags = self->m_timeFlags & rhs.m_timeFlags;
    if (andFlags & (WTF::MediaTime::PositiveInfinite | WTF::MediaTime::NegativeInfinite | WTF::MediaTime::Indefinite)) {
        return WTF::MediaTime::EqualTo;
    }

    auto orFlags = self->m_timeFlags | rhs.m_timeFlags;
    if (!(orFlags & WTF::MediaTime::Valid)) {
        return WTF::MediaTime::EqualTo;
    }

    if (!(andFlags & WTF::MediaTime::Valid)) {
        return mediaTimeIsInvalid(*self) ? WTF::MediaTime::GreaterThan : WTF::MediaTime::LessThan;
    }

    if (orFlags & WTF::MediaTime::NegativeInfinite) {
        return mediaTimeIsNegativeInfinite(*self) ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    if (orFlags & WTF::MediaTime::PositiveInfinite) {
        return mediaTimeIsPositiveInfinite(*self) ? WTF::MediaTime::GreaterThan : WTF::MediaTime::LessThan;
    }

    if (orFlags & WTF::MediaTime::Indefinite) {
        return mediaTimeIsIndefinite(*self) ? WTF::MediaTime::GreaterThan : WTF::MediaTime::LessThan;
    }

    if (andFlags & WTF::MediaTime::DoubleValue) {
        if (self->m_timeValueAsDouble == rhs.m_timeValueAsDouble) {
            return WTF::MediaTime::EqualTo;
        }
        return self->m_timeValueAsDouble < rhs.m_timeValueAsDouble ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    if (orFlags & WTF::MediaTime::DoubleValue) {
        double a = mediaTimeToDouble(*self);
        double b = mediaTimeToDouble(rhs);
        if (a > b) {
            return WTF::MediaTime::GreaterThan;
        }
        if (a < b) {
            return WTF::MediaTime::LessThan;
        }
        return WTF::MediaTime::EqualTo;
    }

    if ((self->m_timeValue < 0) != (rhs.m_timeValue < 0)) {
        return self->m_timeValue < 0 ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    if (!self->m_timeValue && !rhs.m_timeValue) {
        return WTF::MediaTime::EqualTo;
    }

    if (self->m_timeScale == rhs.m_timeScale) {
        if (self->m_timeValue == rhs.m_timeValue) {
            return WTF::MediaTime::EqualTo;
        }
        return self->m_timeValue < rhs.m_timeValue ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    if (self->m_timeValue == rhs.m_timeValue) {
        return self->m_timeScale < rhs.m_timeScale ? WTF::MediaTime::GreaterThan : WTF::MediaTime::LessThan;
    }

    if (self->m_timeValue >= 0) {
        if (self->m_timeValue < rhs.m_timeValue && self->m_timeScale > rhs.m_timeScale) {
            return WTF::MediaTime::LessThan;
        }
        if (self->m_timeValue > rhs.m_timeValue && self->m_timeScale < rhs.m_timeScale) {
            return WTF::MediaTime::GreaterThan;
        }
    } else {
        if (self->m_timeValue < rhs.m_timeValue && self->m_timeScale < rhs.m_timeScale) {
            return WTF::MediaTime::LessThan;
        }
        if (self->m_timeValue > rhs.m_timeValue && self->m_timeScale > rhs.m_timeScale) {
            return WTF::MediaTime::GreaterThan;
        }
    }

    int64_t lhsFactor;
    int64_t rhsFactor;
    if (!__builtin_mul_overflow(self->m_timeValue, static_cast<int64_t>(rhs.m_timeScale), &lhsFactor)
        && !__builtin_mul_overflow(rhs.m_timeValue, static_cast<int64_t>(self->m_timeScale), &rhsFactor)) {
        if (lhsFactor == rhsFactor) {
            return WTF::MediaTime::EqualTo;
        }
        return lhsFactor < rhsFactor ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    int64_t rhsWhole = rhs.m_timeValue / rhs.m_timeScale;
    int64_t lhsWhole = self->m_timeValue / self->m_timeScale;
    if (lhsWhole > rhsWhole) {
        return WTF::MediaTime::GreaterThan;
    }
    if (lhsWhole < rhsWhole) {
        return WTF::MediaTime::LessThan;
    }

    int64_t rhsRemain = rhs.m_timeValue % rhs.m_timeScale;
    int64_t lhsRemain = self->m_timeValue % self->m_timeScale;
    lhsFactor = lhsRemain * rhs.m_timeScale;
    rhsFactor = rhsRemain * self->m_timeScale;

    if (lhsFactor == rhsFactor) {
        return WTF::MediaTime::EqualTo;
    }
    return lhsFactor > rhsFactor ? WTF::MediaTime::GreaterThan : WTF::MediaTime::LessThan;
}

bool APS5_VABI _ZNK3WTF9MediaTimeeqERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    return _ZNK3WTF9MediaTime7compareERKS0_(self, rhs) == WTF::MediaTime::EqualTo;
}

bool APS5_VABI _ZNK3WTF9MediaTimeltERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    return _ZNK3WTF9MediaTime7compareERKS0_(self, rhs) == WTF::MediaTime::LessThan;
}

bool APS5_VABI _ZNK3WTF9MediaTimegtERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    return _ZNK3WTF9MediaTime7compareERKS0_(self, rhs) == WTF::MediaTime::GreaterThan;
}

bool APS5_VABI _ZNK3WTF9MediaTimeleERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    return _ZNK3WTF9MediaTime7compareERKS0_(self, rhs) <= WTF::MediaTime::EqualTo;
}

bool APS5_VABI _ZNK3WTF9MediaTimegeERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    return _ZNK3WTF9MediaTime7compareERKS0_(self, rhs) >= WTF::MediaTime::EqualTo;
}

bool APS5_VABI _ZNK3WTF9MediaTime9isBetweenERKS0_S2_(const WTF::MediaTime* self, const WTF::MediaTime& a, const WTF::MediaTime& b) {
    if (_ZNK3WTF9MediaTimegtERKS0_(&a, b)) {
        return _ZNK3WTF9MediaTimegtERKS0_(self, b) && _ZNK3WTF9MediaTimeltERKS0_(self, a);
    }
    return _ZNK3WTF9MediaTimegtERKS0_(self, a) && _ZNK3WTF9MediaTimeltERKS0_(self, b);
}

bool APS5_VABI _ZNK3WTF9MediaTimentEv(const WTF::MediaTime* self) {
    return (self->m_timeFlags == WTF::MediaTime::Valid && !self->m_timeValue)
        || (self->m_timeFlags == (WTF::MediaTime::Valid | WTF::MediaTime::DoubleValue) && !self->m_timeValueAsDouble)
        || mediaTimeIsInvalid(*self);
}

bool APS5_VABI _ZNK3WTF9MediaTimecvbEv(const WTF::MediaTime* self) {
    return !_ZNK3WTF9MediaTimentEv(self);
}

bool APS5_VABI _ZNK3WTF9MediaTime12isIndefiniteEv(const WTF::MediaTime* self) {
    return mediaTimeIsIndefinite(*self);
}

bool APS5_VABI _ZNK3WTF9MediaTime14hasBeenRoundedEv(const WTF::MediaTime* self) {
    return mediaTimeHasBeenRounded(*self);
}

bool APS5_VABI _ZNK3WTF9MediaTime14hasDoubleValueEv(const WTF::MediaTime* self) {
    return mediaTimeHasDoubleValue(*self);
}

bool APS5_VABI _ZNK3WTF9MediaTime18isNegativeInfiniteEv(const WTF::MediaTime* self) {
    return mediaTimeIsNegativeInfinite(*self);
}

bool APS5_VABI _ZNK3WTF9MediaTime18isPositiveInfiniteEv(const WTF::MediaTime* self) {
    return mediaTimeIsPositiveInfinite(*self);
}

bool APS5_VABI _ZNK3WTF9MediaTime7isValidEv(const WTF::MediaTime* self) {
    return mediaTimeIsValid(*self);
}

bool APS5_VABI _ZNK3WTF9MediaTime9isInvalidEv(const WTF::MediaTime* self) {
    return mediaTimeIsInvalid(*self);
}

float APS5_VABI _ZNK3WTF9MediaTime7toFloatEv(const WTF::MediaTime* self) {
    return mediaTimeToFloat(*self);
}

double APS5_VABI _ZNK3WTF9MediaTime8toDoubleEv(const WTF::MediaTime* self) {
    return mediaTimeToDouble(*self);
}

uint8_t APS5_VABI _ZNK3WTF9MediaTime9timeFlagsEv(const WTF::MediaTime* self) {
    return self->m_timeFlags;
}

const uint32_t& APS5_VABI _ZNK3WTF9MediaTime9timeScaleEv(const WTF::MediaTime* self) {
    return self->m_timeScale;
}

const int64_t& APS5_VABI _ZNK3WTF9MediaTime9timeValueEv(const WTF::MediaTime* self) {
    return self->m_timeValue;
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimeplERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    if (mediaTimeIsInvalid(rhs) || mediaTimeIsInvalid(*self)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsIndefinite(rhs) || mediaTimeIsIndefinite(*self)) {
        return _ZN3WTF9MediaTime14indefiniteTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(*self) && mediaTimeIsNegativeInfinite(rhs)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(*self) && mediaTimeIsPositiveInfinite(rhs)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(*self) || mediaTimeIsPositiveInfinite(rhs)) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(*self) || mediaTimeIsNegativeInfinite(rhs)) {
        return _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (mediaTimeHasDoubleValue(*self) && mediaTimeHasDoubleValue(rhs)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(self->m_timeValueAsDouble + rhs.m_timeValueAsDouble);
    }
    if (mediaTimeHasDoubleValue(*self) || mediaTimeHasDoubleValue(rhs)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(mediaTimeToDouble(*self) + mediaTimeToDouble(rhs));
    }

    WTF::MediaTime a = *self;
    WTF::MediaTime b = rhs;

    uint32_t commonTimeScale;
    if (!leastCommonMultiple(a.m_timeScale, b.m_timeScale, commonTimeScale) || commonTimeScale > WTF::MediaTime::MaximumTimeScale) {
        commonTimeScale = WTF::MediaTime::MaximumTimeScale;
    }
    _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&a, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&b, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);

    while (__builtin_add_overflow(a.m_timeValue, b.m_timeValue, &a.m_timeValue)) {
        if (commonTimeScale == 1) {
            return a.m_timeValue > 0 ? _ZN3WTF9MediaTime20positiveInfiniteTimeEv() : _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
        }
        commonTimeScale /= 2;
        _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&a, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
        _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&b, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    }
    return a;
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimemiERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    if (mediaTimeIsInvalid(rhs) || mediaTimeIsInvalid(*self)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsIndefinite(rhs) || mediaTimeIsIndefinite(*self)) {
        return _ZN3WTF9MediaTime14indefiniteTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(*self) && mediaTimeIsPositiveInfinite(rhs)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(*self) && mediaTimeIsNegativeInfinite(rhs)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(*self) || mediaTimeIsNegativeInfinite(rhs)) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(*self) || mediaTimeIsPositiveInfinite(rhs)) {
        return _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (mediaTimeHasDoubleValue(*self) && mediaTimeHasDoubleValue(rhs)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(self->m_timeValueAsDouble - rhs.m_timeValueAsDouble);
    }
    if (mediaTimeHasDoubleValue(*self) || mediaTimeHasDoubleValue(rhs)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(mediaTimeToDouble(*self) - mediaTimeToDouble(rhs));
    }

    WTF::MediaTime a = *self;
    WTF::MediaTime b = rhs;

    uint32_t commonTimeScale;
    if (!leastCommonMultiple(self->m_timeScale, rhs.m_timeScale, commonTimeScale) || commonTimeScale > WTF::MediaTime::MaximumTimeScale) {
        commonTimeScale = WTF::MediaTime::MaximumTimeScale;
    }
    _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&a, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&b, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);

    while (__builtin_sub_overflow(a.m_timeValue, b.m_timeValue, &a.m_timeValue)) {
        if (commonTimeScale == 1) {
            return a.m_timeValue > 0 ? _ZN3WTF9MediaTime20positiveInfiniteTimeEv() : _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
        }
        commonTimeScale /= 2;
        _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&a, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
        _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&b, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    }
    return a;
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimengEv(const WTF::MediaTime* self) {
    if (mediaTimeIsInvalid(*self)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsIndefinite(*self)) {
        return _ZN3WTF9MediaTime14indefiniteTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(*self)) {
        return _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(*self)) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    WTF::MediaTime negativeTime = *self;
    if (mediaTimeHasDoubleValue(negativeTime)) {
        negativeTime.m_timeValueAsDouble = -negativeTime.m_timeValueAsDouble;
    } else {
        negativeTime.m_timeValue = -negativeTime.m_timeValue;
    }
    return negativeTime;
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimemlEi(const WTF::MediaTime* self, int32_t rhs) {
    if (mediaTimeIsInvalid(*self)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsIndefinite(*self)) {
        return _ZN3WTF9MediaTime14indefiniteTimeEv();
    }
    if (!rhs) {
        return _ZN3WTF9MediaTime8zeroTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(*self)) {
        return rhs > 0 ? _ZN3WTF9MediaTime20positiveInfiniteTimeEv() : _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(*self)) {
        return rhs > 0 ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (mediaTimeHasDoubleValue(*self)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(self->m_timeValueAsDouble * rhs);
    }
    WTF::MediaTime a = *self;
    while (__builtin_mul_overflow(a.m_timeValue, rhs, &a.m_timeValue)) {
        if (a.m_timeScale == 1) {
            return signum(a.m_timeValue) == signum(rhs) ? _ZN3WTF9MediaTime20positiveInfiniteTimeEv() : _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
        }
        _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(&a, a.m_timeScale / 2, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    }
    return a;
}

WTF::MediaTime APS5_VABI _ZN3WTF3absERKNS_9MediaTimeE(const WTF::MediaTime& rhs) {
    if (mediaTimeIsInvalid(rhs)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(rhs) || mediaTimeIsPositiveInfinite(rhs)) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (mediaTimeHasDoubleValue(rhs)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(std::abs(rhs.m_timeValueAsDouble));
    }
    WTF::MediaTime val = rhs;
    val.m_timeValue = std::abs(rhs.m_timeValue);
    return val;
}

WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTimepLERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    *self = _ZNK3WTF9MediaTimeplERKS0_(self, rhs);
    return *self;
}

WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTimemIERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    *self = _ZNK3WTF9MediaTimemiERKS0_(self, rhs);
    return *self;
}


void* APS5_VABI _ZN3WTF9MediaTimenwEmPv(std::size_t, void* ptr) {
    return ptr;
}

void* APS5_VABI _ZN3WTF9MediaTimenwEm10NotNullTagPv(std::size_t, WTF::NotNullTag, void* ptr) {
    return ptr;
}

void* APS5_VABI _ZN3WTF9MediaTimenaEmPv(std::size_t, void* ptr) {
    return ptr;
}

void* APS5_VABI _ZN7WebCore18PlatformTimeRangesnwEmPv(std::size_t, void* ptr) {
    return ptr;
}

void* APS5_VABI _ZN7WebCore18PlatformTimeRangesnwEm10NotNullTagPv(std::size_t, WTF::NotNullTag, void* ptr) {
    return ptr;
}

void* APS5_VABI _ZN7WebCore18PlatformTimeRangesnaEmPv(std::size_t, void* ptr) {
    return ptr;
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesC1Ev(WebCore::PlatformTimeRanges* self) {
    self->m_ranges.m_buffer = nullptr;
    self->m_ranges.m_capacity = 0;
    self->m_ranges.m_size = 0;
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesC2Ev(WebCore::PlatformTimeRanges* self) {
    _ZN7WebCore18PlatformTimeRangesC1Ev(self);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesC1ERKN3WTF9MediaTimeES4_(WebCore::PlatformTimeRanges* self, const WTF::MediaTime& start, const WTF::MediaTime& end) {
    _ZN7WebCore18PlatformTimeRangesC1Ev(self);
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(self, start, end);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesC2ERKN3WTF9MediaTimeES4_(WebCore::PlatformTimeRanges* self, const WTF::MediaTime& start, const WTF::MediaTime& end) {
    _ZN7WebCore18PlatformTimeRangesC1ERKN3WTF9MediaTimeES4_(self, start, end);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesC1ERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other) {
    _ZN7WebCore18PlatformTimeRangesC1Ev(self);
    _ZN7WebCore18PlatformTimeRangesaSERKS0_(self, other);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesC2ERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other) {
    _ZN7WebCore18PlatformTimeRangesC1ERKS0_(self, other);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesC1EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(WebCore::PlatformTimeRanges* self, WebCore::PlatformTimeRanges::VectorRange* ranges) {
    self->m_ranges.m_buffer = std::exchange(ranges->m_buffer, nullptr);
    self->m_ranges.m_capacity = std::exchange(ranges->m_capacity, 0);
    self->m_ranges.m_size = std::exchange(ranges->m_size, 0);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesC2EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(WebCore::PlatformTimeRanges* self, WebCore::PlatformTimeRanges::VectorRange* ranges) {
    _ZN7WebCore18PlatformTimeRangesC1EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(self, ranges);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesD1Ev(WebCore::PlatformTimeRanges* self) {
    if (self->m_ranges.m_buffer) {
        _ZN3WTF8fastFreeEPv(self->m_ranges.m_buffer);
        self->m_ranges.m_buffer = nullptr;
    }
    self->m_ranges.m_capacity = 0;
    self->m_ranges.m_size = 0;
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesD2Ev(WebCore::PlatformTimeRanges* self) {
    _ZN7WebCore18PlatformTimeRangesD1Ev(self);
}

WebCore::PlatformTimeRanges& APS5_VABI _ZN7WebCore18PlatformTimeRangesaSERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other) {
    if (self != &other) {
        if (self->m_ranges.m_buffer) {
            _ZN3WTF8fastFreeEPv(self->m_ranges.m_buffer);
        }
        if (other.m_ranges.m_size) {
            self->m_ranges.m_buffer = static_cast<WebCore::PlatformTimeRanges::Range*>(_ZN3WTF10fastMallocEm(other.m_ranges.m_size * sizeof(WebCore::PlatformTimeRanges::Range)));
            std::memcpy(self->m_ranges.m_buffer, other.m_ranges.m_buffer, other.m_ranges.m_size * sizeof(WebCore::PlatformTimeRanges::Range));
            self->m_ranges.m_capacity = other.m_ranges.m_size;
            self->m_ranges.m_size = other.m_ranges.m_size;
        } else {
            self->m_ranges.m_buffer = nullptr;
            self->m_ranges.m_capacity = 0;
            self->m_ranges.m_size = 0;
        }
    }
    return *self;
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(WebCore::PlatformTimeRanges* self, const WTF::MediaTime& start, const WTF::MediaTime& end) {
    if (_ZNK3WTF9MediaTimegtERKS0_(&start, end)) {
        return;
    }

    WebCore::PlatformTimeRanges::Range addedRange{start, end};
    unsigned overlappingArcIndex = 0;

    for (overlappingArcIndex = 0; overlappingArcIndex < self->m_ranges.m_size; overlappingArcIndex++) {
        if (rangeIsOverlappingRange(addedRange, self->m_ranges.m_buffer[overlappingArcIndex])
            || rangeIsContiguousWithRange(addedRange, self->m_ranges.m_buffer[overlappingArcIndex])) {
            addedRange = rangeUnionWithOverlappingOrContiguousRange(addedRange, self->m_ranges.m_buffer[overlappingArcIndex]);
            for (unsigned j = overlappingArcIndex; j + 1 < self->m_ranges.m_size; ++j) {
                self->m_ranges.m_buffer[j] = self->m_ranges.m_buffer[j + 1];
            }
            --self->m_ranges.m_size;
            --overlappingArcIndex;
        } else {
            if (!overlappingArcIndex) {
                if (rangeIsBeforeRange(addedRange, self->m_ranges.m_buffer[0])) {
                    break;
                }
            } else {
                if (rangeIsBeforeRange(self->m_ranges.m_buffer[overlappingArcIndex - 1], addedRange)
                    && rangeIsBeforeRange(addedRange, self->m_ranges.m_buffer[overlappingArcIndex])) {
                    break;
                }
            }
        }
    }

    if (self->m_ranges.m_size == self->m_ranges.m_capacity) {
        unsigned newCapacity = self->m_ranges.m_capacity ? self->m_ranges.m_capacity * 2 : 4;
        auto* newBuf = static_cast<WebCore::PlatformTimeRanges::Range*>(_ZN3WTF10fastMallocEm(newCapacity * sizeof(WebCore::PlatformTimeRanges::Range)));
        if (self->m_ranges.m_buffer) {
            std::memcpy(newBuf, self->m_ranges.m_buffer, self->m_ranges.m_size * sizeof(WebCore::PlatformTimeRanges::Range));
            _ZN3WTF8fastFreeEPv(self->m_ranges.m_buffer);
        }
        self->m_ranges.m_buffer = newBuf;
        self->m_ranges.m_capacity = newCapacity;
    }

    for (unsigned j = self->m_ranges.m_size; j > overlappingArcIndex; --j) {
        self->m_ranges.m_buffer[j] = self->m_ranges.m_buffer[j - 1];
    }
    self->m_ranges.m_buffer[overlappingArcIndex] = addedRange;
    ++self->m_ranges.m_size;
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges5clearEv(WebCore::PlatformTimeRanges* self) {
    self->m_ranges.m_size = 0;
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges6invertEv(WebCore::PlatformTimeRanges* self) {
    WebCore::PlatformTimeRanges inverted;
    _ZN7WebCore18PlatformTimeRangesC1Ev(&inverted);
    WTF::MediaTime posInf = _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    WTF::MediaTime negInf = _ZN3WTF9MediaTime20negativeInfiniteTimeEv();

    if (!self->m_ranges.m_size) {
        _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&inverted, negInf, posInf);
    } else {
        WTF::MediaTime start = self->m_ranges.m_buffer[0].start;
        if (!_ZNK3WTF9MediaTimeeqERKS0_(&start, negInf)) {
            _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&inverted, negInf, start);
        }

        for (unsigned index = 0; index + 1 < self->m_ranges.m_size; ++index) {
            _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&inverted, self->m_ranges.m_buffer[index].end, self->m_ranges.m_buffer[index + 1].start);
        }

        WTF::MediaTime end = self->m_ranges.m_buffer[self->m_ranges.m_size - 1].end;
        if (!_ZNK3WTF9MediaTimeeqERKS0_(&end, posInf)) {
            _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&inverted, end, posInf);
        }
    }

    std::swap(self->m_ranges, inverted.m_ranges);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges9unionWithERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other) {
    WebCore::PlatformTimeRanges unioned;
    _ZN7WebCore18PlatformTimeRangesC1ERKS0_(&unioned, *self);
    for (unsigned index = 0; index < other.m_ranges.m_size; ++index) {
        const auto& range = other.m_ranges.m_buffer[index];
        _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&unioned, range.start, range.end);
    }
    std::swap(self->m_ranges, unioned.m_ranges);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other) {
    WebCore::PlatformTimeRanges invertedOther;
    _ZN7WebCore18PlatformTimeRangesC1ERKS0_(&invertedOther, other);
    _ZN7WebCore18PlatformTimeRanges6invertEv(&invertedOther);
    _ZN7WebCore18PlatformTimeRanges6invertEv(self);
    _ZN7WebCore18PlatformTimeRanges9unionWithERKS0_(self, invertedOther);
    _ZN7WebCore18PlatformTimeRanges6invertEv(self);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges5startEjRb(const WebCore::PlatformTimeRanges* self, unsigned int index, bool& valid) {
    if (index >= self->m_ranges.m_size) {
        valid = false;
        return _ZN3WTF9MediaTime8zeroTimeEv();
    }
    valid = true;
    return self->m_ranges.m_buffer[index].start;
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges5startEj(const WebCore::PlatformTimeRanges* self, unsigned int index) {
    bool ignored = false;
    return _ZNK7WebCore18PlatformTimeRanges5startEjRb(self, index, ignored);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges3endEjRb(const WebCore::PlatformTimeRanges* self, unsigned int index, bool& valid) {
    if (index >= self->m_ranges.m_size) {
        valid = false;
        return _ZN3WTF9MediaTime8zeroTimeEv();
    }
    valid = true;
    return self->m_ranges.m_buffer[index].end;
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges3endEj(const WebCore::PlatformTimeRanges* self, unsigned int index) {
    bool ignored = false;
    return _ZNK7WebCore18PlatformTimeRanges3endEjRb(self, index, ignored);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges8durationEj(const WebCore::PlatformTimeRanges* self, unsigned int index) {
    if (index >= self->m_ranges.m_size) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    return _ZNK3WTF9MediaTimemiERKS0_(&self->m_ranges.m_buffer[index].end, self->m_ranges.m_buffer[index].start);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges19maximumBufferedTimeEv(const WebCore::PlatformTimeRanges* self) {
    if (!self->m_ranges.m_size) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    return self->m_ranges.m_buffer[self->m_ranges.m_size - 1].end;
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges13totalDurationEv(const WebCore::PlatformTimeRanges* self) {
    WTF::MediaTime total = _ZN3WTF9MediaTime8zeroTimeEv();
    for (unsigned n = 0; n < self->m_ranges.m_size; ++n) {
        WTF::MediaTime diff = _ZNK3WTF9MediaTimemiERKS0_(&self->m_ranges.m_buffer[n].end, self->m_ranges.m_buffer[n].start);
        WTF::MediaTime absDiff = _ZN3WTF3absERKNS_9MediaTimeE(diff);
        total = _ZNK3WTF9MediaTimeplERKS0_(&total, absDiff);
    }
    return total;
}

std::size_t APS5_VABI _ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(const WebCore::PlatformTimeRanges* self, const WTF::MediaTime& time) {
    bool ignore = false;
    for (unsigned n = 0; n < self->m_ranges.m_size; ++n) {
        WTF::MediaTime s = _ZNK7WebCore18PlatformTimeRanges5startEjRb(self, n, ignore);
        WTF::MediaTime e = _ZNK7WebCore18PlatformTimeRanges3endEjRb(self, n, ignore);
        if (_ZNK3WTF9MediaTimegeERKS0_(&time, s) && _ZNK3WTF9MediaTimeleERKS0_(&time, e)) {
            return n;
        }
    }
    return static_cast<std::size_t>(-1);
}

bool APS5_VABI _ZNK7WebCore18PlatformTimeRanges7containERKN3WTF9MediaTimeE(const WebCore::PlatformTimeRanges* self, const WTF::MediaTime& time) {
    return _ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(self, time) != static_cast<std::size_t>(-1);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(const WebCore::PlatformTimeRanges* self, const WTF::MediaTime& time) {
    WTF::MediaTime closestDelta = _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    WTF::MediaTime closestTime = _ZN3WTF9MediaTime8zeroTimeEv();
    unsigned count = self->m_ranges.m_size;
    if (!count) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    bool ignore = false;
    for (unsigned ndx = 0; ndx < count; ++ndx) {
        WTF::MediaTime startTime = _ZNK7WebCore18PlatformTimeRanges5startEjRb(self, ndx, ignore);
        WTF::MediaTime endTime = _ZNK7WebCore18PlatformTimeRanges3endEjRb(self, ndx, ignore);
        if (_ZNK3WTF9MediaTimegeERKS0_(&time, startTime) && _ZNK3WTF9MediaTimeleERKS0_(&time, endTime)) {
            return time;
        }
        WTF::MediaTime startDiff = _ZNK3WTF9MediaTimemiERKS0_(&startTime, time);
        WTF::MediaTime startTimeDelta = _ZN3WTF3absERKNS_9MediaTimeE(startDiff);
        if (_ZNK3WTF9MediaTimeltERKS0_(&startTimeDelta, closestDelta)) {
            closestTime = startTime;
            closestDelta = startTimeDelta;
        }
        WTF::MediaTime endDiff = _ZNK3WTF9MediaTimemiERKS0_(&endTime, time);
        WTF::MediaTime endTimeDelta = _ZN3WTF3absERKNS_9MediaTimeE(endDiff);
        if (_ZNK3WTF9MediaTimeltERKS0_(&endTimeDelta, closestDelta)) {
            closestTime = endTime;
            closestDelta = endTimeDelta;
        }
    }
    return closestTime;
}

unsigned int APS5_VABI _ZNK7WebCore18PlatformTimeRanges6lengthEv(const WebCore::PlatformTimeRanges* self) {
    return self->m_ranges.m_size;
}

WebCore::Ref<WebCore::TimeRanges> APS5_VABI _ZN7WebCore10TimeRanges6createEv() {
    void* mem = _ZN3WTF10fastMallocEm(sizeof(WebCore::TimeRanges));
    auto* ptr = static_cast<WebCore::TimeRanges*>(mem);
    _ZN7WebCore10TimeRangesC1Ev(ptr);
    return WebCore::Ref<WebCore::TimeRanges>(*ptr);
}

WebCore::Ref<WebCore::TimeRanges> APS5_VABI _ZN7WebCore10TimeRanges6createEdd(double start, double end) {
    void* mem = _ZN3WTF10fastMallocEm(sizeof(WebCore::TimeRanges));
    auto* ptr = static_cast<WebCore::TimeRanges*>(mem);
    _ZN7WebCore10TimeRangesC1Edd(ptr, start, end);
    return WebCore::Ref<WebCore::TimeRanges>(*ptr);
}

void APS5_VABI _ZN7WebCore10TimeRangesC1Ev(WebCore::TimeRanges* self) {
    self->m_refCount = 1;
    _ZN7WebCore18PlatformTimeRangesC1Ev(&self->m_ranges);
}

void APS5_VABI _ZN7WebCore10TimeRangesC2Ev(WebCore::TimeRanges* self) {
    _ZN7WebCore10TimeRangesC1Ev(self);
}

void APS5_VABI _ZN7WebCore10TimeRangesC1Edd(WebCore::TimeRanges* self, double start, double end) {
    self->m_refCount = 1;
    WTF::MediaTime s = _ZN3WTF9MediaTime16createWithDoubleEd(start);
    WTF::MediaTime e = _ZN3WTF9MediaTime16createWithDoubleEd(end);
    _ZN7WebCore18PlatformTimeRangesC1ERKN3WTF9MediaTimeES4_(&self->m_ranges, s, e);
}

void APS5_VABI _ZN7WebCore10TimeRangesC2Edd(WebCore::TimeRanges* self, double start, double end) {
    _ZN7WebCore10TimeRangesC1Edd(self, start, end);
}

void APS5_VABI _ZN7WebCore10TimeRanges3addEdd(WebCore::TimeRanges* self, double start, double end) {
    WTF::MediaTime s = _ZN3WTF9MediaTime16createWithDoubleEd(start);
    WTF::MediaTime e = _ZN3WTF9MediaTime16createWithDoubleEd(end);
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(&self->m_ranges, s, e);
}

void APS5_VABI _ZN7WebCore10TimeRanges13intersectWithERKS0_(WebCore::TimeRanges* self, const WebCore::TimeRanges& other) {
    _ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_(&self->m_ranges, other.m_ranges);
}

unsigned int APS5_VABI _ZNK7WebCore10TimeRanges6lengthEv(const WebCore::TimeRanges* self) {
    return _ZNK7WebCore18PlatformTimeRanges6lengthEv(&self->m_ranges);
}

double APS5_VABI _ZNK7WebCore10TimeRanges7nearestEd(const WebCore::TimeRanges* self, double time) {
    WTF::MediaTime t = _ZN3WTF9MediaTime16createWithDoubleEd(time);
    WTF::MediaTime n = _ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(&self->m_ranges, t);
    return mediaTimeToDouble(n);
}

WebCore::Ref<WebCore::TimeRanges> APS5_VABI _ZNK7WebCore10TimeRanges4copyEv(const WebCore::TimeRanges* self) {
    void* mem = _ZN3WTF10fastMallocEm(sizeof(WebCore::TimeRanges));
    auto* ptr = static_cast<WebCore::TimeRanges*>(mem);
    ptr->m_refCount = 1;
    _ZN7WebCore18PlatformTimeRangesC1ERKS0_(&ptr->m_ranges, self->m_ranges);
    return WebCore::Ref<WebCore::TimeRanges>(*ptr);
}

WebCore::ExceptionOr<double> APS5_VABI _ZNK7WebCore10TimeRanges5startEj(const WebCore::TimeRanges* self, unsigned int index) {
    bool valid = false;
    WTF::MediaTime result = _ZNK7WebCore18PlatformTimeRanges5startEjRb(&self->m_ranges, index, valid);
    if (!valid) {
        return WebCore::ExceptionOr<double>(WebCore::IndexSizeError);
    }
    return WebCore::ExceptionOr<double>(mediaTimeToDouble(result));
}

WebCore::ExceptionOr<double> APS5_VABI _ZNK7WebCore10TimeRanges3endEj(const WebCore::TimeRanges* self, unsigned int index) {
    bool valid = false;
    WTF::MediaTime result = _ZNK7WebCore18PlatformTimeRanges3endEjRb(&self->m_ranges, index, valid);
    if (!valid) {
        return WebCore::ExceptionOr<double>(WebCore::IndexSizeError);
    }
    return WebCore::ExceptionOr<double>(mediaTimeToDouble(result));
}

}
