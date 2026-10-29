#include "prx/libSceSystemService/DateTimeLocale.hpp"
#include <cstdio>
#include <optional>
#include <string>

namespace {

int failures = 0;

void Require(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "check failed: %s\n", what.c_str());
    ++failures;
}

std::string Show(const std::optional<int>& value) { return value ? std::to_string(*value) : "none"; }

void RequireDate(const char* pattern, std::optional<int> expected) {
    const auto actual = SystemServiceDateTime::DateFormatOfStrftime(pattern);
    Require(actual == expected, std::string("strftime date \"") + pattern + "\" gave " + Show(actual) + ", expected " + Show(expected));
}

void RequireStrftimeDates() {
    using namespace SystemServiceDateTime;
    RequireDate("%m/%d/%Y", DATE_FORMAT_MMDDYYYY);
    RequireDate("%d/%m/%y", DATE_FORMAT_DDMMYYYY);
    RequireDate("%d.%m.%Y", DATE_FORMAT_DDMMYYYY);
    RequireDate("%d/%m/%Y", DATE_FORMAT_DDMMYYYY);
    RequireDate("%Y-%m-%d", DATE_FORMAT_YYYYMMDD);
    RequireDate("%Y\xE5\xB9\xB4%m\xE6\x9C\x88%d\xE6\x97\xA5", DATE_FORMAT_YYYYMMDD);
    RequireDate("%Y\xEB\x85\x84 %m\xEC\x9B\x94 %d\xEC\x9D\xBC", DATE_FORMAT_YYYYMMDD);
    RequireDate("%D", DATE_FORMAT_MMDDYYYY);
    RequireDate("%F", DATE_FORMAT_YYYYMMDD);
    RequireDate("%e %b %Y", DATE_FORMAT_DDMMYYYY);
    RequireDate("%B %-d, %Y", DATE_FORMAT_MMDDYYYY);
    RequireDate("%Y. %b. %-e.", DATE_FORMAT_YYYYMMDD);
    RequireDate("%_d.%0m.%Ey", DATE_FORMAT_DDMMYYYY);
    RequireDate("%Od/%Om/%Y", DATE_FORMAT_DDMMYYYY);
    RequireDate("%10d/%02m/%Y", DATE_FORMAT_DDMMYYYY);
    RequireDate("%A %d %B %Y", DATE_FORMAT_DDMMYYYY);
    RequireDate("%a, %m/%d/%Y (%j)", DATE_FORMAT_MMDDYYYY);
    RequireDate("%d%%%m%%%Y", DATE_FORMAT_DDMMYYYY);
}

void RequireStrftimeDatesWithoutConsoleFormat() {
    RequireDate("%Y/%d/%m", std::nullopt);
    RequireDate("%d/%Y/%m", std::nullopt);
    RequireDate("%m/%Y/%d", std::nullopt);
    RequireDate("%d/%m", std::nullopt);
    RequireDate("%m/%Y", std::nullopt);
    RequireDate("%Y", std::nullopt);
    RequireDate("%d/%d/%Y", std::nullopt);
    RequireDate("%d/%m/%Y %Y", std::nullopt);
    RequireDate("%D %d", std::nullopt);
    RequireDate("%x", std::nullopt);
    RequireDate("%c", std::nullopt);
    RequireDate("%X %d/%m/%Y", std::nullopt);
    RequireDate("%d/%m/%", std::nullopt);
    RequireDate("%d/%m/%-", std::nullopt);
    RequireDate("", std::nullopt);
    RequireDate("dd/mm/yyyy", std::nullopt);
    RequireDate("%H:%M:%S", std::nullopt);
}

void RequireTime(const char* pattern, std::optional<int> expected) {
    const auto actual = SystemServiceDateTime::TimeFormatOfStrftime(pattern);
    Require(actual == expected, std::string("strftime time \"") + pattern + "\" gave " + Show(actual) + ", expected " + Show(expected));
}

void RequireStrftimeTimes() {
    using namespace SystemServiceDateTime;
    RequireTime("%r", TIME_FORMAT_12HOUR);
    RequireTime("%I:%M:%S %p", TIME_FORMAT_12HOUR);
    RequireTime("%l:%M:%S %P %Z", TIME_FORMAT_12HOUR);
    RequireTime("%-I:%M %p", TIME_FORMAT_12HOUR);
    RequireTime("%OI:%OM", TIME_FORMAT_12HOUR);
    RequireTime("%T", TIME_FORMAT_24HOUR);
    RequireTime("%R", TIME_FORMAT_24HOUR);
    RequireTime("%H:%M:%S", TIME_FORMAT_24HOUR);
    RequireTime("%k:%M", TIME_FORMAT_24HOUR);
    RequireTime("%H\xE6\x99\x82%M\xE5\x88\x86%S\xE7\xA7\x92", TIME_FORMAT_24HOUR);
    RequireTime("%H\xEC\x8B\x9C %M\xEB\xB6\x84 %S\xEC\xB4\x88", TIME_FORMAT_24HOUR);
    RequireTime("%H\xE6\x97\xB6%M\xE5\x88\x86%S\xE7\xA7\x92", TIME_FORMAT_24HOUR);
    RequireTime("%H%%%M", TIME_FORMAT_24HOUR);
    RequireTime("%H:%M:%S %Z", TIME_FORMAT_24HOUR);
}

