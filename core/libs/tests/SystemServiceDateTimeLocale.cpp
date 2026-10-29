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

}

int main() {
    RequireStrftimeDates();
    RequireStrftimeDatesWithoutConsoleFormat();
    if (failures != 0) return 1;
    std::puts("system service date and time locale: all checks passed");
    return 0;
}
