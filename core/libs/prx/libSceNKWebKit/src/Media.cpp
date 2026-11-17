#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

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

constexpr std::size_t mediaTimeFieldBytes = offsetof(WTF::MediaTime, m_timeFlags) + sizeof(uint8_t);

bool loadMediaTime(WTF::MediaTime& time, const WTF::MediaTime* source) {
    return GuestCopyIn_nid_no_patch(&time, source, mediaTimeFieldBytes);
}

bool storeMediaTime(WTF::MediaTime* destination, const WTF::MediaTime& time) {
    return GuestCopyOut_nid_no_patch(destination, &time, mediaTimeFieldBytes);
}

void setMediaTimeScale(WTF::MediaTime& time, uint32_t timeScale, WTF::MediaTime::RoundingFlags flags) {
    if (mediaTimeHasDoubleValue(time)) {
        time = _ZN3WTF9MediaTime16createWithDoubleEdj(time.m_timeValueAsDouble, timeScale);
        return;
    }

    if (!timeScale) {
        time = time.m_timeValue < 0 ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
        return;
    }

    if (timeScale == time.m_timeScale) {
        return;
    }

    timeScale = std::min(WTF::MediaTime::MaximumTimeScale, timeScale);

    __int128_t newValue = static_cast<__int128_t>(time.m_timeValue) * timeScale;
    int64_t remainder = static_cast<int64_t>(newValue % time.m_timeScale);
    newValue = newValue / time.m_timeScale;

    if (newValue < std::numeric_limits<int64_t>::min()) {
        time = _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
        return;
    }

    if (newValue > std::numeric_limits<int64_t>::max()) {
        time = _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
        return;
    }

    time.m_timeValue = static_cast<int64_t>(newValue);
    std::swap(time.m_timeScale, timeScale);

    if (!remainder) {
        return;
    }

    time.m_timeFlags |= WTF::MediaTime::HasBeenRounded;
    switch (flags) {
    case WTF::MediaTime::RoundingFlags::HalfAwayFromZero:
        if (static_cast<int64_t>(std::abs(remainder)) * 2 >= static_cast<int64_t>(timeScale)) {
            if (remainder < 0) {
                time.m_timeValue--;
            } else {
                time.m_timeValue++;
            }
        }
        break;

    case WTF::MediaTime::RoundingFlags::TowardZero:
        break;

    case WTF::MediaTime::RoundingFlags::AwayFromZero:
        if (remainder < 0) {
            time.m_timeValue--;
        } else {
            time.m_timeValue++;
        }
        break;

    case WTF::MediaTime::RoundingFlags::TowardPositiveInfinity:
        if (remainder > 0) {
            time.m_timeValue++;
        }
        break;

    case WTF::MediaTime::RoundingFlags::TowardNegativeInfinity:
        if (remainder < 0) {
            time.m_timeValue--;
        }
        break;
    }
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

using TimeRange = WebCore::PlatformTimeRanges::Range;
using TimeRangeVector = WebCore::PlatformTimeRanges::VectorRange;

static_assert(offsetof(WebCore::PlatformTimeRanges, m_ranges) == 0);
static_assert(offsetof(WebCore::TimeRanges, m_refCount) == 0);

constexpr std::size_t timeRangesPerChunk = 4096 / sizeof(TimeRange);

struct HostTimeRanges {
    std::vector<TimeRange> ranges;
    unsigned capacity = 0;
    bool reallocated = true;
};

bool loadTimeRangeVector(TimeRangeVector& vector, const void* source) {
    return GuestCopyIn_nid_no_patch(&vector, source, sizeof(vector));
}

bool storeTimeRangeVector(void* destination, const TimeRangeVector& vector) {
    return GuestCopyOut_nid_no_patch(destination, &vector, sizeof(vector));
}

bool loadTimeRange(TimeRange& range, const TimeRangeVector& vector, unsigned index) {
    return GuestCopyIn_nid_no_patch(&range, vector.m_buffer + index, sizeof(range));
}

bool loadTimeRanges(TimeRangeVector& vector, HostTimeRanges& host, const void* source) {
    if (!loadTimeRangeVector(vector, source)) {
        return false;
    }
    host.capacity = vector.m_capacity;
    host.reallocated = false;
    host.ranges.clear();
    for (std::size_t offset = 0; offset < vector.m_size;) {
        std::size_t count = std::min<std::size_t>(timeRangesPerChunk, vector.m_size - offset);
        host.ranges.resize(offset + count);
        if (!GuestCopyIn_nid_no_patch(host.ranges.data() + offset, vector.m_buffer + offset, count * sizeof(TimeRange))) {
            return false;
        }
        offset += count;
    }
    return true;
}

bool storeTimeRanges(WebCore::PlatformTimeRanges* destination, const TimeRangeVector& previous, const HostTimeRanges& host) {
    TimeRangeVector vector{previous.m_buffer, host.capacity, static_cast<unsigned>(host.ranges.size())};
    if (!host.reallocated) {
        return GuestWritable_nid_no_patch(destination, sizeof(vector))
            && (host.ranges.empty() || GuestCopyOut_nid_no_patch(previous.m_buffer, host.ranges.data(), host.ranges.size() * sizeof(TimeRange)))
            && storeTimeRangeVector(destination, vector);
    }
    vector.m_buffer = host.capacity ? static_cast<TimeRange*>(_ZN3WTF10fastMallocEm(host.capacity * sizeof(TimeRange))) : nullptr;
    if (!host.ranges.empty()) {
        std::memcpy(static_cast<void*>(vector.m_buffer), host.ranges.data(), host.ranges.size() * sizeof(TimeRange));
    }
    if (!storeTimeRangeVector(destination, vector)) {
        if (vector.m_buffer) {
            _ZN3WTF8fastFreeEPv(vector.m_buffer);
        }
        return false;
    }
    if (previous.m_buffer) {
        _ZN3WTF8fastFreeEPv(previous.m_buffer);
    }
    return true;
}

void addTimeRange(HostTimeRanges& host, const WTF::MediaTime& start, const WTF::MediaTime& end) {
    if (_ZNK3WTF9MediaTimegtERKS0_(&start, end)) {
        return;
    }

    TimeRange addedRange{start, end};
    auto& ranges = host.ranges;
    unsigned overlappingArcIndex = 0;

    for (overlappingArcIndex = 0; overlappingArcIndex < ranges.size(); overlappingArcIndex++) {
        if (rangeIsOverlappingRange(addedRange, ranges[overlappingArcIndex])
            || rangeIsContiguousWithRange(addedRange, ranges[overlappingArcIndex])) {
            addedRange = rangeUnionWithOverlappingOrContiguousRange(addedRange, ranges[overlappingArcIndex]);
            ranges.erase(ranges.begin() + overlappingArcIndex);
            --overlappingArcIndex;
        } else {
            if (!overlappingArcIndex) {
                if (rangeIsBeforeRange(addedRange, ranges[0])) {
                    break;
                }
            } else {
                if (rangeIsBeforeRange(ranges[overlappingArcIndex - 1], addedRange)
                    && rangeIsBeforeRange(addedRange, ranges[overlappingArcIndex])) {
                    break;
                }
            }
        }
    }

    if (ranges.size() == host.capacity) {
        host.capacity = host.capacity ? host.capacity * 2 : 4;
        host.reallocated = true;
    }

    ranges.insert(ranges.begin() + overlappingArcIndex, addedRange);
}

void uniteTimeRanges(HostTimeRanges& host, const std::vector<TimeRange>& ranges) {
    for (const auto& range : ranges) {
        addTimeRange(host, range.start, range.end);
    }
}

HostTimeRanges invertedTimeRanges(const std::vector<TimeRange>& ranges) {
    HostTimeRanges inverted;
    WTF::MediaTime posInf = _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    WTF::MediaTime negInf = _ZN3WTF9MediaTime20negativeInfiniteTimeEv();

    if (ranges.empty()) {
        addTimeRange(inverted, negInf, posInf);
        return inverted;
    }

    WTF::MediaTime start = ranges.front().start;
    if (!_ZNK3WTF9MediaTimeeqERKS0_(&start, negInf)) {
        addTimeRange(inverted, negInf, start);
    }

    for (std::size_t index = 0; index + 1 < ranges.size(); ++index) {
        addTimeRange(inverted, ranges[index].end, ranges[index + 1].start);
    }

    WTF::MediaTime end = ranges.back().end;
    if (!_ZNK3WTF9MediaTimeeqERKS0_(&end, posInf)) {
        addTimeRange(inverted, end, posInf);
    }
    return inverted;
}

WebCore::PlatformTimeRanges* platformTimeRanges(WebCore::TimeRanges* ranges) {
    return &ranges->m_ranges;
}

const WebCore::PlatformTimeRanges* platformTimeRanges(const WebCore::TimeRanges* ranges) {
    return &ranges->m_ranges;
}

bool initializeTimeRangesRefCount(WebCore::TimeRanges* ranges) {
    unsigned refCount = 1;
    return GuestCopyOut_nid_no_patch(ranges, &refCount, sizeof(refCount));
}

} // namespace

