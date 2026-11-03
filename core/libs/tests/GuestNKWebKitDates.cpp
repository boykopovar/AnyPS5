#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF date parsing check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool Same(double first, double second) {
    return std::memcmp(&first, &second, sizeof(double)) == 0;
}

static void Date(const char* text, double expected, bool expectedLocal) {
    bool isLocalTime = !expectedLocal;
    Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb(text, &isLocalTime), expected));
    Require(isLocalTime == expectedLocal);
}

static void DateInvalid(const char* text) {
    bool isLocalTime = false;
    Require(std::isnan(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb(text, &isLocalTime)));
}

template<typename TException, typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

struct ScopedTimeZone {
    explicit ScopedTimeZone(const char* zone) {
        const char* original = std::getenv("TZ");
        if (original != nullptr) {
            saved = original;
            hadZone = true;
        }
#ifdef _WIN32
        savedBias = *__dstbias();
#endif
        apply(zone);
    }

    ~ScopedTimeZone() {
        if (hadZone) {
            apply(saved.c_str());
        } else {
            clear();
        }
#ifdef _WIN32
        *__dstbias() = savedBias;
#endif
    }

    ScopedTimeZone(const ScopedTimeZone&) = delete;
    ScopedTimeZone& operator=(const ScopedTimeZone&) = delete;

private:
    static void apply(const char* zone) {
#ifdef _WIN32
        _putenv((std::string("TZ=") + zone).c_str());
        _tzset();
        *__dstbias() = -3600;
#else
        setenv("TZ", zone, 1);
        tzset();
#endif
    }

    static void clear() {
#ifdef _WIN32
        _putenv("TZ=");
        _tzset();
#else
        unsetenv("TZ");
        tzset();
#endif
    }

    std::string saved{};
    bool hadZone{false};
#ifdef _WIN32
    long savedBias{0};
#endif
};

static void LegacyDates() {
    Date("Sat, 01 Jan 2000 08:00:00 GMT", 946713600000.0, false);
    Date("Tuesday, 09-Nov-99 23:12:40 GMT", 942189160000.0, false);
    Date("01 Jan 99 22:00 +0100", 915224400000.0, false);
    Date("January 09 1999 23:12:40 GMT", 915923560000.0, false);
    Date("Wednesday January 09 23:12:40 GMT 1999", 915923560000.0, false);
    Date("12/25/2000", 977702400000.0, true);
    Date("2000/12/25 10:30 pm EST", 977801400000.0, false);
    Date("Jan 1 2000 12:30 am", 946686600000.0, true);
    Date("Feb 29", 951782400000.0, true);
    Date("Jan 1 49", 2493072000000.0, true);
    Date("Jan 1 50", -631152000000.0, true);
    Date("Mon Jan 1 2000 10:00:00 GMT+05:30", 946701000000.0, false);
    Date("Jan 1 2000 (comment) 10:00 UTC", 946720800000.0, false);
    Date("Jan 1 2000 GMT+05:2147483000", -127902313200000.0, false);
    DateInvalid("");
    DateInvalid("foo");
    DateInvalid("Jan 1 2000 garbage");
    DateInvalid("Jan 32 2000");
    DateInvalid("13/01/2000");
    DateInvalid("Jan 1 2000 13:00 pm");
    DateInvalid("Jan 1 2000 GMT+9960");
}

