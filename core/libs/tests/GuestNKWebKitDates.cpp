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

int main() {
    LegacyDates();
    UndefinedInWebKit();
}