void WebCore::TimeRanges::deref() const {
    if (--m_refCount == 0) {
        _ZN7WebCore18PlatformTimeRangesD1Ev(&const_cast<TimeRanges*>(this)->m_ranges);
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
    while (floatTime * timeScale >= maxPlusOneInt64 || floatTime * timeScale < -maxPlusOneInt64) {
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
    while (doubleTime * timeScale >= maxPlusOneInt64 || doubleTime * timeScale < -maxPlusOneInt64) {
        timeScale /= 2;
    }
    WTF::MediaTime value{};
    value.m_timeValue = static_cast<int64_t>(std::round(doubleTime * timeScale));
    value.m_timeScale = timeScale;
    value.m_timeFlags = WTF::MediaTime::Valid;
    return value;
}

void APS5_VABI _ZN3WTF9MediaTimeC1Ev(WTF::MediaTime* self) {
    const WTF::MediaTime time{0, WTF::MediaTime::DefaultTimeScale, WTF::MediaTime::Valid};
    if (!storeMediaTime(self, time))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN3WTF9MediaTimeC2Ev(WTF::MediaTime* self) {
    _ZN3WTF9MediaTimeC1Ev(self);
}

void APS5_VABI _ZN3WTF9MediaTimeC1ERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, &rhs) || !storeMediaTime(self, time))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN3WTF9MediaTimeC2ERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    _ZN3WTF9MediaTimeC1ERKS0_(self, rhs);
}

