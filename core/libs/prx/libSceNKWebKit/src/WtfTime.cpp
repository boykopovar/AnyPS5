#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

namespace WTF {

static double currentWallTimeSeconds() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

static double currentMonotonicTimeSeconds() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

static bool isLeapYear(int year) {
    if (year % 4 != 0) {
        return false;
    }
    if (year % 100 != 0) {
        return true;
    }
    return (year % 400 == 0);
}

static int dayInYear(int year, int month, int day) {
    static const int firstDayOfMonth[2][12] = {
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
    };
    bool leap = isLeapYear(year);
    int m = (month >= 0 && month < 12) ? month : 0;
    return firstDayOfMonth[leap ? 1 : 0][m] + day - 1;
}

static double daysFrom1970ToYear(int year) {
    const double yMinus1 = static_cast<double>(year) - 1.0;
    const double by4 = std::floor(yMinus1 / 4.0) - (1970 / 4);
    const double by100 = std::floor(yMinus1 / 100.0) - (1970 / 100);
    const double by400 = std::floor(yMinus1 / 400.0) - (1970 / 400);
    return 365.0 * (year - 1970.0) + by4 - by100 + by400;
}

static double ymdhmsToMilliseconds(int year, long mon, long day, long hour, long minute, long second, double milliseconds) {
    static const int firstDayOfMonth[2][12] = {
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
    };
    int mday = firstDayOfMonth[isLeapYear(year) ? 1 : 0][mon - 1];
    double ydays = daysFrom1970ToYear(year);
    double dateMs = milliseconds + second * 1000.0 + minute * 60000.0 + hour * 3600000.0 + (mday + day - 1 + ydays) * 86400000.0;
    if (dateMs < -8640000000000000.0 || dateMs > 8640000000000000.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return dateMs;
}

static const char* parseES5DatePortion(const char* currentPosition, int& year, long& month, long& day) {
    char* postParse = nullptr;
    year = static_cast<int>(std::strtol(currentPosition, &postParse, 10));
    if (postParse == currentPosition) {
        return nullptr;
    }
    if (*currentPosition == '-' && year == 0) {
        return nullptr;
    }
    if (*postParse != '-') {
        return postParse;
    }
    currentPosition = postParse + 1;
    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    month = std::strtol(currentPosition, &postParse, 10);
    if ((postParse - currentPosition) != 2) {
        return nullptr;
    }
    if (*postParse != '-') {
        return postParse;
    }
    currentPosition = postParse + 1;
    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    day = std::strtol(currentPosition, &postParse, 10);
    if ((postParse - currentPosition) != 2) {
        return nullptr;
    }
    return postParse;
}

static const char* parseES5TimePortion(const char* currentPosition, long& hours, long& minutes, long& seconds, double& milliseconds, bool& isLocalTime, long& timeZoneSeconds) {
    isLocalTime = false;
    char* postParse = nullptr;
    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    hours = std::strtol(currentPosition, &postParse, 10);
    if (*postParse != ':' || (postParse - currentPosition) != 2) {
        return nullptr;
    }
    currentPosition = postParse + 1;
    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    minutes = std::strtol(currentPosition, &postParse, 10);
    if ((postParse - currentPosition) != 2) {
        return nullptr;
    }
    currentPosition = postParse;

    if (*currentPosition == ':') {
        ++currentPosition;
        if (*currentPosition < '0' || *currentPosition > '9') {
            return nullptr;
        }
        seconds = std::strtol(currentPosition, &postParse, 10);
        if ((postParse - currentPosition) != 2) {
            return nullptr;
        }
        if (*postParse == '.') {
            currentPosition = postParse + 1;
            if (*currentPosition < '0' || *currentPosition > '9') {
                return nullptr;
            }
            long frac = std::strtol(currentPosition, &postParse, 10);
            long numFracDigits = postParse - currentPosition;
            milliseconds = frac * std::pow(10.0, static_cast<double>(-numFracDigits + 3));
        }
        currentPosition = postParse;
    }

    if (*currentPosition == 'Z') {
        return currentPosition + 1;
    }

    bool tzNegative = false;
    if (*currentPosition == '-') {
        tzNegative = true;
    } else if (*currentPosition == '+') {
        tzNegative = false;
    } else {
        isLocalTime = true;
        return currentPosition;
    }
    ++currentPosition;

    long tzHours = 0;
    long tzHoursAbs = 0;
    long tzMinutes = 0;

    if (*currentPosition < '0' || *currentPosition > '9') {
        return nullptr;
    }
    tzHours = std::strtol(currentPosition, &postParse, 10);
    if (*postParse != ':') {
        if ((postParse - currentPosition) == 2) {
            tzHoursAbs = std::labs(tzHours);
        } else if ((postParse - currentPosition) == 4) {
            tzHoursAbs = std::labs(tzHours);
            tzMinutes = tzHoursAbs % 100;
            tzHoursAbs = tzHoursAbs / 100;
        } else {
            return nullptr;
        }
    } else {
        if ((postParse - currentPosition) != 2) {
            return nullptr;
        }
        tzHoursAbs = std::labs(tzHours);
        currentPosition = postParse + 1;
        if (*currentPosition < '0' || *currentPosition > '9') {
            return nullptr;
        }
        tzMinutes = std::strtol(currentPosition, &postParse, 10);
        if ((postParse - currentPosition) != 2) {
            return nullptr;
        }
    }
    currentPosition = postParse;
    if (tzHoursAbs > 24 || tzMinutes < 0 || tzMinutes > 59) {
        return nullptr;
    }
    timeZoneSeconds = 60 * (tzMinutes + (60 * tzHoursAbs));
    if (tzNegative) {
        timeZoneSeconds = -timeZoneSeconds;
    }
    return currentPosition;
}

}  // namespace WTF

namespace {

struct TimingCallData {
    double totalDuration{0.0};
    unsigned callCount{0};
    double maxDuration{0.0};
};

class TimingTotals {
public:
    TimingCallData Add(const char* name, double duration) {
        std::lock_guard lock(m_lock);
        auto& result = m_totals[name];
        ++result.callCount;
        result.maxDuration = std::max(result.maxDuration, duration);
        result.totalDuration += duration;
        return result;
    }

private:
    std::mutex m_lock;
    std::unordered_map<const char*, TimingCallData> m_totals;
};

TimingTotals& Totals() {
    static TimingTotals* totals = new TimingTotals();
    return *totals;
}

}  // namespace

extern "C" {
WTF::MonotonicTime APS5_VABI _ZN3WTF13MonotonicTime3nowEv() {
    return WTF::MonotonicTime{WTF::currentMonotonicTimeSeconds()};
}

void APS5_VABI _ZN3WTF11TimingScope11scopeDidEndEv(WTF::TimingScope* self) {
    if (!self) throw std::invalid_argument("TimingScope::scopeDidEnd: null scope");
    if (!self->m_name) throw std::invalid_argument("TimingScope::scopeDidEnd: null name");
    if (!self->m_logIterationInterval) throw std::invalid_argument("TimingScope::scopeDidEnd: zero log iteration interval");
    const double duration = _ZN3WTF13MonotonicTime3nowEv().m_value - self->m_startTime.m_value;
    const auto data = Totals().Add(self->m_name, duration);
    if (!(data.callCount % self->m_logIterationInterval))
        WTFLogAlways("%s: %u calls, mean duration: %.6fms, total duration: %.6fms, max duration %.6fms", self->m_name, data.callCount,
            data.totalDuration / data.callCount * 1000.0, data.totalDuration * 1000.0, data.maxDuration * 1000.0);
}

WTF::WallTime APS5_VABI _ZN3WTF8WallTime3nowEv() {
    return WTF::WallTime{WTF::currentWallTimeSeconds()};
}

WTF::WallTime APS5_VABI _ZNK3WTF13MonotonicTime19approximateWallTimeEv(const WTF::MonotonicTime* self) {
    if (std::isinf(self->m_value)) {
        return WTF::WallTime{self->m_value};
    }
    return WTF::WallTime{self->m_value - WTF::currentMonotonicTimeSeconds() + WTF::currentWallTimeSeconds()};
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF8WallTime24approximateMonotonicTimeEv(const WTF::WallTime* self) {
    if (std::isinf(self->m_value)) {
        return WTF::MonotonicTime{self->m_value};
    }
    return WTF::MonotonicTime{self->m_value - WTF::currentWallTimeSeconds() + WTF::currentMonotonicTimeSeconds()};
}

WTF::TimeWithDynamicClockType APS5_VABI _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(WTF::ClockType type) {
    switch (type) {
    case WTF::ClockType::Wall:
        return WTF::TimeWithDynamicClockType{WTF::currentWallTimeSeconds(), WTF::ClockType::Wall};
    case WTF::ClockType::Monotonic:
        return WTF::TimeWithDynamicClockType{WTF::currentMonotonicTimeSeconds(), WTF::ClockType::Monotonic};
    case WTF::ClockType::Approximate:
        return WTF::TimeWithDynamicClockType{WTF::currentMonotonicTimeSeconds(), WTF::ClockType::Approximate};
    }
    return WTF::TimeWithDynamicClockType{0.0, WTF::ClockType::Wall};
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF24TimeWithDynamicClockType13monotonicTimeEv(const WTF::TimeWithDynamicClockType* self) {
    return WTF::MonotonicTime{self->m_value};
}

WTF::WallTime APS5_VABI _ZNK3WTF24TimeWithDynamicClockType8wallTimeEv(const WTF::TimeWithDynamicClockType* self) {
    return WTF::WallTime{self->m_value};
}

WTF::WallTime APS5_VABI _ZNK3WTF24TimeWithDynamicClockType19approximateWallTimeEv(const WTF::TimeWithDynamicClockType* self) {
    if (self->m_type == WTF::ClockType::Wall) {
        return WTF::WallTime{self->m_value};
    }
    WTF::MonotonicTime mono{self->m_value};
    return _ZNK3WTF13MonotonicTime19approximateWallTimeEv(&mono);
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF24TimeWithDynamicClockType24approximateMonotonicTimeEv(const WTF::TimeWithDynamicClockType* self) {
    if (self->m_type == WTF::ClockType::Monotonic || self->m_type == WTF::ClockType::Approximate) {
        return WTF::MonotonicTime{self->m_value};
    }
    WTF::WallTime wall{self->m_value};
    return _ZNK3WTF8WallTime24approximateMonotonicTimeEv(&wall);
}

WTF::TimeWithDynamicClockType APS5_VABI _ZNK3WTF24TimeWithDynamicClockType16nowWithSameClockEv(const WTF::TimeWithDynamicClockType* self) {
    return _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(self->m_type);
}

WTF::Seconds APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypemiERKS0_(const WTF::TimeWithDynamicClockType* self, const WTF::TimeWithDynamicClockType& other) {
    return WTF::Seconds{self->m_value - other.m_value};
}

bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypegtERKS0_(const WTF::TimeWithDynamicClockType* self, const WTF::TimeWithDynamicClockType& other) {
    return self->m_value > other.m_value;
}

bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypegeERKS0_(const WTF::TimeWithDynamicClockType* self, const WTF::TimeWithDynamicClockType& other) {
    return self->m_value >= other.m_value;
}

bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypeleERKS0_(const WTF::TimeWithDynamicClockType* self, const WTF::TimeWithDynamicClockType& other) {
    return self->m_value <= other.m_value;
}

bool APS5_VABI _ZN3WTF10hasElapsedERKNS_24TimeWithDynamicClockTypeE(const WTF::TimeWithDynamicClockType& time) {
    if (std::isinf(time.m_value)) {
        return false;
    }
    WTF::TimeWithDynamicClockType now = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(time.m_type);
    return time.m_value <= now.m_value;
}

void APS5_VABI _ZN3WTF5sleepERKNS_24TimeWithDynamicClockTypeE(const WTF::TimeWithDynamicClockType& time) {
    WTF::TimeWithDynamicClockType now = _ZN3WTF24TimeWithDynamicClockType3nowENS_9ClockTypeE(time.m_type);
    double diff = time.m_value - now.m_value;
    if (diff > 0.0) {
        std::this_thread::sleep_for(std::chrono::duration<double>(diff));
    }
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF7SecondsmiENS_13MonotonicTimeE(const WTF::Seconds* self, WTF::MonotonicTime other) {
    return WTF::MonotonicTime{self->m_value - other.m_value};
}

WTF::WallTime APS5_VABI _ZNK3WTF7SecondsmiENS_8WallTimeE(const WTF::Seconds* self, WTF::WallTime other) {
    return WTF::WallTime{self->m_value - other.m_value};
}

WTF::TimeWithDynamicClockType APS5_VABI _ZNK3WTF7SecondsmiERKNS_24TimeWithDynamicClockTypeE(const WTF::Seconds* self, const WTF::TimeWithDynamicClockType& other) {
    return WTF::TimeWithDynamicClockType{self->m_value - other.m_value, other.m_type};
}

WTF::MonotonicTime APS5_VABI _ZNK3WTF7SecondsplENS_13MonotonicTimeE(const WTF::Seconds* self, WTF::MonotonicTime other) {
    return WTF::MonotonicTime{self->m_value + other.m_value};
}

WTF::WallTime APS5_VABI _ZNK3WTF7SecondsplENS_8WallTimeE(const WTF::Seconds* self, WTF::WallTime other) {
    return WTF::WallTime{self->m_value + other.m_value};
}

WTF::Seconds APS5_VABI _ZN3WTF7CPUTime16forCurrentThreadEv() {
#ifdef _WIN32
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kernelTime.dwLowDateTime;
        k.HighPart = kernelTime.dwHighDateTime;
        u.LowPart = userTime.dwLowDateTime;
        u.HighPart = userTime.dwHighDateTime;
        double totalSec = static_cast<double>(k.QuadPart + u.QuadPart) * 1e-7;
        return WTF::Seconds{totalSec};
    }
#elif defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
        return WTF::Seconds{static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9};
    }
#endif
    return WTF::Seconds{0.0};
}

double APS5_VABI _ZNK3WTF7CPUTime23percentageCPUUsageSinceERKS0_(const WTF::CPUTime* self, const WTF::CPUTime& reference) {
    double cpuUsageDelta = (self->systemTime.m_value + self->userTime.m_value) - (reference.systemTime.m_value + reference.userTime.m_value);
    double cpuTimeDelta = self->cpuTime.m_value - reference.cpuTime.m_value;
    if (cpuTimeDelta <= 0.0) {
        return 0.0;
    }
    return cpuUsageDelta * 100.0 / cpuTimeDelta;
}

void APS5_VABI _ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv(WTF::GregorianDateTime* self) {
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    TIME_ZONE_INFORMATION tzi;
    DWORD tzId = GetTimeZoneInformation(&tzi);
    LONG bias = 0;
    if (tzId != TIME_ZONE_ID_INVALID) {
        bias = tzi.Bias;
        if (tzId == TIME_ZONE_ID_DAYLIGHT) {
            bias += tzi.DaylightBias;
        } else if (tzId == TIME_ZONE_ID_STANDARD || tzId == TIME_ZONE_ID_UNKNOWN) {
            bias += tzi.StandardBias;
        }
    }
    self->m_year = st.wYear;
    self->m_month = st.wMonth - 1;
    self->m_monthDay = st.wDay;
    self->m_yearDay = WTF::dayInYear(self->m_year, self->m_month, self->m_monthDay);
    self->m_weekDay = st.wDayOfWeek;
    self->m_hour = st.wHour;
    self->m_minute = st.wMinute;
    self->m_second = st.wSecond;
    self->m_utcOffsetInMinute = -bias;
    self->m_isDST = (tzId == TIME_ZONE_ID_DAYLIGHT) ? 1 : 0;
#else
    time_t raw = time(nullptr);
    struct tm localTM;
    localtime_r(&raw, &localTM);
    self->m_year = localTM.tm_year + 1900;
    self->m_month = localTM.tm_mon;
    self->m_monthDay = localTM.tm_mday;
    self->m_yearDay = localTM.tm_yday;
    self->m_weekDay = localTM.tm_wday;
    self->m_hour = localTM.tm_hour;
    self->m_minute = localTM.tm_min;
    self->m_second = localTM.tm_sec;
    self->m_isDST = localTM.tm_isdst;
#if defined(HAVE_TM_GMTOFF) || defined(__GLIBC__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    self->m_utcOffsetInMinute = static_cast<int>(localTM.tm_gmtoff / 60);
#else
    self->m_utcOffsetInMinute = 0;
#endif
#endif
}

static int32_t calculateUTCOffset() {
#ifdef _WIN32
    _tzset();
    long tz = 0;
    if (_get_timezone(&tz) != 0) {
        throw std::runtime_error("calculateUTCOffset: _get_timezone failed");
    }
    return -static_cast<int32_t>(tz * 1000);
#else
    struct tm localt{};
    localt.tm_sec = 0;
    localt.tm_min = 0;
    localt.tm_hour = 0;
    localt.tm_mday = 1;
    localt.tm_mon = 0;
    localt.tm_year = 119;
    localt.tm_isdst = 0;
#if defined(HAVE_TM_GMTOFF) || defined(__GLIBC__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    localt.tm_gmtoff = 0;
#endif
#if defined(HAVE_TM_ZONE)
    localt.tm_zone = nullptr;
#endif
    time_t localTimestamp = mktime(&localt);
    if (localTimestamp == static_cast<time_t>(-1)) {
        throw std::runtime_error("calculateUTCOffset: mktime failed");
    }
    time_t utcOffset = 1546300800 - localTimestamp;
    return static_cast<int32_t>(utcOffset * 1000);
#endif
}

static inline int msToMinutes(double ms) {
    double result = std::fmod(std::floor(ms / 60000.0), 60.0);
    if (result < 0.0) {
        result += 60.0;
    }
    return static_cast<int>(result);
}

static inline int msToHours(double ms) {
    double result = std::fmod(std::floor(ms / 3600000.0), 24.0);
    if (result < 0.0) {
        result += 24.0;
    }
    return static_cast<int>(result);
}

static double calculateDSTOffset(time_t localTime, double utcOffset, const struct tm& localTM) {
    double offsetTime = (static_cast<double>(localTime) * 1000.0) + utcOffset;
    int offsetHour = msToHours(offsetTime);
    int offsetMinute = msToMinutes(offsetTime);
    double diff = (localTM.tm_hour - offsetHour) * 3600.0 + (localTM.tm_min - offsetMinute) * 60.0;
    if (diff < 0.0) {
        diff += 86400.0;
    }
    return diff * 1000.0;
}

WTF::LocalTimeOffset APS5_VABI _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(double ms, WTF::TimeType inputTimeType) {
    if (std::isnan(ms) || std::isinf(ms)) {
        throw std::invalid_argument("calculateLocalTimeOffset: NaN or Inf timestamp");
    }
#if defined(HAVE_TM_GMTOFF) || defined(__GLIBC__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    double localToUTCTimeOffset = (inputTimeType == WTF::TimeType::LocalTime) ? calculateUTCOffset() : 0.0;
    double localTimeSeconds = (ms - localToUTCTimeOffset) / 1000.0;
    constexpr double maxUnixTime = 4102444740.0;
    if (localTimeSeconds > maxUnixTime) {
        localTimeSeconds = maxUnixTime;
    } else if (localTimeSeconds < 0.0) {
        localTimeSeconds += 86400.0;
    }
    constexpr double minRepresentableTimeT = -9223372036854775808.0;
    if (localTimeSeconds < minRepresentableTimeT) {
        throw std::overflow_error("calculateLocalTimeOffset: timestamp out of representable time_t range");
    }
    time_t localTime = static_cast<time_t>(localTimeSeconds);
    struct tm localTM{};
    if (localtime_r(&localTime, &localTM) == nullptr) {
        throw std::overflow_error("calculateLocalTimeOffset: localtime_r failed");
    }
    return WTF::LocalTimeOffset{localTM.tm_isdst > 0, static_cast<int>(localTM.tm_gmtoff * 1000)};
#else
    double localToUTCTimeOffset = calculateUTCOffset();
    if (inputTimeType == WTF::TimeType::LocalTime) {
        ms -= localToUTCTimeOffset;
    }
    double localTimeSeconds = ms / 1000.0;
    constexpr double maxUnixTime = 4102444740.0;
    if (localTimeSeconds > maxUnixTime) {
        localTimeSeconds = maxUnixTime;
    } else if (localTimeSeconds < 0.0) {
        localTimeSeconds += 86400.0;
    }
    constexpr double minRepresentableTimeT = -9223372036854775808.0;
    if (localTimeSeconds < minRepresentableTimeT) {
        throw std::overflow_error("calculateLocalTimeOffset: timestamp out of representable time_t range");
    }
    time_t localTime = static_cast<time_t>(localTimeSeconds);
    struct tm localTM{};
#ifdef _WIN32
    _tzset();
    if (localtime_s(&localTM, &localTime) != 0) {
        throw std::overflow_error("calculateLocalTimeOffset: localtime_s failed");
    }
#else
    if (localtime_r(&localTime, &localTM) == nullptr) {
        throw std::overflow_error("calculateLocalTimeOffset: localtime_r failed");
    }
#endif
    double dstOffset = 0.0;
    if (localTM.tm_isdst > 0) {
        dstOffset = calculateDSTOffset(localTime, localToUTCTimeOffset, localTM);
    }
    return WTF::LocalTimeOffset{localTM.tm_isdst > 0, static_cast<int>(localToUTCTimeOffset + dstOffset)};
#endif
}

double APS5_VABI _ZN3WTF40parseES5DateFromNullTerminatedCharactersEPKcRb(const char* dateString, bool& isLocalTime) {
    isLocalTime = false;
    static const long daysPerMonth[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year = 0;
    long month = 1;
    long day = 1;
    long hours = 0;
    long minutes = 0;
    long seconds = 0;
    double milliseconds = 0.0;
    long timeZoneSeconds = 0;

    const char* current = WTF::parseES5DatePortion(dateString, year, month, day);
    if (!current) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (*current == 'T' || *current == 't' || *current == ' ') {
        current = WTF::parseES5TimePortion(current + 1, hours, minutes, seconds, milliseconds, isLocalTime, timeZoneSeconds);
        if (!current) {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }
    if (*current != '\0') {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (month < 1 || month > 12) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (day < 1 || day > daysPerMonth[month - 1]) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (month == 2 && day > 28 && !WTF::isLeapYear(year)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (hours < 0 || hours > 24) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (hours == 24 && (minutes || seconds)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (minutes < 0 || minutes > 59) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (seconds < 0 || seconds >= 61) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (seconds == 60) {
        milliseconds = 0.0;
    }
    return WTF::ymdhmsToMilliseconds(year, month, day, hours, minutes, seconds, milliseconds) - (timeZoneSeconds * 1000.0);
}

}
