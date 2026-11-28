#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/General.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t MaxInputLength = 65536;
constexpr std::size_t MaxFormatLength = 4096;

struct MonthName {
    std::string_view abbrev;
    std::string_view full;
    int index;
};

constexpr MonthName Months[] = {
    {"jan", "january", 0},
    {"feb", "february", 1},
    {"mar", "march", 2},
    {"apr", "april", 3},
    {"may", "may", 4},
    {"jun", "june", 5},
    {"jul", "july", 6},
    {"aug", "august", 7},
    {"sep", "september", 8},
    {"oct", "october", 9},
    {"nov", "november", 10},
    {"dec", "december", 11},
};

struct WeekdayName {
    std::string_view abbrev;
    std::string_view full;
    int index;
};

constexpr WeekdayName Weekdays[] = {
    {"sun", "sunday", 0},
    {"mon", "monday", 1},
    {"tue", "tuesday", 2},
    {"wed", "wednesday", 3},
    {"thu", "thursday", 4},
    {"fri", "friday", 5},
    {"sat", "saturday", 6},
};

bool MatchesCaseInsensitive(const char*& buf, std::string_view target) {
    const char* p = buf;
    for (char c : target) {
        if (*p == '\0' || std::tolower(static_cast<unsigned char>(*p)) != std::tolower(static_cast<unsigned char>(c))) {
            return false;
        }
        ++p;
    }
    buf = p;
    return true;
}

bool ParseDigits(const char*& buf, int minDigits, int maxDigits, int minVal, int maxVal, int& out) {
    const char* p = buf;
    int value = 0;
    int digits = 0;
    while (digits < maxDigits && *p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        ++p;
        ++digits;
    }
    if (digits < minDigits || value < minVal || value > maxVal) {
        return false;
    }
    buf = p;
    out = value;
    return true;
}

