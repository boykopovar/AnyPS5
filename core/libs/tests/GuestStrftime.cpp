#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {

bool correct = true;

GuestTm MakeTime() {
    GuestTm time{};
    time.tm_year = 126;
    time.tm_mon = 9;
    time.tm_mday = 2;
    time.tm_hour = 13;
    time.tm_min = 5;
    time.tm_sec = 9;
    time.tm_wday = 5;
    time.tm_yday = 274;
    time.tm_gmtoff = 19800;
    time.tm_zone = "XYZ";
    return time;
}

void Check(const char* name, const GuestTm& time, const char* format, const char* expected) {
    char text[256];
    const std::size_t length = strftime_nid_postfix(text, sizeof(text), format, &time);
    if (length != std::strlen(expected) || std::strcmp(text, expected) != 0) {
        std::fprintf(stderr, "strftime %s (\"%s\"): expected \"%s\", received \"%s\" (%zu)\n", name, format, expected, length == 0 ? "" : text, length);
        correct = false;
    }
}

void CheckAsctime(const char* name, const GuestTm& time, const char* expected) {
    const char* result = asctime_nid_postfix(&time);
    if (std::strcmp(result, expected) != 0) {
        std::fprintf(stderr, "asctime %s: expected \"%s\", received \"%s\"\n", name, expected, result);
        correct = false;
    }
}

}