void APS5_VABI _ZN3WTF9MediaTimeC1Eljh(WTF::MediaTime* self, int64_t value, uint32_t scale, uint8_t flags) {
    WTF::MediaTime time{value, scale, flags};
    if (!scale && (flags & WTF::MediaTime::Valid)) {
        time = value < 0 ? WTF::MediaTime{-1, 1, WTF::MediaTime::NegativeInfinite | WTF::MediaTime::Valid}
                         : WTF::MediaTime{0, 1, WTF::MediaTime::PositiveInfinite | WTF::MediaTime::Valid};
    }
    if (!storeMediaTime(self, time))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN3WTF9MediaTimeC2Eljh(WTF::MediaTime* self, int64_t value, uint32_t scale, uint8_t flags) {
    _ZN3WTF9MediaTimeC1Eljh(self, value, scale, flags);
}

WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTimeaSERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, &rhs) || !storeMediaTime(self, time))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return *self;
}

void APS5_VABI _ZN3WTF9MediaTime12setTimeScaleEjNS0_13RoundingFlagsE(WTF::MediaTime* self, uint32_t timeScale, WTF::MediaTime::RoundingFlags flags) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    setMediaTimeScale(time, timeScale, flags);
    if (!storeMediaTime(self, time))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTime11toTimeScaleEjNS0_13RoundingFlagsE(const WTF::MediaTime* self, uint32_t scale, WTF::MediaTime::RoundingFlags flags) {
    WTF::MediaTime result;
    if (!loadMediaTime(result, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    setMediaTimeScale(result, scale, flags);
    return result;
}

WTF::MediaTime::ComparisonFlags APS5_VABI _ZNK3WTF9MediaTime7compareERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    WTF::MediaTime time;
    WTF::MediaTime other;
    if (!loadMediaTime(time, self) || !loadMediaTime(other, &rhs))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    auto andFlags = time.m_timeFlags & other.m_timeFlags;
    if (andFlags & (WTF::MediaTime::PositiveInfinite | WTF::MediaTime::NegativeInfinite | WTF::MediaTime::Indefinite)) {
        return WTF::MediaTime::EqualTo;
    }

    auto orFlags = time.m_timeFlags | other.m_timeFlags;
    if (!(orFlags & WTF::MediaTime::Valid)) {
        return WTF::MediaTime::EqualTo;
    }

    if (!(andFlags & WTF::MediaTime::Valid)) {
        return mediaTimeIsInvalid(time) ? WTF::MediaTime::GreaterThan : WTF::MediaTime::LessThan;
    }

    if (orFlags & WTF::MediaTime::NegativeInfinite) {
        return mediaTimeIsNegativeInfinite(time) ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    if (orFlags & WTF::MediaTime::PositiveInfinite) {
        return mediaTimeIsPositiveInfinite(time) ? WTF::MediaTime::GreaterThan : WTF::MediaTime::LessThan;
    }

    if (orFlags & WTF::MediaTime::Indefinite) {
        return mediaTimeIsIndefinite(time) ? WTF::MediaTime::GreaterThan : WTF::MediaTime::LessThan;
    }

    if (andFlags & WTF::MediaTime::DoubleValue) {
        if (time.m_timeValueAsDouble == other.m_timeValueAsDouble) {
            return WTF::MediaTime::EqualTo;
        }
        return time.m_timeValueAsDouble < other.m_timeValueAsDouble ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    if (orFlags & WTF::MediaTime::DoubleValue) {
        double a = mediaTimeToDouble(time);
        double b = mediaTimeToDouble(other);
        if (a > b) {
            return WTF::MediaTime::GreaterThan;
        }
        if (a < b) {
            return WTF::MediaTime::LessThan;
        }
        return WTF::MediaTime::EqualTo;
    }

    if ((time.m_timeValue < 0) != (other.m_timeValue < 0)) {
        return time.m_timeValue < 0 ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    if (!time.m_timeValue && !other.m_timeValue) {
        return WTF::MediaTime::EqualTo;
    }

    if (time.m_timeScale == other.m_timeScale) {
        if (time.m_timeValue == other.m_timeValue) {
            return WTF::MediaTime::EqualTo;
        }
        return time.m_timeValue < other.m_timeValue ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    if (time.m_timeValue == other.m_timeValue) {
        return time.m_timeScale < other.m_timeScale ? WTF::MediaTime::GreaterThan : WTF::MediaTime::LessThan;
    }

    if (time.m_timeValue >= 0) {
        if (time.m_timeValue < other.m_timeValue && time.m_timeScale > other.m_timeScale) {
            return WTF::MediaTime::LessThan;
        }
        if (time.m_timeValue > other.m_timeValue && time.m_timeScale < other.m_timeScale) {
            return WTF::MediaTime::GreaterThan;
        }
    } else {
        if (time.m_timeValue < other.m_timeValue && time.m_timeScale < other.m_timeScale) {
            return WTF::MediaTime::LessThan;
        }
        if (time.m_timeValue > other.m_timeValue && time.m_timeScale > other.m_timeScale) {
            return WTF::MediaTime::GreaterThan;
        }
    }

    int64_t lhsFactor;
    int64_t rhsFactor;
    if (!__builtin_mul_overflow(time.m_timeValue, static_cast<int64_t>(other.m_timeScale), &lhsFactor)
        && !__builtin_mul_overflow(other.m_timeValue, static_cast<int64_t>(time.m_timeScale), &rhsFactor)) {
        if (lhsFactor == rhsFactor) {
            return WTF::MediaTime::EqualTo;
        }
        return lhsFactor < rhsFactor ? WTF::MediaTime::LessThan : WTF::MediaTime::GreaterThan;
    }

    int64_t rhsWhole = other.m_timeValue / other.m_timeScale;
    int64_t lhsWhole = time.m_timeValue / time.m_timeScale;
    if (lhsWhole > rhsWhole) {
        return WTF::MediaTime::GreaterThan;
    }
    if (lhsWhole < rhsWhole) {
        return WTF::MediaTime::LessThan;
    }

    int64_t rhsRemain = other.m_timeValue % other.m_timeScale;
    int64_t lhsRemain = time.m_timeValue % time.m_timeScale;
    lhsFactor = lhsRemain * other.m_timeScale;
    rhsFactor = rhsRemain * time.m_timeScale;

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
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return (time.m_timeFlags == WTF::MediaTime::Valid && !time.m_timeValue)
        || (time.m_timeFlags == (WTF::MediaTime::Valid | WTF::MediaTime::DoubleValue) && !time.m_timeValueAsDouble)
        || mediaTimeIsInvalid(time);
}

bool APS5_VABI _ZNK3WTF9MediaTimecvbEv(const WTF::MediaTime* self) {
    return !_ZNK3WTF9MediaTimentEv(self);
}

bool APS5_VABI _ZNK3WTF9MediaTime12isIndefiniteEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return mediaTimeIsIndefinite(time);
}

bool APS5_VABI _ZNK3WTF9MediaTime14hasBeenRoundedEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return mediaTimeHasBeenRounded(time);
}

bool APS5_VABI _ZNK3WTF9MediaTime14hasDoubleValueEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return mediaTimeHasDoubleValue(time);
}

bool APS5_VABI _ZNK3WTF9MediaTime18isNegativeInfiniteEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return mediaTimeIsNegativeInfinite(time);
}

bool APS5_VABI _ZNK3WTF9MediaTime18isPositiveInfiniteEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return mediaTimeIsPositiveInfinite(time);
}

