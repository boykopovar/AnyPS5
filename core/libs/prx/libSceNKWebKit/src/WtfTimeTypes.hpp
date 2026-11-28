#pragma once

#include <cstddef>
#include <cstdint>

namespace WTF {

enum class ClockType : std::uint32_t {
    Wall,
    Monotonic,
    Approximate,
};

enum class TimeType : std::uint32_t {
    UTCTime,
    LocalTime,
};

struct Seconds {
    double m_value{0.0};
};

struct MonotonicTime {
    double m_value{0.0};
};

struct WallTime {
    double m_value{0.0};
};

struct TimeWithDynamicClockType {
    double m_value{0.0};
    ClockType m_type{ClockType::Wall};
};

struct CPUTime {
    MonotonicTime cpuTime;
    Seconds userTime;
    Seconds systemTime;
};

struct GregorianDateTime {
    int m_year{0};
    int m_month{0};
    int m_yearDay{0};
    int m_monthDay{0};
    int m_weekDay{0};
    int m_hour{0};
    int m_minute{0};
    int m_second{0};
    int m_utcOffsetInMinute{0};
    int m_isDST{0};
};

struct TimingScope {
    MonotonicTime m_startTime;
    const char* m_name;
    unsigned m_logIterationInterval;
};

static_assert(sizeof(TimingScope) == 24);
static_assert(offsetof(TimingScope, m_startTime) == 0);
static_assert(offsetof(TimingScope, m_name) == 8);
static_assert(offsetof(TimingScope, m_logIterationInterval) == 16);

struct LocalTimeOffset {
    bool isDST{false};
    int offset{0};
};

}  // namespace WTF
