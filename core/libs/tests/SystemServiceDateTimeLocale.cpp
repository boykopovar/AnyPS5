#include "prx/libSceSystemService/DateTimeLocale.hpp"
#include <cstdio>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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
    RequireDate("%x %d/%m/%Y", std::nullopt);
    RequireDate("%d/%m/%Y %c", std::nullopt);
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

void RequireLdmlTime(const char* pattern, std::optional<int> expected) {
    const auto actual = SystemServiceDateTime::TimeFormatOfLdml(pattern);
    Require(actual == expected, std::string("pattern time \"") + pattern + "\" gave " + Show(actual) + ", expected " + Show(expected));
}

void RequireLdmlTimes() {
    using namespace SystemServiceDateTime;
    RequireLdmlTime("h:mm:ss tt", TIME_FORMAT_12HOUR);
    RequireLdmlTime("hh:mm:ss tt", TIME_FORMAT_12HOUR);
    RequireLdmlTime("h:mm a", TIME_FORMAT_12HOUR);
    RequireLdmlTime("K:mm a", TIME_FORMAT_12HOUR);
    RequireLdmlTime("tt h:mm:ss", TIME_FORMAT_12HOUR);
    RequireLdmlTime("mm:ss tt", TIME_FORMAT_12HOUR);
    RequireLdmlTime("mm:ss a", TIME_FORMAT_12HOUR);
    RequireLdmlTime("K", TIME_FORMAT_12HOUR);
    RequireLdmlTime("h", TIME_FORMAT_12HOUR);
    RequireLdmlTime("HH:mm:ss", TIME_FORMAT_24HOUR);
    RequireLdmlTime("H:mm", TIME_FORMAT_24HOUR);
    RequireLdmlTime("k:mm", TIME_FORMAT_24HOUR);
    RequireLdmlTime("H.mm.ss", TIME_FORMAT_24HOUR);
    RequireLdmlTime("HH'\xE6\x99\x82'mm'\xE5\x88\x86'ss'\xE7\xA7\x92'", TIME_FORMAT_24HOUR);
    RequireLdmlTime("HH:mm:ss 'at' zzzz", TIME_FORMAT_24HOUR);
    RequireLdmlTime("HH:mm 'h' 'a'", TIME_FORMAT_24HOUR);
    RequireLdmlTime("HH''mm", TIME_FORMAT_24HOUR);
}

void RequireLdmlTimesWithoutConsoleFormat() {
    RequireLdmlTime("H:mm a", std::nullopt);
    RequireLdmlTime("h:mm H", std::nullopt);
    RequireLdmlTime("mm:ss", std::nullopt);
    RequireLdmlTime("'h' 'H'", std::nullopt);
    RequireLdmlTime("'o''clock' mm", std::nullopt);
    RequireLdmlTime("", std::nullopt);
    RequireLdmlTime("dd/MM/yyyy", std::nullopt);
}

SystemServiceDateTime::HostPatterns Host(SystemServiceDateTime::PatternSyntax syntax, const char* date, const char* time) {
    SystemServiceDateTime::HostPatterns host;
    host.available = true;
    host.syntax = syntax;
    host.date = date;
    host.time = time;
    host.source = "test host";
    return host;
}

void RequireResolution(const SystemServiceDateTime::HostPatterns& host, int date, int time, std::size_t dateNotices, std::size_t timeNotices, const std::string& what) {
    std::vector<std::string> notices;
    const auto notice = [&notices](const std::string& line) { notices.push_back(line); };
    Require(SystemServiceDateTime::ResolveHostDateFormat(host, notice) == date, what + ": date format");
    Require(notices.size() == dateNotices, what + ": date notices, got " + std::to_string(notices.size()));
    notices.clear();
    Require(SystemServiceDateTime::ResolveHostTimeFormat(host, notice) == time, what + ": time format");
    Require(notices.size() == timeNotices, what + ": time notices, got " + std::to_string(notices.size()));
}