bool APS5_VABI _ZNK3WTF9MediaTime7isValidEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return mediaTimeIsValid(time);
}

bool APS5_VABI _ZNK3WTF9MediaTime9isInvalidEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return mediaTimeIsInvalid(time);
}

float APS5_VABI _ZNK3WTF9MediaTime7toFloatEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return mediaTimeToFloat(time);
}

double APS5_VABI _ZNK3WTF9MediaTime8toDoubleEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return mediaTimeToDouble(time);
}

uint8_t APS5_VABI _ZNK3WTF9MediaTime9timeFlagsEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return time.m_timeFlags;
}

const uint32_t& APS5_VABI _ZNK3WTF9MediaTime9timeScaleEv(const WTF::MediaTime* self) {
    return self->m_timeScale;
}

const int64_t& APS5_VABI _ZNK3WTF9MediaTime9timeValueEv(const WTF::MediaTime* self) {
    return self->m_timeValue;
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimeplERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    WTF::MediaTime time;
    WTF::MediaTime other;
    if (!loadMediaTime(time, self) || !loadMediaTime(other, &rhs))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (mediaTimeIsInvalid(other) || mediaTimeIsInvalid(time)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsIndefinite(other) || mediaTimeIsIndefinite(time)) {
        return _ZN3WTF9MediaTime14indefiniteTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(time) && mediaTimeIsNegativeInfinite(other)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(time) && mediaTimeIsPositiveInfinite(other)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(time) || mediaTimeIsPositiveInfinite(other)) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(time) || mediaTimeIsNegativeInfinite(other)) {
        return _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (mediaTimeHasDoubleValue(time) && mediaTimeHasDoubleValue(other)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(time.m_timeValueAsDouble + other.m_timeValueAsDouble);
    }
    if (mediaTimeHasDoubleValue(time) || mediaTimeHasDoubleValue(other)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(mediaTimeToDouble(time) + mediaTimeToDouble(other));
    }

    WTF::MediaTime a = time;
    WTF::MediaTime b = other;

    uint32_t commonTimeScale;
    if (!leastCommonMultiple(a.m_timeScale, b.m_timeScale, commonTimeScale) || commonTimeScale > WTF::MediaTime::MaximumTimeScale) {
        commonTimeScale = WTF::MediaTime::MaximumTimeScale;
    }
    setMediaTimeScale(a, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    setMediaTimeScale(b, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);

    int64_t result;
    while (__builtin_add_overflow(a.m_timeValue, b.m_timeValue, &result)) {
        if (commonTimeScale == 1) {
            return a.m_timeValue > 0 ? _ZN3WTF9MediaTime20positiveInfiniteTimeEv() : _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
        }
        commonTimeScale /= 2;
        setMediaTimeScale(a, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
        setMediaTimeScale(b, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    }
    a.m_timeValue = result;
    return a;
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimemiERKS0_(const WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    WTF::MediaTime time;
    WTF::MediaTime other;
    if (!loadMediaTime(time, self) || !loadMediaTime(other, &rhs))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (mediaTimeIsInvalid(other) || mediaTimeIsInvalid(time)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsIndefinite(other) || mediaTimeIsIndefinite(time)) {
        return _ZN3WTF9MediaTime14indefiniteTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(time) && mediaTimeIsPositiveInfinite(other)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(time) && mediaTimeIsNegativeInfinite(other)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(time) || mediaTimeIsNegativeInfinite(other)) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(time) || mediaTimeIsPositiveInfinite(other)) {
        return _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (mediaTimeHasDoubleValue(time) && mediaTimeHasDoubleValue(other)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(time.m_timeValueAsDouble - other.m_timeValueAsDouble);
    }
    if (mediaTimeHasDoubleValue(time) || mediaTimeHasDoubleValue(other)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(mediaTimeToDouble(time) - mediaTimeToDouble(other));
    }

    WTF::MediaTime a = time;
    WTF::MediaTime b = other;

    uint32_t commonTimeScale;
    if (!leastCommonMultiple(time.m_timeScale, other.m_timeScale, commonTimeScale) || commonTimeScale > WTF::MediaTime::MaximumTimeScale) {
        commonTimeScale = WTF::MediaTime::MaximumTimeScale;
    }
    setMediaTimeScale(a, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    setMediaTimeScale(b, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);

    int64_t result;
    while (__builtin_sub_overflow(a.m_timeValue, b.m_timeValue, &result)) {
        if (commonTimeScale == 1) {
            return a.m_timeValue > 0 ? _ZN3WTF9MediaTime20positiveInfiniteTimeEv() : _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
        }
        commonTimeScale /= 2;
        setMediaTimeScale(a, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
        setMediaTimeScale(b, commonTimeScale, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    }
    a.m_timeValue = result;
    return a;
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimengEv(const WTF::MediaTime* self) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (mediaTimeIsInvalid(time)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsIndefinite(time)) {
        return _ZN3WTF9MediaTime14indefiniteTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(time)) {
        return _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(time)) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    WTF::MediaTime negativeTime = time;
    if (mediaTimeHasDoubleValue(negativeTime)) {
        negativeTime.m_timeValueAsDouble = -negativeTime.m_timeValueAsDouble;
    } else {
        negativeTime.m_timeValue = -negativeTime.m_timeValue;
    }
    return negativeTime;
}

WTF::MediaTime APS5_VABI _ZNK3WTF9MediaTimemlEi(const WTF::MediaTime* self, int32_t rhs) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (mediaTimeIsInvalid(time)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsIndefinite(time)) {
        return _ZN3WTF9MediaTime14indefiniteTimeEv();
    }
    if (!rhs) {
        return _ZN3WTF9MediaTime8zeroTimeEv();
    }
    if (mediaTimeIsPositiveInfinite(time)) {
        return rhs > 0 ? _ZN3WTF9MediaTime20positiveInfiniteTimeEv() : _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(time)) {
        return rhs > 0 ? _ZN3WTF9MediaTime20negativeInfiniteTimeEv() : _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (mediaTimeHasDoubleValue(time)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(time.m_timeValueAsDouble * rhs);
    }
    WTF::MediaTime a = time;
    int64_t result;
    while (__builtin_mul_overflow(a.m_timeValue, rhs, &result)) {
        if (a.m_timeScale == 1) {
            return signum(a.m_timeValue) == signum(rhs) ? _ZN3WTF9MediaTime20positiveInfiniteTimeEv() : _ZN3WTF9MediaTime20negativeInfiniteTimeEv();
        }
        setMediaTimeScale(a, a.m_timeScale / 2, WTF::MediaTime::RoundingFlags::HalfAwayFromZero);
    }
    a.m_timeValue = result;
    return a;
}

WTF::MediaTime APS5_VABI _ZN3WTF3absERKNS_9MediaTimeE(const WTF::MediaTime& rhs) {
    WTF::MediaTime time;
    if (!loadMediaTime(time, &rhs))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (mediaTimeIsInvalid(time)) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    if (mediaTimeIsNegativeInfinite(time) || mediaTimeIsPositiveInfinite(time)) {
        return _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    }
    if (mediaTimeHasDoubleValue(time)) {
        return _ZN3WTF9MediaTime16createWithDoubleEd(std::abs(time.m_timeValueAsDouble));
    }
    WTF::MediaTime val = time;
    val.m_timeValue = std::abs(time.m_timeValue);
    return val;
}

WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTimepLERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    if (!storeMediaTime(self, _ZNK3WTF9MediaTimeplERKS0_(self, rhs)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return *self;
}

WTF::MediaTime& APS5_VABI _ZN3WTF9MediaTimemIERKS0_(WTF::MediaTime* self, const WTF::MediaTime& rhs) {
    if (!storeMediaTime(self, _ZNK3WTF9MediaTimemiERKS0_(self, rhs)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
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
    if (!storeTimeRangeVector(self, TimeRangeVector{}))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
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
    TimeRangeVector moved;
    if (!loadTimeRangeVector(moved, ranges) || !GuestWritable_nid_no_patch(ranges, sizeof(moved))
        || !storeTimeRangeVector(self, moved) || !storeTimeRangeVector(ranges, TimeRangeVector{}))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesC2EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(WebCore::PlatformTimeRanges* self, WebCore::PlatformTimeRanges::VectorRange* ranges) {
    _ZN7WebCore18PlatformTimeRangesC1EON3WTF6VectorINS0_5RangeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(self, ranges);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesD1Ev(WebCore::PlatformTimeRanges* self) {
    TimeRangeVector previous;
    if (!loadTimeRangeVector(previous, self) || !storeTimeRangeVector(self, TimeRangeVector{}))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (previous.m_buffer) {
        _ZN3WTF8fastFreeEPv(previous.m_buffer);
    }
}

void APS5_VABI _ZN7WebCore18PlatformTimeRangesD2Ev(WebCore::PlatformTimeRanges* self) {
    _ZN7WebCore18PlatformTimeRangesD1Ev(self);
}

WebCore::PlatformTimeRanges& APS5_VABI _ZN7WebCore18PlatformTimeRangesaSERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other) {
    if (self != &other) {
        TimeRangeVector previous;
        TimeRangeVector source;
        HostTimeRanges copy;
        if (!loadTimeRangeVector(previous, self) || !loadTimeRanges(source, copy, &other))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        copy.capacity = source.m_size;
        copy.reallocated = true;
        if (!storeTimeRanges(self, previous, copy))
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return *self;
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(WebCore::PlatformTimeRanges* self, const WTF::MediaTime& start, const WTF::MediaTime& end) {
    WTF::MediaTime first;
    WTF::MediaTime last;
    if (!loadMediaTime(first, &start) || !loadMediaTime(last, &end))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (_ZNK3WTF9MediaTimegtERKS0_(&first, last)) {
        return;
    }
    TimeRangeVector previous;
    HostTimeRanges ranges;
    if (!loadTimeRanges(previous, ranges, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    addTimeRange(ranges, first, last);
    if (!storeTimeRanges(self, previous, ranges))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges5clearEv(WebCore::PlatformTimeRanges* self) {
    TimeRangeVector vector;
    if (!loadTimeRangeVector(vector, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    vector.m_size = 0;
    if (!storeTimeRangeVector(self, vector))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges6invertEv(WebCore::PlatformTimeRanges* self) {
    TimeRangeVector previous;
    HostTimeRanges ranges;
    if (!loadTimeRanges(previous, ranges, self) || !storeTimeRanges(self, previous, invertedTimeRanges(ranges.ranges)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges9unionWithERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other) {
    TimeRangeVector previous;
    TimeRangeVector source;
    HostTimeRanges unioned;
    HostTimeRanges others;
    if (!loadTimeRanges(previous, unioned, self) || !loadTimeRanges(source, others, &other))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    unioned.capacity = previous.m_size;
    unioned.reallocated = true;
    uniteTimeRanges(unioned, others.ranges);
    if (!storeTimeRanges(self, previous, unioned))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_(WebCore::PlatformTimeRanges* self, const WebCore::PlatformTimeRanges& other) {
    TimeRangeVector previous;
    TimeRangeVector source;
    HostTimeRanges ranges;
    HostTimeRanges others;
    if (!loadTimeRanges(previous, ranges, self) || !loadTimeRanges(source, others, &other))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    HostTimeRanges unioned = invertedTimeRanges(ranges.ranges);
    uniteTimeRanges(unioned, invertedTimeRanges(others.ranges).ranges);
    if (!storeTimeRanges(self, previous, invertedTimeRanges(unioned.ranges)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges5startEjRb(const WebCore::PlatformTimeRanges* self, unsigned int index, bool& valid) {
    TimeRangeVector vector;
    TimeRange range;
    if (!loadTimeRangeVector(vector, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    bool found = index < vector.m_size;
    if ((found && !loadTimeRange(range, vector, index)) || !GuestCopyOut_nid_no_patch(&valid, &found, sizeof(found)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!found) {
        return _ZN3WTF9MediaTime8zeroTimeEv();
    }
    return range.start;
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges5startEj(const WebCore::PlatformTimeRanges* self, unsigned int index) {
    bool ignored = false;
    return _ZNK7WebCore18PlatformTimeRanges5startEjRb(self, index, ignored);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges3endEjRb(const WebCore::PlatformTimeRanges* self, unsigned int index, bool& valid) {
    TimeRangeVector vector;
    TimeRange range;
    if (!loadTimeRangeVector(vector, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    bool found = index < vector.m_size;
    if ((found && !loadTimeRange(range, vector, index)) || !GuestCopyOut_nid_no_patch(&valid, &found, sizeof(found)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!found) {
        return _ZN3WTF9MediaTime8zeroTimeEv();
    }
    return range.end;
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges3endEj(const WebCore::PlatformTimeRanges* self, unsigned int index) {
    bool ignored = false;
    return _ZNK7WebCore18PlatformTimeRanges3endEjRb(self, index, ignored);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges8durationEj(const WebCore::PlatformTimeRanges* self, unsigned int index) {
    TimeRangeVector vector;
    TimeRange range;
    if (!loadTimeRangeVector(vector, self) || (index < vector.m_size && !loadTimeRange(range, vector, index)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (index >= vector.m_size) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    return _ZNK3WTF9MediaTimemiERKS0_(&range.end, range.start);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges19maximumBufferedTimeEv(const WebCore::PlatformTimeRanges* self) {
    TimeRangeVector vector;
    TimeRange range;
    if (!loadTimeRangeVector(vector, self) || (vector.m_size && !loadTimeRange(range, vector, vector.m_size - 1)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!vector.m_size) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    return range.end;
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges13totalDurationEv(const WebCore::PlatformTimeRanges* self) {
    TimeRangeVector vector;
    HostTimeRanges ranges;
    if (!loadTimeRanges(vector, ranges, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    WTF::MediaTime total = _ZN3WTF9MediaTime8zeroTimeEv();
    for (const auto& range : ranges.ranges) {
        WTF::MediaTime diff = _ZNK3WTF9MediaTimemiERKS0_(&range.end, range.start);
        WTF::MediaTime absDiff = _ZN3WTF3absERKNS_9MediaTimeE(diff);
        total = _ZNK3WTF9MediaTimeplERKS0_(&total, absDiff);
    }
    return total;
}

std::size_t APS5_VABI _ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(const WebCore::PlatformTimeRanges* self, const WTF::MediaTime& time) {
    WTF::MediaTime point;
    TimeRangeVector vector;
    HostTimeRanges ranges;
    if (!loadMediaTime(point, &time) || !loadTimeRanges(vector, ranges, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    for (std::size_t n = 0; n < ranges.ranges.size(); ++n) {
        if (_ZNK3WTF9MediaTimegeERKS0_(&point, ranges.ranges[n].start) && _ZNK3WTF9MediaTimeleERKS0_(&point, ranges.ranges[n].end)) {
            return n;
        }
    }
    return static_cast<std::size_t>(-1);
}

bool APS5_VABI _ZNK7WebCore18PlatformTimeRanges7containERKN3WTF9MediaTimeE(const WebCore::PlatformTimeRanges* self, const WTF::MediaTime& time) {
    return _ZNK7WebCore18PlatformTimeRanges4findERKN3WTF9MediaTimeE(self, time) != static_cast<std::size_t>(-1);
}

WTF::MediaTime APS5_VABI _ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(const WebCore::PlatformTimeRanges* self, const WTF::MediaTime& time) {
    WTF::MediaTime point;
    TimeRangeVector vector;
    HostTimeRanges ranges;
    if (!loadMediaTime(point, &time) || !loadTimeRanges(vector, ranges, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    WTF::MediaTime closestDelta = _ZN3WTF9MediaTime20positiveInfiniteTimeEv();
    WTF::MediaTime closestTime = _ZN3WTF9MediaTime8zeroTimeEv();
    if (ranges.ranges.empty()) {
        return _ZN3WTF9MediaTime11invalidTimeEv();
    }
    for (const auto& range : ranges.ranges) {
        WTF::MediaTime startTime = range.start;
        WTF::MediaTime endTime = range.end;
        if (_ZNK3WTF9MediaTimegeERKS0_(&point, startTime) && _ZNK3WTF9MediaTimeleERKS0_(&point, endTime)) {
            return point;
        }
        WTF::MediaTime startDiff = _ZNK3WTF9MediaTimemiERKS0_(&startTime, point);
        WTF::MediaTime startTimeDelta = _ZN3WTF3absERKNS_9MediaTimeE(startDiff);
        if (_ZNK3WTF9MediaTimeltERKS0_(&startTimeDelta, closestDelta)) {
            closestTime = startTime;
            closestDelta = startTimeDelta;
        }
        WTF::MediaTime endDiff = _ZNK3WTF9MediaTimemiERKS0_(&endTime, point);
        WTF::MediaTime endTimeDelta = _ZN3WTF3absERKNS_9MediaTimeE(endDiff);
        if (_ZNK3WTF9MediaTimeltERKS0_(&endTimeDelta, closestDelta)) {
            closestTime = endTime;
            closestDelta = endTimeDelta;
        }
    }
    return closestTime;
}

unsigned int APS5_VABI _ZNK7WebCore18PlatformTimeRanges6lengthEv(const WebCore::PlatformTimeRanges* self) {
    TimeRangeVector vector;
    if (!loadTimeRangeVector(vector, self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return vector.m_size;
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
    if (!initializeTimeRangesRefCount(self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    _ZN7WebCore18PlatformTimeRangesC1Ev(platformTimeRanges(self));
}

void APS5_VABI _ZN7WebCore10TimeRangesC2Ev(WebCore::TimeRanges* self) {
    _ZN7WebCore10TimeRangesC1Ev(self);
}

void APS5_VABI _ZN7WebCore10TimeRangesC1Edd(WebCore::TimeRanges* self, double start, double end) {
    if (!initializeTimeRangesRefCount(self))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    WTF::MediaTime s = _ZN3WTF9MediaTime16createWithDoubleEd(start);
    WTF::MediaTime e = _ZN3WTF9MediaTime16createWithDoubleEd(end);
    _ZN7WebCore18PlatformTimeRangesC1ERKN3WTF9MediaTimeES4_(platformTimeRanges(self), s, e);
}

void APS5_VABI _ZN7WebCore10TimeRangesC2Edd(WebCore::TimeRanges* self, double start, double end) {
    _ZN7WebCore10TimeRangesC1Edd(self, start, end);
}

void APS5_VABI _ZN7WebCore10TimeRanges3addEdd(WebCore::TimeRanges* self, double start, double end) {
    WTF::MediaTime s = _ZN3WTF9MediaTime16createWithDoubleEd(start);
    WTF::MediaTime e = _ZN3WTF9MediaTime16createWithDoubleEd(end);
    _ZN7WebCore18PlatformTimeRanges3addERKN3WTF9MediaTimeES4_(platformTimeRanges(self), s, e);
}

void APS5_VABI _ZN7WebCore10TimeRanges13intersectWithERKS0_(WebCore::TimeRanges* self, const WebCore::TimeRanges& other) {
    _ZN7WebCore18PlatformTimeRanges13intersectWithERKS0_(platformTimeRanges(self), *platformTimeRanges(&other));
}

unsigned int APS5_VABI _ZNK7WebCore10TimeRanges6lengthEv(const WebCore::TimeRanges* self) {
    return _ZNK7WebCore18PlatformTimeRanges6lengthEv(platformTimeRanges(self));
}

double APS5_VABI _ZNK7WebCore10TimeRanges7nearestEd(const WebCore::TimeRanges* self, double time) {
    WTF::MediaTime t = _ZN3WTF9MediaTime16createWithDoubleEd(time);
    WTF::MediaTime n = _ZNK7WebCore18PlatformTimeRanges7nearestERKN3WTF9MediaTimeE(platformTimeRanges(self), t);
    return mediaTimeToDouble(n);
}

WebCore::Ref<WebCore::TimeRanges> APS5_VABI _ZNK7WebCore10TimeRanges4copyEv(const WebCore::TimeRanges* self) {
    void* mem = _ZN3WTF10fastMallocEm(sizeof(WebCore::TimeRanges));
    auto* ptr = static_cast<WebCore::TimeRanges*>(mem);
    ptr->m_refCount = 1;
    _ZN7WebCore18PlatformTimeRangesC1ERKS0_(&ptr->m_ranges, *platformTimeRanges(self));
    return WebCore::Ref<WebCore::TimeRanges>(*ptr);
}

WebCore::ExceptionOr<double> APS5_VABI _ZNK7WebCore10TimeRanges5startEj(const WebCore::TimeRanges* self, unsigned int index) {
    bool valid = false;
    WTF::MediaTime result = _ZNK7WebCore18PlatformTimeRanges5startEjRb(platformTimeRanges(self), index, valid);
    if (!valid) {
        return WebCore::ExceptionOr<double>(WebCore::IndexSizeError);
    }
    return WebCore::ExceptionOr<double>(mediaTimeToDouble(result));
}

WebCore::ExceptionOr<double> APS5_VABI _ZNK7WebCore10TimeRanges3endEj(const WebCore::TimeRanges* self, unsigned int index) {
    bool valid = false;
    WTF::MediaTime result = _ZNK7WebCore18PlatformTimeRanges3endEjRb(platformTimeRanges(self), index, valid);
    if (!valid) {
        return WebCore::ExceptionOr<double>(WebCore::IndexSizeError);
    }
    return WebCore::ExceptionOr<double>(mediaTimeToDouble(result));
}

}