bool ParseSignedInt64(const char*& buf, std::int64_t& out) {
    const char* p = buf;
    bool negative = false;
    if (*p == '-') {
        negative = true;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    if (*p < '0' || *p > '9') {
        return false;
    }
    constexpr auto maxPos = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const std::uint64_t limit = negative ? maxPos + 1ULL : maxPos;
    std::uint64_t value = 0;
    while (*p >= '0' && *p <= '9') {
        const auto digit = static_cast<std::uint64_t>(*p - '0');
        if (value > (limit - digit) / 10ULL) {
            return false;
        }
        value = value * 10ULL + digit;
        ++p;
    }
    buf = p;
    if (negative) {
        out = (value == limit ? std::numeric_limits<std::int64_t>::min() : -static_cast<std::int64_t>(value));
    } else {
        out = static_cast<std::int64_t>(value);
    }
    return true;
}

bool IsLeapYear(std::int64_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int CalculateDayOfYear(std::int64_t year, int mon, int mday) {
    constexpr int daysBeforeMonth[2][12] = {
        {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
    };
    const bool leap = IsLeapYear(year);
    return daysBeforeMonth[leap ? 1 : 0][mon] + (mday - 1);
}

int CalculateDayOfWeek(std::int64_t y, int m, int d) {
    constexpr int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    y = (y % 400 + 400) % 400;
    return static_cast<int>((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
}

std::int64_t DaysFromJan1(int year) {
    const std::int64_t y = static_cast<std::int64_t>(year) - 1;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const std::int64_t yoe = y - era * 400;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + 306;
    return era * 146097 + doe - 719468;
}

bool ApplyDaysOffset(int baseYear, int daysOffset, int wday, GuestTm& tm) {
    const std::int64_t days = DaysFromJan1(baseYear) + daysOffset;
    const std::int64_t shifted = days + 719468;
    const std::int64_t era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
    const std::int64_t doe = shifted - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    const int mday = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    const std::int64_t civilYear = yoe + era * 400 + (month <= 2 ? 1 : 0);
    const std::int64_t tmYear = civilYear - 1900;
    if (tmYear < std::numeric_limits<int>::min() || tmYear > std::numeric_limits<int>::max()) {
        return false;
    }
    tm.tm_year = static_cast<int>(tmYear);
    tm.tm_mon = month - 1;
    tm.tm_mday = mday;
    tm.tm_yday = CalculateDayOfYear(civilYear, tm.tm_mon, mday);
    tm.tm_wday = wday;
    return true;
}

int ResolveTwoDigitYear(int twoDigit, int century) {
    if (century >= 0) {
        return century * 100 + twoDigit;
    }
    return (twoDigit >= 69 ? 1900 : 2000) + twoDigit;
}

struct ParseState {
    int century = -1;
    int yearTwoDigit = -1;
    int yearFull = -1;
    int isoYearTwoDigit = -1;
    int isoYearFull = -1;
    int isoWeek = -1;
    int weekBase = -1;
    int weekNumber = -1;
    int hour12 = -1;
    int hour24 = -1;
    int pmOffset = -1;
    bool hasYear = false;
    bool hasIsoYear = false;
    bool hasMon = false;
    bool hasMday = false;
    bool hasWday = false;
    bool hasYday = false;
    bool hasEpoch = false;
};

const char* ParseInternal(const char* buf, const char* format, GuestTm& tm, ParseState& state, int depth) {
    if (depth > 8) return nullptr;
    const char* b = buf;
    const char* f = format;

    while (*f != '\0') {
        if (std::isspace(static_cast<unsigned char>(*f))) {
            while (std::isspace(static_cast<unsigned char>(*f))) ++f;
            while (std::isspace(static_cast<unsigned char>(*b))) ++b;
            continue;
        }

        if (*f != '%') {
            if (*b != *f) return nullptr;
            ++b;
            ++f;
            continue;
        }

        ++f;
        if (*f == 'E' || *f == 'O') {
            ++f;
        }

        switch (*f) {
            case '%': {
                if (*b != '%') return nullptr;
                ++b;
                break;
            }
            case 'n':
            case 't': {
                while (std::isspace(static_cast<unsigned char>(*b))) ++b;
                break;
            }
            case 'a':
            case 'A': {
                bool matched = false;
                for (const auto& w : Weekdays) {
                    const char* trial = b;
                    if (MatchesCaseInsensitive(trial, w.full) || MatchesCaseInsensitive(trial, w.abbrev)) {
                        b = trial;
                        tm.tm_wday = w.index;
                        state.hasWday = true;
                        matched = true;
                        break;
                    }
                }
                if (!matched) return nullptr;
                break;
            }
            case 'b':
            case 'B':
            case 'h': {
                bool matched = false;
                for (const auto& m : Months) {
                    const char* trial = b;
                    if (MatchesCaseInsensitive(trial, m.full) || MatchesCaseInsensitive(trial, m.abbrev)) {
                        b = trial;
                        tm.tm_mon = m.index;
                        state.hasMon = true;
                        matched = true;
                        break;
                    }
                }
                if (!matched) return nullptr;
                break;
            }
            case 'c': {
                b = ParseInternal(b, "%a %b %e %H:%M:%S %Y", tm, state, depth + 1);
                if (b == nullptr) return nullptr;
                break;
            }
            case 'C': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 0, 99, val)) return nullptr;
                state.century = val;
                break;
            }
            case 'd':
            case 'e': {
                while (*b == ' ') ++b;
                int val = 0;
                if (!ParseDigits(b, 1, 2, 1, 31, val)) return nullptr;
                tm.tm_mday = val;
                state.hasMday = true;
                break;
            }
            case 'D': {
                b = ParseInternal(b, "%m/%d/%y", tm, state, depth + 1);
                if (b == nullptr) return nullptr;
                break;
            }
            case 'F': {
                b = ParseInternal(b, "%Y-%m-%d", tm, state, depth + 1);
                if (b == nullptr) return nullptr;
                break;
            }
            case 'H': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 0, 23, val)) return nullptr;
                state.hour24 = val;
                break;
            }
            case 'I': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 1, 12, val)) return nullptr;
                state.hour12 = val;
                break;
            }
            case 'j': {
                int val = 0;
                if (!ParseDigits(b, 1, 3, 1, 366, val)) return nullptr;
                tm.tm_yday = val - 1;
                state.hasYday = true;
                break;
            }
            case 'm': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 1, 12, val)) return nullptr;
                tm.tm_mon = val - 1;
                state.hasMon = true;
                break;
            }
            case 'M': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 0, 59, val)) return nullptr;
                tm.tm_min = val;
                break;
            }
            case 'p': {
                const char* trial = b;
                if (MatchesCaseInsensitive(trial, "am")) {
                    b = trial;
                    state.pmOffset = 0;
                } else if (MatchesCaseInsensitive(trial, "pm")) {
                    b = trial;
                    state.pmOffset = 12;
                } else {
                    return nullptr;
                }
                break;
            }
            case 'r': {
                b = ParseInternal(b, "%I:%M:%S %p", tm, state, depth + 1);
                if (b == nullptr) return nullptr;
                break;
            }
            case 'R': {
                b = ParseInternal(b, "%H:%M", tm, state, depth + 1);
                if (b == nullptr) return nullptr;
                break;
            }
            case 'S': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 0, 60, val)) return nullptr;
                tm.tm_sec = val;
                break;
            }
            case 'T': {
                b = ParseInternal(b, "%H:%M:%S", tm, state, depth + 1);
                if (b == nullptr) return nullptr;
                break;
            }
            case 'u': {
                int val = 0;
                if (!ParseDigits(b, 1, 1, 1, 7, val)) return nullptr;
                tm.tm_wday = (val == 7 ? 0 : val);
                state.hasWday = true;
                break;
            }
            case 'w': {
                int val = 0;
                if (!ParseDigits(b, 1, 1, 0, 6, val)) return nullptr;
                tm.tm_wday = val;
                state.hasWday = true;
                break;
            }
            case 'x': {
                b = ParseInternal(b, "%m/%d/%y", tm, state, depth + 1);
                if (b == nullptr) return nullptr;
                break;
            }
            case 'X': {
                b = ParseInternal(b, "%H:%M:%S", tm, state, depth + 1);
                if (b == nullptr) return nullptr;
                break;
            }
            case 'y': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 0, 99, val)) return nullptr;
                state.yearTwoDigit = val;
                state.hasYear = false;
                break;
            }
            case 'Y': {
                int val = 0;
                if (!ParseDigits(b, 1, 4, 0, 9999, val)) return nullptr;
                state.yearFull = val;
                state.yearTwoDigit = -1;
                state.hasYear = true;
                break;
            }
            case 'z': {
                if (*b == 'Z' || *b == 'z') {
                    ++b;
                    tm.tm_gmtoff = 0;
                } else if (*b == '+' || *b == '-') {
                    const char sign = *b++;
                    int hours = 0;
                    if (!ParseDigits(b, 2, 2, 0, 23, hours)) return nullptr;
                    int mins = 0;
                    if (*b == ':') ++b;
                    if (*b >= '0' && *b <= '9') {
                        if (!ParseDigits(b, 2, 2, 0, 59, mins)) return nullptr;
                    }
                    const int totalSec = (hours * 3600 + mins * 60) * (sign == '-' ? -1 : 1);
                    tm.tm_gmtoff = totalSec;
                } else {
                    return nullptr;
                }
                break;
            }
            case 'Z': {
                const char* start = b;
                while (std::isalpha(static_cast<unsigned char>(*b))) ++b;
                if (b == start) return nullptr;
                const std::string_view name(start, static_cast<std::size_t>(b - start));
                if (name == "UTC" || name == "GMT") {
                    tm.tm_gmtoff = 0;
                }
                break;
            }
            case 'U':
            case 'W': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 0, 53, val)) return nullptr;
                state.weekBase = (*f == 'U' ? 0 : 1);
                state.weekNumber = val;
                break;
            }
            case 's': {
                std::int64_t epoch = 0;
                if (!ParseSignedInt64(b, epoch)) return nullptr;
                GuestTm epochTm{};
                if (localtime_r_nid_postfix(&epoch, &epochTm) == nullptr) return nullptr;
                if (epochTm.tm_year > std::numeric_limits<int>::max() - 1900 ||
                    epochTm.tm_year < std::numeric_limits<int>::min() + 1900) {
                    return nullptr;
                }
                tm = epochTm;
                state = {};
                state.hasEpoch = true;
                break;
            }
            case 'G': {
                int val = 0;
                if (!ParseDigits(b, 1, 4, 0, 9999, val)) return nullptr;
                state.isoYearFull = val;
                state.hasIsoYear = true;
                break;
            }
            case 'g': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 0, 99, val)) return nullptr;
                state.isoYearTwoDigit = val;
                break;
            }
            case 'V': {
                int val = 0;
                if (!ParseDigits(b, 1, 2, 1, 53, val)) return nullptr;
                state.isoWeek = val;
                break;
            }
            case 'k': {
                while (*b == ' ') ++b;
                int val = 0;
                if (!ParseDigits(b, 1, 2, 0, 23, val)) return nullptr;
                state.hour24 = val;
                break;
            }
            case 'l': {
                while (*b == ' ') ++b;
                int val = 0;
                if (!ParseDigits(b, 1, 2, 1, 12, val)) return nullptr;
                state.hour12 = val;
                break;
            }
            default: {
                return nullptr;
            }
        }
        ++f;
    }

    if (depth == 0) {
        if (state.hour24 >= 0) {
            tm.tm_hour = state.hour24;
        } else if (state.hour12 >= 0) {
            if (state.pmOffset == 0) {
                tm.tm_hour = (state.hour12 == 12 ? 0 : state.hour12);
            } else if (state.pmOffset == 12) {
                tm.tm_hour = (state.hour12 == 12 ? 12 : state.hour12 + 12);
            } else {
                tm.tm_hour = state.hour12;
            }
        }

        if (state.hasYear) {
            tm.tm_year = state.yearFull - 1900;
        } else if (state.yearTwoDigit >= 0) {
            tm.tm_year = ResolveTwoDigitYear(state.yearTwoDigit, state.century) - 1900;
            state.hasYear = true;
        } else if (state.century >= 0 && !state.hasIsoYear && state.isoYearTwoDigit < 0) {
            const int existingYearInCentury = (tm.tm_year >= 0 ? tm.tm_year % 100 : (100 - ((-tm.tm_year) % 100)) % 100);
            tm.tm_year = (state.century * 100 + existingYearInCentury) - 1900;
            state.hasYear = true;
        }

        const bool hasEffectiveYear = state.hasYear || state.hasEpoch;
        const bool hasUpdatedCalendar =
            state.hasEpoch && (state.hasMon || state.hasMday || (state.hasYear && state.weekBase < 0 && state.isoWeek <= 0));
        if ((state.hasYear && state.hasMon && state.hasMday) || hasUpdatedCalendar) {
            const std::int64_t fullYear = static_cast<std::int64_t>(tm.tm_year) + 1900;
            if (!state.hasYday) {
                tm.tm_yday = CalculateDayOfYear(fullYear, tm.tm_mon, tm.tm_mday);
            }
            if (!state.hasWday) {
                tm.tm_wday = CalculateDayOfWeek(fullYear, tm.tm_mon + 1, tm.tm_mday);
            }
        } else if (hasEffectiveYear && !state.hasYday && !(state.hasMon && state.hasMday) && state.weekBase >= 0) {
            const int fullYear = tm.tm_year + 1900;
            const int fwo = CalculateDayOfWeek(fullYear, 1, 1);
            if (state.weekNumber == 0 && fwo == state.weekBase) {
                return nullptr;
            }
            int tmpwday = state.hasWday ? tm.tm_wday : state.weekBase;
            int daysOffset = (7 - fwo + state.weekBase) % 7 + (state.weekNumber - 1) * 7 + (tmpwday - state.weekBase + 7) % 7;
            if (daysOffset < 0) {
                if (state.hasWday) {
                    return nullptr;
                }
                daysOffset = 0;
                tmpwday = fwo;
            }
            if (!ApplyDaysOffset(fullYear, daysOffset, tmpwday, tm)) {
                return nullptr;
            }
        } else if (state.isoWeek > 0 && !state.hasYday && !(state.hasMon && state.hasMday) &&
                   (state.hasIsoYear || state.isoYearTwoDigit >= 0 || hasEffectiveYear)) {
            const int isoYear = state.hasIsoYear
                ? state.isoYearFull
                : (state.isoYearTwoDigit >= 0 ? ResolveTwoDigitYear(state.isoYearTwoDigit, state.century) : tm.tm_year + 1900);
            const int jan4IsoWday = (CalculateDayOfWeek(isoYear, 1, 4) + 6) % 7;
            const int isoWday = state.hasWday ? (tm.tm_wday + 6) % 7 : 0;
            const int daysOffset = 3 - jan4IsoWday + (state.isoWeek - 1) * 7 + isoWday;
            if (!ApplyDaysOffset(isoYear, daysOffset, (isoWday + 1) % 7, tm)) {
                return nullptr;
            }
        }
    }

    return b;
}

} // namespace

extern "C" char* APS5_VABI strptime_nid_postfix(const char* buf, const char* format, GuestTm* timeptr) {
    if (buf == nullptr || format == nullptr || timeptr == nullptr) {
        return nullptr;
    }
    std::string input;
    if (CopyGuestString_nid_no_patch(buf, MaxInputLength, input) != 0) {
        return nullptr;
    }
    std::string fmt;
    if (CopyGuestString_nid_no_patch(format, MaxFormatLength, fmt) != 0) {
        return nullptr;
    }
    GuestTm tm{};
    if (!GuestCopyIn_nid_no_patch(&tm, timeptr, sizeof(tm))) {
        return nullptr;
    }

    ParseState state{};
    const char* const end = ParseInternal(input.c_str(), fmt.c_str(), tm, state, 0);
    if (end == nullptr) {
        return nullptr;
    }

    if (!GuestCopyOut_nid_no_patch(timeptr, &tm, sizeof(tm))) {
        return nullptr;
    }
    const std::size_t consumed = static_cast<std::size_t>(end - input.c_str());
    return const_cast<char*>(buf + consumed);
}
