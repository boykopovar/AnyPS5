#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/Api.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

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

struct ScopedGuestTimeZone {
    ScopedGuestTimeZone(const char* appliedZone, const char* pendingZone) {
        if (const char* original = getenv_nid_postfix("TZ")) {
            saved = original;
            hadZone = true;
        }
        Require(setenv_nid_postfix("TZ", appliedZone, 1) == 0);
        tzset_nid_postfix();
        Require(setenv_nid_postfix("TZ", pendingZone, 1) == 0);
    }

    ~ScopedGuestTimeZone() {
        if (hadZone) {
            setenv_nid_postfix("TZ", saved.c_str(), 1);
        } else {
            unsetenv_nid_postfix("TZ");
        }
        tzset_nid_postfix();
    }

    ScopedGuestTimeZone(const ScopedGuestTimeZone&) = delete;
    ScopedGuestTimeZone& operator=(const ScopedGuestTimeZone&) = delete;

private:
    std::string saved{};
    bool hadZone{false};
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
        ScopedGuestTimeZone tz("JST-9", "EST5EDT,M3.2.0/2,M11.1.0/2");

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

static void LocalTimeOffsetFollowsGuestZone() {
    {
        ScopedGuestTimeZone tz("JST-9", "EST5EDT,M3.2.0/2,M11.1.0/2");
        WTF::LocalTimeOffset summer = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(1782907200000.0, WTF::TimeType::UTCTime);
        Require(summer.offset == -14400000 && summer.isDST);
    }
    {
        ScopedGuestTimeZone tz("JST-9", "EST5EDT,M3.2.0/2,M11.1.0/2");
        WTF::LocalTimeOffset afterSpringForward = _ZN3WTF24calculateLocalTimeOffsetEdNS_8TimeTypeE(1772971200000.0, WTF::TimeType::LocalTime);
        Require(afterSpringForward.offset == -14400000 && afterSpringForward.isDST);
    }
}

static void CurrentLocalTime() {
    ScopedGuestTimeZone tz("JST-9", "EST5EDT,M3.2.0/2,M11.1.0/2");
    for (int attempt = 0;; ++attempt) {
        Require(attempt < 100);
        WTF::GregorianDateTime current{};
        const std::int64_t before = time_nid_postfix(nullptr);
        _ZN3WTF17GregorianDateTime21setToCurrentLocalTimeEv(&current);
        const std::int64_t after = time_nid_postfix(nullptr);
        if (before != after) {
            continue;
        }
        GuestTm expected{};
        Require(localtime_r_nid_postfix(&before, &expected) != nullptr);
        Require(current.m_year == expected.tm_year + 1900);
        Require(current.m_month == expected.tm_mon);
        Require(current.m_yearDay == expected.tm_yday);
        Require(current.m_monthDay == expected.tm_mday);
        Require(current.m_weekDay == expected.tm_wday);
        Require(current.m_hour == expected.tm_hour);
        Require(current.m_minute == expected.tm_min);
        Require(current.m_second == expected.tm_sec);
        Require(current.m_isDST == expected.tm_isdst);
        Require(current.m_utcOffsetInMinute == expected.tm_gmtoff / 60);
        Require(current.m_utcOffsetInMinute == (expected.tm_isdst > 0 ? -240 : -300));
        return;
    }
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

static void LocalTimePointerValidation() {
    bool local = false;
    Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("12/25/2000", &local), 977702400000.0));
    Require(local);

    local = true;
    Require(Same(_ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("Sat, 01 Jan 2000 08:00:00 GMT", &local), 946713600000.0));
    Require(!local);

    auto* dangling = reinterpret_cast<bool*>(0x10);
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("2000", dangling); }));
#ifdef _WIN32
    auto* readOnly = static_cast<bool*>(VirtualAlloc(nullptr, sizeof(bool), MEM_RESERVE | MEM_COMMIT, PAGE_READONLY));
    Require(readOnly != nullptr);
#else
    auto* readOnly = static_cast<bool*>(mmap(nullptr, sizeof(bool), PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0));
    Require(readOnly != MAP_FAILED);
#endif
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("2000", readOnly); }));
#ifdef _WIN32
    Require(VirtualFree(readOnly, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(readOnly, sizeof(bool)) == 0);
#endif
}