static void SingleArgumentDates() {
    Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Sat, 01 Jan 2000 08:00:00 GMT"), 946713600000.0));
    Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Tuesday, 09-Nov-99 23:12:40 GMT"), 942189160000.0));
    Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("01 Jan 99 22:00 +0100"), 915224400000.0));
    Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Mon Jan 1 2000 10:00:00 GMT+05:30"), 946701000000.0));

    {
        ScopedTimeZone tz("EST5EDT,M3.2.0/2,M11.1.0/2");

        Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("01/01/2026 12:00"), 1767286800000.0));
        Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("07/01/2026 12:00"), 1782921600000.0));
        Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Jan 1 2026 12:00"), 1767286800000.0));
        Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Jul 1 2026 12:00"), 1782921600000.0));
        Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Jan 1 2000 12:30 am"), 946704600000.0));

        Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("01/01/2026 12:00 GMT"), 1767268800000.0));
        Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("07/01/2026 12:00 GMT"), 1782907200000.0));
        Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Jan 1 2026 12:00 GMT"), 1767268800000.0));
        Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Jul 1 2026 12:00 GMT"), 1782907200000.0));

        WTF::LocalTimeOffset at = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(4102444740000.0, WTF::TimeType::UTCTime);
        WTF::LocalTimeOffset above = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(4102444741000.0, WTF::TimeType::UTCTime);
        WTF::LocalTimeOffset below = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(4102444739000.0, WTF::TimeType::UTCTime);
        WTF::LocalTimeOffset clampedHuge = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(1e25, WTF::TimeType::UTCTime);
        WTF::LocalTimeOffset clampedInt64 = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(static_cast<double>(std::numeric_limits<std::time_t>::max()) * 1000.0, WTF::TimeType::UTCTime);
        Require(at.offset == -18000000 && !at.isDST);
        Require(above.offset == at.offset && above.isDST == at.isDST);
        Require(below.offset == at.offset && below.isDST == at.isDST);
        Require(clampedHuge.offset == at.offset && clampedHuge.isDST == at.isDST);
        Require(clampedInt64.offset == at.offset && clampedInt64.isDST == at.isDST);

        WTF::LocalTimeOffset epochUtc = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(0.0, WTF::TimeType::UTCTime);
        WTF::LocalTimeOffset afterEpochUtc = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(1000.0, WTF::TimeType::UTCTime);
        WTF::LocalTimeOffset beforeEpochUtc = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(-1000.0, WTF::TimeType::UTCTime);
        WTF::LocalTimeOffset dayBeforeEpochUtc = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(-86400000.0, WTF::TimeType::UTCTime);
        Require(epochUtc.offset == -18000000 && !epochUtc.isDST);
        Require(afterEpochUtc.offset == -18000000 && !afterEpochUtc.isDST);
        Require(beforeEpochUtc.offset == -18000000 && !beforeEpochUtc.isDST);
        Require(dayBeforeEpochUtc.offset == -18000000 && !dayBeforeEpochUtc.isDST);

        WTF::LocalTimeOffset epochLocal = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(0.0, WTF::TimeType::LocalTime);
        WTF::LocalTimeOffset beforeEpochLocal = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(-1000.0, WTF::TimeType::LocalTime);
        WTF::LocalTimeOffset crossDateLocal = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(-19000000.0, WTF::TimeType::LocalTime);
        Require(epochLocal.offset == -18000000 && !epochLocal.isDST);
        Require(beforeEpochLocal.offset == -18000000 && !beforeEpochLocal.isDST);
        Require(crossDateLocal.offset == -18000000 && !crossDateLocal.isDST);
    }

    Require(std::isnan(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("")));
    Require(std::isnan(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("foo")));
    Require(std::isnan(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Jan 1 2000 garbage")));
    Require(std::isnan(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("Jan 32 2000")));
    Require(std::isnan(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("13/01/2000")));
}

static void UndefinedInWebKit() {
    bool isLocalTime = false;
    Require(Throws<std::overflow_error>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("2000/12/9223372036854775800", &isLocalTime); }));
    Require(Throws<std::overflow_error>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("Jan 1 2000 GMT+05:2147483600", &isLocalTime); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb(nullptr, &isLocalTime); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("2000", nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKc(nullptr); }));
    Require(Throws<std::overflow_error>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKc("2000/12/9223372036854775800"); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(std::numeric_limits<double>::quiet_NaN(), WTF::TimeType::UTCTime); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(std::numeric_limits<double>::infinity(), WTF::TimeType::UTCTime); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(-std::numeric_limits<double>::infinity(), WTF::TimeType::UTCTime); }));
    Require(Throws<std::overflow_error>([&] { _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(-9223372036854777905152.0, WTF::TimeType::UTCTime); }));
    Require(Throws<std::overflow_error>([&] { _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(-1e25, WTF::TimeType::UTCTime); }));
}

int main() {
    LegacyDates();
    SingleArgumentDates();
    UndefinedInWebKit();
}
