#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>

extern "C" {
std::size_t APS5_VABI strftime_nid_postfix(char*, std::size_t, const char*, const GuestTm*);
int APS5_VABI setenv_nid_postfix(const char*, const char*, int);
void APS5_VABI tzset_nid_postfix();
char* APS5_VABI asctime_nid_postfix(const GuestTm*);
}

namespace {

GuestTm MakeTime(int year, int hour, int wday, int yday, int isdst) {
    GuestTm time{};
    time.tm_sec = 5;
    time.tm_min = 4;
    time.tm_hour = hour;
    time.tm_mday = 2;
    time.tm_mon = 0;
    time.tm_year = year;
    time.tm_wday = wday;
    time.tm_yday = yday;
    time.tm_isdst = isdst;
    return time;
}

}

int main() {
    if (setenv_nid_postfix("TZ", "IST-5:30", 1) != 0) return 1;
    tzset_nid_postfix();
    struct Case {
        const char* name;
        int year;
        int hour;
        int wday;
        int yday;
        int isdst;
        const char* format;
        const char* expected;
    };
    constexpr Case cases[] = {
        {"alternative", 126, 3, 5, 1, 0, "%Ea %OX %OD %EF %EEY", "Fri 03:04:05 01/02/26 2026-01-02 EY"},
        {"padding", 126, 3, 5, 1, 0, "%-d|%_d|%0e|%-k|%_+|%--d", "2| 2|02|3|Fri Jan  2 03:04:05 IST 2026|-d"},
        {"zone", 126, 3, 5, 1, 0, "%z %Z %0z", "+0530 IST +0530"},
        {"year 899", -1001, 3, 5, 1, 0, "%Y %C %y %G %g %F", "0899 08 99 0899 99 0899-01-02"},
        {"year -1", -1901, 3, 5, 1, 0, "%Y %C %y %c", "-001 -0 01 Fri Jan  2 03:04:05 -001"},
        {"year 12345", 10445, 3, 5, 1, 0, "%Y %C %y %D %v", "12345 123 45 01/02/45  2-Jan-12345"},
        {"out of range", 126, 100, 8, 400, 0, "%I %l %u %W %U %j %a %p", "04  4 8 57 57 401 ? PM"},
        {"unknown dst", 126, 3, 5, 1, -1, "[%z][%Z]", "[][]"},
        {"literals", 126, 3, 5, 1, 0, "%q %% %", "q % %"},
    };
    bool correct = true;
    for (const auto& test : cases) {
        const GuestTm time = MakeTime(test.year, test.hour, test.wday, test.yday, test.isdst);
        char text[128];
        const std::size_t length = strftime_nid_postfix(text, sizeof(text), test.format, &time);
        if (length != std::strlen(test.expected) || std::strcmp(text, test.expected) != 0) {
            std::fprintf(stderr, "strftime %s (\"%s\"): expected \"%s\", received \"%s\" (%zu)\n",
                test.name, test.format, test.expected, length == 0 ? "" : text, length);
            correct = false;
        }
    }
    const GuestTm now = MakeTime(126, 3, 5, 1, 0);
    char text[32];
    if (strftime_nid_postfix(text, 10, "%Y-%m-%d", &now) != 0 || strftime_nid_postfix(text, 11, "%Y-%m-%d", &now) != 10) {
        std::fprintf(stderr, "strftime does not return 0 when the result and its terminator do not fit\n");
        correct = false;
    }
    if (strftime_nid_postfix(text, sizeof(text), nullptr, &now) == 0 || std::strcmp(text, "Fri Jan  2 03:04:05 2026") != 0) {
        std::fprintf(stderr, "strftime with a null format does not format %%c\n");
        correct = false;
    }
    struct AsctimeCase {
        int year;
        const char* expected;
    };
    constexpr AsctimeCase asctimeCases[] = {
        {126, "Fri Jan  2 03:04:05 2026\n"},
        {-1001, "Fri Jan  2 03:04:05 0899\n"},
        {-1901, "Fri Jan  2 03:04:05 -001\n"},
        {10445, "Fri Jan  2 03:04:05     12345\n"},
        {-2147483647 - 1, "Fri Jan  2 03:04:05     -2147481748\n"},
    };
    for (const auto& test : asctimeCases) {
        const GuestTm time = MakeTime(test.year, 3, 5, 1, 0);
        const char* result = asctime_nid_postfix(&time);
        if (std::strcmp(result, test.expected) != 0) {
            std::fprintf(stderr, "asctime of tm_year %d: expected \"%s\", received \"%s\"\n", test.year, test.expected, result);
            correct = false;
        }
    }
    return correct ? 0 : 1;
}