void RequireStrftimeTimesWithoutConsoleFormat() {
    RequireTime("%H:%M %p", std::nullopt);
    RequireTime("%I:%M %H", std::nullopt);
    RequireTime("%r %T", std::nullopt);
    RequireTime("%M:%S", std::nullopt);
    RequireTime("%X", std::nullopt);
    RequireTime("%x %T", std::nullopt);
    RequireTime("%c", std::nullopt);
    RequireTime("%H:%M:%", std::nullopt);
    RequireTime("", std::nullopt);
    RequireTime("HH:mm", std::nullopt);
    RequireTime("%d/%m/%Y", std::nullopt);
}

void RequireLdmlDate(const char* pattern, std::optional<int> expected) {
    const auto actual = SystemServiceDateTime::DateFormatOfLdml(pattern);
    Require(actual == expected, std::string("pattern date \"") + pattern + "\" gave " + Show(actual) + ", expected " + Show(expected));
}

void RequireLdmlDates() {
    using namespace SystemServiceDateTime;
    RequireLdmlDate("M/d/yyyy", DATE_FORMAT_MMDDYYYY);
    RequireLdmlDate("M/d/yy", DATE_FORMAT_MMDDYYYY);
    RequireLdmlDate("MM/dd/y", DATE_FORMAT_MMDDYYYY);
    RequireLdmlDate("dd/MM/yyyy", DATE_FORMAT_DDMMYYYY);
    RequireLdmlDate("d/M/yyyy", DATE_FORMAT_DDMMYYYY);
    RequireLdmlDate("dd.MM.y", DATE_FORMAT_DDMMYYYY);
    RequireLdmlDate("yyyy-MM-dd", DATE_FORMAT_YYYYMMDD);
    RequireLdmlDate("yyyy/MM/dd", DATE_FORMAT_YYYYMMDD);
    RequireLdmlDate("y. MM. dd.", DATE_FORMAT_YYYYMMDD);
    RequireLdmlDate("yyyy. MM. dd.", DATE_FORMAT_YYYYMMDD);
    RequireLdmlDate("yyyy'\xE5\xB9\xB4'M'\xE6\x9C\x88'd'\xE6\x97\xA5'", DATE_FORMAT_YYYYMMDD);
    RequireLdmlDate("d MMM y", DATE_FORMAT_DDMMYYYY);
    RequireLdmlDate("MMM d, y", DATE_FORMAT_MMDDYYYY);
    RequireLdmlDate("LLLL d y", DATE_FORMAT_MMDDYYYY);
    RequireLdmlDate("dddd, d MMMM yyyy", DATE_FORMAT_DDMMYYYY);
    RequireLdmlDate("ddd dd/MM/yyyy", DATE_FORMAT_DDMMYYYY);
    RequireLdmlDate("EEE, d/M/y", DATE_FORMAT_DDMMYYYY);
    RequireLdmlDate("'de' dd 'de' MM 'de' yyyy", DATE_FORMAT_DDMMYYYY);
    RequireLdmlDate("dd/MM/yyyy g", DATE_FORMAT_DDMMYYYY);
    RequireLdmlDate("dd''MM''yyyy", DATE_FORMAT_DDMMYYYY);
}

void RequireLdmlDatesWithoutConsoleFormat() {
    RequireLdmlDate("yyyy/dd/MM", std::nullopt);
    RequireLdmlDate("d/yyyy/M", std::nullopt);
    RequireLdmlDate("M/yyyy/d", std::nullopt);
    RequireLdmlDate("M/d", std::nullopt);
    RequireLdmlDate("MMMM yyyy", std::nullopt);
    RequireLdmlDate("dd/dd/yyyy", std::nullopt);
    RequireLdmlDate("dd/MM/yyyy yyyy", std::nullopt);
    RequireLdmlDate("dddd", std::nullopt);
    RequireLdmlDate("YYYY-MM-dd", std::nullopt);
    RequireLdmlDate("'dd/MM/yyyy'", std::nullopt);
    RequireLdmlDate("dd/MM/'yyyy", std::nullopt);
    RequireLdmlDate("", std::nullopt);
    RequireLdmlDate("%d/%m/%Y", std::nullopt);
}

}

int main() {
    RequireStrftimeDates();
    RequireStrftimeDatesWithoutConsoleFormat();
    RequireStrftimeTimes();
    RequireStrftimeTimesWithoutConsoleFormat();
    RequireLdmlDates();
    RequireLdmlDatesWithoutConsoleFormat();
    if (failures != 0) return 1;
    std::puts("system service date and time locale: all checks passed");
    return 0;
}
