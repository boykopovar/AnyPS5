#include "prx/libSceSystemService/include/SystemService.hpp"
#include "SceTypes.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#ifdef __linux__
#include <unistd.h>
#endif

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stdout, "check failed: %s\n", what);
    std::abort();
}

void Set(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value != nullptr ? value : "") == 0, "environment change");
#else
    Require((value != nullptr ? ::setenv(name, value, 1) : ::unsetenv(name)) == 0, "environment change");
#endif
}

#ifdef __linux__
const char* Argument(const char* text) { return std::strcmp(text, "-") == 0 ? nullptr : text; }
#endif

constexpr int Throws = -100;

int Query(int paramId, const char* variable) {
    int value = -12345;
    try {
        Require(sceSystemServiceParamGetInt(paramId, &value) == SYSTEM_SERVICE_OK, "parameter answers");
    } catch (const std::runtime_error& error) {
        const std::string text = error.what();
        Require(value == -12345, "a refused value leaves the output untouched");
        Require(text.find(variable) != std::string::npos, "the error names the variable");
        Require(text.find("or auto, not") != std::string::npos, "the error says auto is accepted");
        return Throws;
    }
    return value;
}

int DateFormat() { return Query(SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT, "ANYPS5_DATE_FORMAT"); }
int TimeFormat() { return Query(SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT, "ANYPS5_TIME_FORMAT"); }

void SetBoth(const char* date, const char* time) {
    Set("ANYPS5_DATE_FORMAT", date);
    Set("ANYPS5_TIME_FORMAT", time);
}

void RequireExplicitValues() {
    SetBoth(nullptr, nullptr);
    Require(DateFormat() == 1 && TimeFormat() == 1, "unset keeps day-month-year and 24-hour");
    SetBoth("", "");
    Require(DateFormat() == 1 && TimeFormat() == 1, "empty keeps day-month-year and 24-hour");
    for (int date = 0; date <= 2; ++date) {
        const std::string text = std::to_string(date);
        SetBoth(text.c_str(), nullptr);
        Require(DateFormat() == date, "explicit date format");
        Require(TimeFormat() == 1, "the time format ignores the date variable");
    }
    for (int time = 0; time <= 1; ++time) {
        const std::string text = std::to_string(time);
        SetBoth(nullptr, text.c_str());
        Require(TimeFormat() == time, "explicit time format");
        Require(DateFormat() == 1, "the date format ignores the time variable");
    }
    SetBoth("2", "0");
    Require(DateFormat() == 2 && TimeFormat() == 0, "both explicit");
    SetBoth("0", "1");
    Require(DateFormat() == 0 && TimeFormat() == 1, "both explicit again, read at each query");
}

void RequireRefusedValues() {
    for (const char* text : {"3", "-1", "+1", " 1", "1 ", "1x", "0x1", "auto ", "autoo", "-auto", "yes"}) {
        SetBoth(text, nullptr);
        Require(DateFormat() == Throws, "an invalid date format is refused");
        Require(TimeFormat() == 1, "an invalid date format does not affect the time format");
        SetBoth(nullptr, text);
        Require(DateFormat() == 1, "an invalid time format does not affect the date format");
        Require(TimeFormat() == Throws, "an invalid time format is refused");
    }
    SetBoth("2", "2");
    Require(DateFormat() == 2, "2 is a date format");
    Require(TimeFormat() == Throws, "2 is not a time format");
    SetBoth(nullptr, nullptr);
}

void RequireNeighbours() {
    SetBoth("2", "0");
    int zone = -12345;
    Require(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_TIME_ZONE, &zone) == SYSTEM_SERVICE_OK && zone != -12345, "the time zone still answers");
    int summer = -12345;
    Require(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_SUMMERTIME, &summer) == SYSTEM_SERVICE_OK && (summer == 0 || summer == 1), "summer time still answers");
    int language = -12345;
    Require(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_LANG, &language) == SYSTEM_SERVICE_OK, "the language still answers");
    Require(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER, "null date format pointer");
    Require(sceSystemServiceParamGetInt(SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER, "null time format pointer");
    SetBoth(nullptr, nullptr);
}

#ifdef __linux__
int Notices(const std::string& path) {
    std::ifstream stream(path);
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    int count = 0;
    for (std::size_t at = text.find("=auto: "); at != std::string::npos; at = text.find("=auto: ", at + 1)) ++count;
    return count;
}

int ParseExpected(const char* text) { return std::strcmp(text, "throws") == 0 ? Throws : std::atoi(text); }

int RunHostCase(char** argument) {
    char path[] = "/tmp/anyps5_date_time_format_XXXXXX";
    const int file = ::mkstemp(path);
    Require(file >= 0, "temporary file");
    std::fflush(stderr);
    Require(::dup2(file, 2) >= 0, "redirect stderr");
    SetBoth(Argument(argument[1]), Argument(argument[2]));
    Set("LC_ALL", Argument(argument[3]));
    Set("LC_TIME", Argument(argument[4]));
    Set("LANG", Argument(argument[5]));
    const int expectedDate = ParseExpected(argument[6]);
    const int expectedTime = ParseExpected(argument[7]);
    const int expectedNotices = std::atoi(argument[8]);
    Require(DateFormat() == expectedDate, "date format of the host case");
    Require(TimeFormat() == expectedTime, "time format of the host case");
    Require(DateFormat() == expectedDate && TimeFormat() == expectedTime, "second query agrees");
    Set("LC_ALL", nullptr);
    Set("LC_TIME", nullptr);
    Set("LANG", "en_US.UTF-8");
    Require(DateFormat() == expectedDate && TimeFormat() == expectedTime, "a later change of the host does not change the answer");
    std::fflush(stderr);
    const int notices = Notices(path);
    ::unlink(path);
    ::close(file);
    if (notices != expectedNotices) {
        std::fprintf(stdout, "expected %d notices, found %d\n", expectedNotices, notices);
        return 1;
    }
    return 0;
}

#endif

}

int main(int argc, char** argv) {
    if (argc == 1) {
        RequireExplicitValues();
        RequireRefusedValues();
        RequireNeighbours();
        return 0;
    }
#ifdef __linux__
    Require(argc == 9, "arguments: date time LC_ALL LC_TIME LANG expectedDate expectedTime notices");
    return RunHostCase(argv);
#else
    static_cast<void>(argv);
    return 2;
#endif
}