int main() {
    if (setenv_nid_postfix("TZ", "UTC+3", 1) != 0) return 1;
    tzset_nid_postfix();
    const GuestTm base = MakeTime();
    Check("names", base, "%a|%A|%b|%B|%h", "Fri|Friday|Oct|October|Oct");
    Check("composites", base, "%c|%D|%x|%X|%T|%r|%R|%F", "Fri Oct  2 13:05:09 2026|10/02/26|10/02/26|13:05:09|13:05:09|01:05:09 PM|13:05|2026-10-02");
    Check("numbers", base, "%C|%d|%e|%g|%G|%H|%I|%j|%m|%M|%p|%S|%u|%U|%V|%w|%W|%y|%Y", "20|02| 2|26|2026|13|01|275|10|05|PM|09|5|39|40|5|39|26|2026");
    Check("whitespace and percent", base, "%n%t%%", "\n\t%");
    Check("alternative forms", base, "%Ec|%EC|%Ex|%EX|%Ey|%EY|%Od|%Oe|%OH|%OI|%Om|%OM|%OS|%Ou|%OU|%OV|%Ow|%OW|%Oy|%Ez", "Fri Oct  2 13:05:09 2026|20|10/02/26|13:05:09|26|2026|02| 2|13|01|10|05|09|5|39|40|5|39|26|-0300");
    Check("other conversions", base, "%k|%l|%P|%s|%+|%v|%Q|%q", "%k|%l|%P|%s|%+|%v|%Q|%q");
    Check("flags and widths", base, "%-d|%_d|%0e|%^a|%#b|%10Y", "%-d|%_d|%0e|%^a|%#b|%10Y");
    Check("modifier before an unknown conversion", base, "%Ea|%Oa|%EE|%OO|%Ek|%EY", "Fri|Fri|%E|%O|%k|2026");
    Check("trailing percent", base, "ab%", "ab%");
    Check("zone", base, "%z|%Z", "-0300|UTC");

    GuestTm zone = base;
    zone.tm_isdst = 1;
    zone.tm_gmtoff = -1;
    Check("zone in daylight time", zone, "%z", "-0200");
    zone.tm_isdst = -1;
    Check("zone with unknown daylight time", zone, "%z", "-0300");

    for (int value = -3; value <= 14; ++value) {
        GuestTm time = base;
        time.tm_wday = value;
        time.tm_mon = value;
        static const char* const expected[] = {
            "Sun|Sun|Jan|Jan|00|0|0", "Sun|Sun|Jan|Jan|00|0|0", "Sun|Sun|Jan|Jan|00|0|0", "Sun|Sunday|Jan|January|01|7|0",
            "Mon|Monday|Feb|February|02|1|1", "Tue|Tuesday|Mar|March|03|2|2", "Wed|Wednesday|Apr|April|04|3|3", "Thu|Thursday|May|May|05|4|4",
            "Fri|Friday|Jun|June|06|5|5", "Sat|Saturday|Jul|July|07|6|6", "||Aug|August|08|7|7", "||Sep|September|09|8|8",
            "||Oct|October|10|9|9", "||Nov|November|11|0|0", "||Dec|December|12|1|1", "||||13|2|2", "||||14|3|3", "||||15|4|4",
        };
        char name[24];
        std::snprintf(name, sizeof(name), "index %d", value);
        Check(name, time, "%a|%A|%b|%B|%m|%u|%w", expected[value + 3]);
    }

    struct Field {
        int value;
        const char* expected;
    };
    constexpr Field fields[] = {
        {-25, "00|00|AM|00|00|00| 0|000|00|00"}, {-13, "00|00|AM|00|00|00| 0|000|00|00"}, {-12, "00|00|AM|00|00|00| 0|000|00|00"},
        {-1, "00|11|AM|00|00|00| 0|000|00|00"}, {0, "00|12|AM|00|00|00| 0|001|00|00"}, {1, "01|01|AM|01|01|01| 1|002|00|00"},
        {11, "11|11|AM|11|11|11|11|012|01|02"}, {12, "12|12|PM|12|12|12|12|013|02|02"}, {13, "13|01|PM|13|13|13|13|014|02|02"},
        {25, "25|01|PM|25|25|25|25|026|03|04"}, {99, "99|03|PM|99|99|99|99|100|14|14"}, {100, "00|04|PM|00|00|00| 0|101|14|14"},
    };
    for (const auto& field : fields) {
        GuestTm time = base;
        time.tm_hour = field.value;
        time.tm_min = field.value;
        time.tm_sec = field.value;
        time.tm_mday = field.value;
        time.tm_yday = field.value;
        char name[24];
        std::snprintf(name, sizeof(name), "field %d", field.value);
        Check(name, time, "%H|%I|%p|%M|%S|%d|%e|%j|%U|%W", field.expected);
    }

    constexpr Field numbers[] = {
        {-10, "00| 0|000|00|00|00"}, {9, "09| 9|010|09|09|09"}, {101, "01| 1|102|01|01|01"}, {999, "99|99|000|99|99|99"},
        {1000, "00| 0|001|00|00|00"}, {123456, "56|56|457|56|56|56"},
    };
    for (const auto& field : numbers) {
        GuestTm time = base;
        time.tm_hour = field.value;
        time.tm_min = field.value;
        time.tm_sec = field.value;
        time.tm_mday = field.value;
        time.tm_yday = field.value;
        char name[24];
        std::snprintf(name, sizeof(name), "number %d", field.value);
        Check(name, time, "%d|%e|%j|%H|%M|%S", field.expected);
    }
    GuestTm leap = base;
    leap.tm_sec = 61;
    Check("leap second", leap, "%S|%T", "61|13:05:61");

    struct Year {
        int tmYear;
        const char* expected;
    };
    constexpr Year years[] = {
        {-14245, "0000|00|55|0000|55"}, {-11900, "0000|00|00|0000|00"}, {-2001, "0000|00|99|0000|99"}, {-1999, "0000|00|01|0000|01"},
        {-1901, "0000|00|99|0000|99"}, {-1900, "0000|00|00|0000|00"}, {-1899, "0001|01|01|0001|01"}, {-1800, "0100|01|00|0100|00"},
        {-1, "1899|19|99|1899|99"}, {0, "1900|19|00|1900|00"}, {8099, "9999|99|99|9999|99"}, {8100, "0000|00|00|0000|00"},
        {8101, "0001|00|01|0001|01"}, {10099, "1999|19|99|1999|99"}, {10445, "2345|23|45|2345|45"},
    };
    for (const auto& year : years) {
        GuestTm time = base;
        time.tm_year = year.tmYear;
        char name[24];
        std::snprintf(name, sizeof(name), "year %d", year.tmYear);
        Check(name, time, "%Y|%C|%y|%G|%g", year.expected);
    }

    struct Iso {
        int year, mon, mday, wday, yday;
        const char* expected;
    };
    constexpr Iso weeks[] = {
        {126, 0, 1, 4, 0, "2026|26|01|00|00"}, {126, 0, 4, 0, 3, "2026|26|01|01|00"}, {126, 0, 5, 1, 4, "2026|26|02|01|01"},
        {120, 11, 31, 4, 365, "2020|20|53|52|52"}, {121, 0, 3, 0, 2, "2020|20|53|01|00"}, {121, 0, 4, 1, 3, "2021|21|01|01|01"},
        {124, 11, 30, 1, 364, "2025|25|01|52|53"}, {104, 11, 31, 5, 365, "2004|04|53|52|52"}, {105, 0, 2, 0, 1, "2004|04|53|01|00"},
        {126, 0, 1, 9, 10, "2026|26|02|02|02"}, {126, 0, 1, -2, 10, "2026|26|02|01|01"}, {-1901, 0, 1, 4, 0, "0000|99|01|00|00"},
    };
    for (const auto& week : weeks) {
        GuestTm time = base;
        time.tm_year = week.year;
        time.tm_mon = week.mon;
        time.tm_mday = week.mday;
        time.tm_wday = week.wday;
        time.tm_yday = week.yday;
        char name[48];
        std::snprintf(name, sizeof(name), "week %d/%d wday %d yday %d", week.year, week.mon, week.wday, week.yday);
        Check(name, time, "%G|%g|%V|%U|%W", week.expected);
    }

    constexpr Iso outOfRangeWeeks[] = {
        {126, 0, 11, -2, 10, "01|01"}, {126, 1, 5, 4, 400, "57|57"}, {126, 3, 11, -19, 100, "16|16"}, {126, 9, 5, -16, 277, "40|41"},
        {126, 0, 19, -12, 18, "04|04"}, {126, 0, 1, 6, 1365, "95|95"}, {97, 0, 1, 6, 415, "59|59"}, {113, 11, 31, 1, 365, "53|53"},
        {7, 11, 31, 1, 365, "53|53"}, {126, 9, 24, 6, 296, "42|42"},
    };
    for (const auto& week : outOfRangeWeeks) {
        GuestTm time = base;
        time.tm_year = week.year;
        time.tm_mon = week.mon;
        time.tm_mday = week.mday;
        time.tm_wday = week.wday;
        time.tm_yday = week.yday;
        char name[48];
        std::snprintf(name, sizeof(name), "week number wday %d yday %d", week.wday, week.yday);
        Check(name, time, "%U|%W", week.expected);
    }

    char text[8];
    std::memset(text, '#', sizeof(text));
    if (strftime_nid_postfix(text, 5, "%Y", &base) != 4 || std::strcmp(text, "2026") != 0) {
        std::fprintf(stderr, "strftime does not fill a buffer that holds the result and its terminator\n");
        correct = false;
    }
    std::memset(text, '#', sizeof(text));
    if (strftime_nid_postfix(text, 4, "%Y", &base) != 0 || std::memcmp(text, "2026#", 5) != 0) {
        std::fprintf(stderr, "strftime with no room for the terminator does not return 0 after writing the conversion\n");
        correct = false;
    }
    std::memset(text, '#', sizeof(text));
    if (strftime_nid_postfix(text, 5, "ab%Ycd", &base) != 0 || std::memcmp(text, "ab####", 6) != 0) {
        std::fprintf(stderr, "strftime writes part of a conversion that does not fit\n");
        correct = false;
    }
    std::memset(text, '#', sizeof(text));
    if (strftime_nid_postfix(text, 8, "%t%D", &base) != 0 || std::memcmp(text, "\t10/02/#", 8) != 0) {
        std::fprintf(stderr, "strftime does not write the parts of a composite conversion that fit\n");
        correct = false;
    }
    std::memset(text, '#', sizeof(text));
    if (strftime_nid_postfix(text, 2, "%X%#H", &base) != 0 || std::memcmp(text, "13#", 3) != 0) {
        std::fprintf(stderr, "strftime does not stop a composite conversion at the first part that does not fit\n");
        correct = false;
    }
    std::memset(text, '#', sizeof(text));
    if (strftime_nid_postfix(text, 3, "abc", &base) != 0 || std::memcmp(text, "abc#", 4) != 0) {
        std::fprintf(stderr, "strftime does not write the literals that fit\n");
        correct = false;
    }
    std::memset(text, '#', sizeof(text));
    if (strftime_nid_postfix(text, 0, "%Y", &base) != 0 || text[0] != '#') {
        std::fprintf(stderr, "strftime writes to a zero-size buffer\n");
        correct = false;
    }
    bool threw = false;
    try {
        strftime_nid_postfix(text, sizeof(text), "%E", &base);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) {
        std::fprintf(stderr, "strftime does not throw for a format ending after a modifier\n");
        correct = false;
    }

    CheckAsctime("base", base, "Fri Oct  2 13:05:09 2026\n");
    GuestTm negative = base;
    negative.tm_wday = -1;
    negative.tm_mon = -1;
    CheckAsctime("negative names", negative, "Sun Jan  2 13:05:09 2026\n");
    GuestTm past = base;
    past.tm_wday = 7;
    past.tm_mon = 12;
    CheckAsctime("names past the tables", past, "   2 13:05:09 2026\n");
    GuestTm big = base;
    big.tm_mday = 40;
    big.tm_hour = 25;
    big.tm_min = 61;
    big.tm_sec = 61;
    CheckAsctime("large fields", big, "Fri Oct 40 25:61:61 2026\n");
    big.tm_mday = 123456;
    big.tm_hour = -10;
    CheckAsctime("wrapped and negative fields", big, "Fri Oct 56 00:61:61 2026\n");
    GuestTm year = base;
    year.tm_year = 8100;
    CheckAsctime("year 10000", year, "Fri Oct  2 13:05:09 0000\n");
    year.tm_year = -1901;
    CheckAsctime("year -1", year, "Fri Oct  2 13:05:09 0000\n");
    return correct ? 0 : 1;
}