void RequireResolver() {
    using namespace SystemServiceDateTime;
    RequireResolution(Host(PatternSyntax::Strftime, "%m/%d/%Y", "%r"), DATE_FORMAT_MMDDYYYY, TIME_FORMAT_12HOUR, 0, 0, "strftime US");
    RequireResolution(Host(PatternSyntax::Strftime, "%d.%m.%Y", "%T"), DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, 0, 0, "strftime DE");
    RequireResolution(Host(PatternSyntax::Strftime, "%Y-%m-%d", "%H:%M:%S"), DATE_FORMAT_YYYYMMDD, TIME_FORMAT_24HOUR, 0, 0, "strftime SE");
    RequireResolution(Host(PatternSyntax::Ldml, "M/d/yyyy", "h:mm:ss tt"), DATE_FORMAT_MMDDYYYY, TIME_FORMAT_12HOUR, 0, 0, "ldml US");
    RequireResolution(Host(PatternSyntax::Ldml, "dd/MM/yyyy", "HH:mm:ss"), DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, 0, 0, "ldml GB");
    RequireResolution(Host(PatternSyntax::Ldml, "yyyy/MM/dd", "H:mm:ss"), DATE_FORMAT_YYYYMMDD, TIME_FORMAT_24HOUR, 0, 0, "ldml JP");
    RequireResolution(Host(PatternSyntax::Strftime, "%Y/%d/%m", "%T"), DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, 1, 0, "date without console format");
    RequireResolution(Host(PatternSyntax::Strftime, "%m/%d/%Y", "%H %p"), DATE_FORMAT_MMDDYYYY, TIME_FORMAT_24HOUR, 0, 1, "time without console format");
    RequireResolution(Host(PatternSyntax::Ldml, "%d/%m/%Y", "%H:%M"), DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, 1, 0, "strftime text read as letters");
    HostPatterns unavailable;
    unavailable.source = "LC_ALL selects the C locale";
    RequireResolution(unavailable, DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, 1, 1, "unavailable host");
}

void RequireNoticeText() {
    using namespace SystemServiceDateTime;
    std::vector<std::string> notices;
    const auto notice = [&notices](const std::string& line) { notices.push_back(line); };
    HostPatterns unavailable;
    unavailable.source = "LC_ALL selects the C locale";
    ResolveHostDateFormat(unavailable, notice);
    ResolveHostTimeFormat(unavailable, notice);
    ResolveHostDateFormat(Host(PatternSyntax::Strftime, "%Y/%d/%m", "%p"), notice);
    ResolveHostTimeFormat(Host(PatternSyntax::Strftime, "%Y/%d/%m", "%p %T"), notice);
    Require(notices.size() == 4, "four notices");
    if (notices.size() != 4) return;
    Require(notices[0] == "ANYPS5_DATE_FORMAT=auto: LC_ALL selects the C locale; using day, month, year (1)", "unavailable date notice: " + notices[0]);
    Require(notices[1] == "ANYPS5_TIME_FORMAT=auto: LC_ALL selects the C locale; using 24-hour (1)", "unavailable time notice: " + notices[1]);
    Require(notices[2] == "ANYPS5_DATE_FORMAT=auto: the date pattern \"%Y/%d/%m\" of test host has no console date format; using day, month, year (1)", "unmapped date notice: " + notices[2]);
    Require(notices[3] == "ANYPS5_TIME_FORMAT=auto: the time pattern \"%p %T\" of test host has no console time format; using 24-hour (1)", "unmapped time notice: " + notices[3]);
}

struct FakeLocale {
    const char* name;
    const char* date;
    const char* time;
};

constexpr FakeLocale FakeLocales[] = {
    {"en_US.UTF-8", "%m/%d/%Y", "%r"},
    {"en_GB.UTF-8", "%d/%m/%y", "%T"},
    {"de_DE.UTF-8", "%d.%m.%Y", "%T"},
    {"fr_FR.UTF-8", "%d/%m/%Y", "%T"},
    {"hu_HU.UTF-8", "%Y-%m-%d", "%H:%M:%S"},
    {"sv_SE.UTF-8", "%Y-%m-%d", "%H:%M:%S"},
    {"ja_JP.UTF-8", "%Y\xE5\xB9\xB4%m\xE6\x9C\x88%d\xE6\x97\xA5", "%H\xE6\x99\x82%M\xE5\x88\x86%S\xE7\xA7\x92"},
    {"ko_KR.UTF-8", "%Y\xEB\x85\x84 %m\xEC\x9B\x94 %d\xEC\x9D\xBC", "%H\xEC\x8B\x9C %M\xEB\xB6\x84 %S\xEC\xB4\x88"},
    {"en_CA.UTF-8", "%Y-%m-%d", "%r"},
};

