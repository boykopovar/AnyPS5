#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

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

static void UndefinedInWebKit() {
    bool isLocalTime = false;
    Require(Throws<std::overflow_error>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("2000/12/9223372036854775800", &isLocalTime); }));
    Require(Throws<std::overflow_error>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("Jan 1 2000 GMT+05:2147483600", &isLocalTime); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb(nullptr, &isLocalTime); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb("2000", nullptr); }));
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
    UndefinedInWebKit();
    RFC2822Dates();
}