static void CheckRFC2822(
    unsigned dayOfWeek,
    unsigned day,
    unsigned month,
    unsigned year,
    unsigned hours,
    unsigned minutes,
    unsigned seconds,
    int utcOffset,
    const char* expected) {
    auto result = _ZN3WTF21makeRFC2822DateStringEjjjjjjji(dayOfWeek, day, month, year, hours, minutes, seconds, utcOffset);
    Require(result.impl != nullptr);
    Require((result.impl->hashAndFlags & 4) != 0);
    Require(result.impl->length == std::strlen(expected));
    Require(std::memcmp(result.impl->data, expected, result.impl->length) == 0);
}

static void RFC2822Dates() {
    CheckRFC2822(0, 1, 0, 2001, 0, 0, 0, 0, "Mon, 1 Jan 2001 00:00:00 -0000");
    CheckRFC2822(1, 2, 1, 2002, 1, 2, 3, 60, "Tue, 2 Feb 2002 01:02:03 +0100");
    CheckRFC2822(2, 3, 2, 2003, 10, 20, 30, -60, "Wed, 3 Mar 2003 10:20:30 -0100");
    CheckRFC2822(3, 4, 3, 2004, 8, 5, 9, 330, "Thu, 4 Apr 2004 08:05:09 +0530");
    CheckRFC2822(4, 5, 4, 2005, 23, 59, 59, -480, "Fri, 5 May 2005 23:59:59 -0800");
    CheckRFC2822(5, 6, 5, 2006, 12, 34, 56, 720, "Sat, 6 Jun 2006 12:34:56 +1200");
    CheckRFC2822(6, 7, 6, 2007, 7, 8, 9, -720, "Sun, 7 Jul 2007 07:08:09 -1200");
    CheckRFC2822(0, 8, 7, 2008, 10, 0, 0, 780, "Mon, 8 Aug 2008 10:00:00 +1300");
    CheckRFC2822(1, 9, 8, 2009, 11, 0, 0, 840, "Tue, 9 Sep 2009 11:00:00 +1400");
    CheckRFC2822(2, 10, 9, 2010, 12, 0, 0, 5999, "Wed, 10 Oct 2010 12:00:00 +9959");
    CheckRFC2822(3, 11, 10, 2011, 13, 0, 0, -5999, "Thu, 11 Nov 2011 13:00:00 -9959");
    CheckRFC2822(0, 15, 7, 2008, 0, 0, 0, 0, "Mon, 15 Aug 2008 00:00:00 -0000");
    CheckRFC2822(1, 16, 8, 2009, 0, 0, 0, 0, "Tue, 16 Sep 2009 00:00:00 -0000");
    CheckRFC2822(2, 17, 9, 2010, 0, 0, 0, 0, "Wed, 17 Oct 2010 00:00:00 -0000");
    CheckRFC2822(3, 18, 10, 2011, 0, 0, 0, 0, "Thu, 18 Nov 2011 00:00:00 -0000");
    CheckRFC2822(4, 19, 11, 2012, 0, 0, 0, 0, "Fri, 19 Dec 2012 00:00:00 -0000");
    CheckRFC2822(6, 31, 11, 2026, 23, 59, 59, 0, "Sun, 31 Dec 2026 23:59:59 -0000");

    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(7, 1, 0, 2001, 0, 0, 0, 0); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 0, 0, 2001, 0, 0, 0, 0); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 32, 0, 2001, 0, 0, 0, 0); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 1, 12, 2001, 0, 0, 0, 0); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 1, 0, 2001, 24, 0, 0, 0); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 1, 0, 2001, 0, 60, 0, 0); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 1, 0, 2001, 0, 0, 60, 0); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 1, 0, 2001, 0, 0, 0, -6000); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 1, 0, 2001, 0, 0, 0, 6000); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 1, 0, 2001, 0, 0, 0, std::numeric_limits<int>::min()); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 1, 0, 2001, 0, 0, 0, std::numeric_limits<int>::max()); }));

    {
        auto original = _ZN3WTF21makeRFC2822DateStringEjjjjjjji(0, 1, 0, 2026, 12, 0, 0, 0);
        auto copy = original;
        Require(original.impl != nullptr);
        Require(copy.impl == original.impl);
    }
}

int main() {
    LegacyDates();
    SingleArgumentDates();
    LocalTimeOffsetFollowsGuestZone();
    CurrentLocalTime();
    UndefinedInWebKit();
    LocalTimePointerValidation();
    RFC2822Dates();
}