SystemServiceDateTime::HostPatterns PosixHost(const std::map<std::string, std::string>& variables) {
    return SystemServiceDateTime::PosixHostPatterns(
        [&variables](const char* name) -> const char* {
            const auto found = variables.find(name);
            return found == variables.end() ? nullptr : found->second.c_str();
        },
        [](const std::string& name) {
            SystemServiceDateTime::LangInfo info;
            for (const auto& fake : FakeLocales) {
                if (name != fake.name) continue;
                info.available = true;
                info.dateFormat = fake.date;
                info.timeFormat = fake.time;
            }
            return info;
        });
}

void RequirePosixPick(const std::map<std::string, std::string>& variables, int date, int time, const std::string& what) {
    const auto host = PosixHost(variables);
    Require(host.available, what + ": available (" + host.source + ")");
    std::vector<std::string> notices;
    const auto notice = [&notices](const std::string& line) { notices.push_back(line); };
    Require(SystemServiceDateTime::ResolveHostDateFormat(host, notice) == date, what + ": date format");
    Require(SystemServiceDateTime::ResolveHostTimeFormat(host, notice) == time, what + ": time format");
    Require(notices.empty(), what + ": no notice");
}

void RequirePosixUnavailable(const std::map<std::string, std::string>& variables, const std::string& source, const std::string& what) {
    const auto host = PosixHost(variables);
    Require(!host.available, what + ": unavailable");
    Require(host.source == source, what + ": source \"" + host.source + "\", expected \"" + source + "\"");
}

void RequirePosixSource() {
    using namespace SystemServiceDateTime;
    RequirePosixPick({{"LANG", "en_US.UTF-8"}}, DATE_FORMAT_MMDDYYYY, TIME_FORMAT_12HOUR, "en_US");
    RequirePosixPick({{"LANG", "en_GB.UTF-8"}}, DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, "en_GB");
    RequirePosixPick({{"LANG", "de_DE.UTF-8"}}, DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, "de_DE");
    RequirePosixPick({{"LANG", "fr_FR.UTF-8"}}, DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, "fr_FR");
    RequirePosixPick({{"LANG", "hu_HU.UTF-8"}}, DATE_FORMAT_YYYYMMDD, TIME_FORMAT_24HOUR, "hu_HU");
    RequirePosixPick({{"LANG", "sv_SE.UTF-8"}}, DATE_FORMAT_YYYYMMDD, TIME_FORMAT_24HOUR, "sv_SE");
    RequirePosixPick({{"LANG", "ja_JP.UTF-8"}}, DATE_FORMAT_YYYYMMDD, TIME_FORMAT_24HOUR, "ja_JP");
    RequirePosixPick({{"LANG", "ko_KR.UTF-8"}}, DATE_FORMAT_YYYYMMDD, TIME_FORMAT_24HOUR, "ko_KR");
    RequirePosixPick({{"LANG", "en_CA.UTF-8"}}, DATE_FORMAT_YYYYMMDD, TIME_FORMAT_12HOUR, "en_CA mixes year-first dates with a 12-hour clock");
    RequirePosixPick({{"LC_ALL", "de_DE.UTF-8"}, {"LC_TIME", "en_US.UTF-8"}, {"LANG", "ja_JP.UTF-8"}}, DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, "LC_ALL wins");
    RequirePosixPick({{"LC_TIME", "en_US.UTF-8"}, {"LANG", "ja_JP.UTF-8"}}, DATE_FORMAT_MMDDYYYY, TIME_FORMAT_12HOUR, "LC_TIME beats LANG");
    RequirePosixPick({{"LC_ALL", ""}, {"LC_TIME", ""}, {"LANG", "en_US.UTF-8"}}, DATE_FORMAT_MMDDYYYY, TIME_FORMAT_12HOUR, "empty LC_ALL and LC_TIME are skipped");
    RequirePosixPick({{"LC_MESSAGES", "ja_JP.UTF-8"}, {"LANGUAGE", "ja"}, {"LANG", "en_GB.UTF-8"}}, DATE_FORMAT_DDMMYYYY, TIME_FORMAT_24HOUR, "other variables are ignored");
    RequirePosixUnavailable({}, "none of LC_ALL, LC_TIME and LANG is set", "nothing set");
    RequirePosixUnavailable({{"LANG", ""}}, "none of LC_ALL, LC_TIME and LANG is set", "empty LANG");
    RequirePosixUnavailable({{"LANG", "C"}}, "LANG is \"C\", the C locale", "C");
    RequirePosixUnavailable({{"LANG", "POSIX"}}, "LANG is \"POSIX\", the C locale", "POSIX");
    RequirePosixUnavailable({{"LANG", "C.UTF-8"}}, "LANG is \"C.UTF-8\", the C locale", "C.UTF-8");
    RequirePosixUnavailable({{"LC_ALL", "C"}, {"LANG", "de_DE.UTF-8"}}, "LC_ALL is \"C\", the C locale", "LC_ALL C beats LANG");
    RequirePosixUnavailable({{"LC_TIME", "POSIX.UTF-8"}, {"LANG", "de_DE.UTF-8"}}, "LC_TIME is \"POSIX.UTF-8\", the C locale", "POSIX codeset");
    RequirePosixUnavailable({{"LANG", "C@x"}}, "LANG is \"C@x\", the C locale", "C with a modifier");
    RequirePosixUnavailable({{"LANG", "xx_YY.UTF-8"}}, "the locale \"xx_YY.UTF-8\" from LANG is not installed", "unknown locale");
    RequirePosixUnavailable({{"LC_TIME", "xx_YY"}, {"LANG", "de_DE.UTF-8"}}, "the locale \"xx_YY\" from LC_TIME is not installed", "unknown LC_TIME is not skipped");
    RequirePosixUnavailable({{"LANG", "Cx"}}, "the locale \"Cx\" from LANG is not installed", "C prefix without a separator");
    RequirePosixUnavailable({{"LANG", "POSIXLY"}}, "the locale \"POSIXLY\" from LANG is not installed", "POSIX prefix without a separator");
}

#if !defined(_WIN32) && !defined(__APPLE__)
void RequireRealLangInfo() {
    const auto c = SystemServiceDateTime::ReadLangInfo("C");
    Require(c.available, "the C locale reads");
    Require(c.dateFormat == "%m/%d/%y", "C D_FMT is " + c.dateFormat);
    Require(c.timeFormat == "%H:%M:%S", "C T_FMT is " + c.timeFormat);
    Require(!SystemServiceDateTime::ReadLangInfo("no_SUCH_locale.nothing").available, "an unknown locale does not read");
}
#endif

void RequireSetting(const char* configured, int expected, const std::string& what) {
    int hostCalls = 0;
    const auto host = [&hostCalls] {
        ++hostCalls;
        return 7;
    };
    const int value = SystemServiceDateTime::ParseSetting(configured, "ANYPS5_TEST", "test value", 2, 1, host, "caller");
    Require(value == expected, what + ": gave " + std::to_string(value));
    Require(hostCalls == (expected == 7 ? 1 : 0), what + ": host calls " + std::to_string(hostCalls));
}

void RequireRefused(const char* configured) {
    bool refused = false;
    try {
        SystemServiceDateTime::ParseSetting(configured, "ANYPS5_TEST", "test value", 2, 1, [] { return 7; }, "caller");
    } catch (const std::runtime_error& error) {
        const std::string expected = std::string("caller: ANYPS5_TEST must be a console test value number from 0 to 2 or auto, not \"") + configured + "\"";
        refused = expected == error.what();
    }
    Require(refused, std::string("refused with the variable's name: \"") + configured + "\"");
}

void RequireSettings() {
    RequireSetting(nullptr, 1, "unset");
    RequireSetting("", 1, "empty");
    RequireSetting("0", 0, "0");
    RequireSetting("1", 1, "1");
    RequireSetting("2", 2, "2");
    RequireSetting("00", 0, "leading zeros");
    RequireSetting("auto", 7, "auto");
    RequireSetting("AUTO", 7, "AUTO");
    RequireSetting("Auto", 7, "Auto");
    RequireSetting("aUtO", 7, "aUtO");
    for (const char* text : {"3", "-1", "+1", " 1", "1 ", "1x", "0x1", "1.0", "99999999999", "auto ", " auto", "autoo", "aut", "-auto", "au to", "yes", "\t", "\xC3\xA4uto", "x"}) RequireRefused(text);
}

}

int main() {
    RequireStrftimeDates();
    RequireStrftimeDatesWithoutConsoleFormat();
    RequireStrftimeTimes();
    RequireStrftimeTimesWithoutConsoleFormat();
    RequireLdmlDates();
    RequireLdmlDatesWithoutConsoleFormat();
    RequireLdmlTimes();
    RequireLdmlTimesWithoutConsoleFormat();
    RequireResolver();
    RequireNoticeText();
    RequirePosixSource();
#if !defined(_WIN32) && !defined(__APPLE__)
    RequireRealLangInfo();
#endif
    RequireSettings();
    if (failures != 0) return 1;
    std::puts("system service date and time locale: all checks passed");
    return 0;
}
