#include "prx/libSceRtc/include/Rtc.hpp"
#include "SceTypes.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <initializer_list>
#include <limits>
#include <random>

static void Require(bool value) { if (!value) std::abort(); }

static void SetTimeZone(const char* zone) {
#ifdef _WIN32
    _putenv_s("TZ", zone);
    _tzset();
#else
    setenv("TZ", zone, 1);
    tzset();
#endif
}

static bool Equal(const RtcDateTime& left, const RtcDateTime& right) {
    return left.year == right.year && left.month == right.month && left.day == right.day && left.hour == right.hour
        && left.minute == right.minute && left.second == right.second && left.microsecond == right.microsecond;
}

enum class Add { Ticks, Microseconds, Seconds, Minutes, Hours, Days, Weeks, Months, Years };

struct AddCase {
    Add function;
    std::uint64_t source;
    std::int64_t amount;
    bool written;
    std::uint64_t result;
};

static int CallAdd(Add function, RtcTick* destination, const RtcTick* source, std::int64_t amount) {
    switch (function) {
    case Add::Ticks: return sceRtcTickAddTicks(destination, source, amount);
    case Add::Microseconds: return sceRtcTickAddMicroseconds(destination, source, amount);
    case Add::Seconds: return sceRtcTickAddSeconds(destination, source, amount);
    case Add::Minutes: return sceRtcTickAddMinutes(destination, source, amount);
    case Add::Hours: return sceRtcTickAddHours(destination, source, static_cast<std::int32_t>(amount));
    case Add::Days: return sceRtcTickAddDays(destination, source, static_cast<std::int32_t>(amount));
    case Add::Weeks: return sceRtcTickAddWeeks(destination, source, static_cast<std::int32_t>(amount));
    case Add::Months: return sceRtcTickAddMonths(destination, source, static_cast<std::int32_t>(amount));
    case Add::Years: return sceRtcTickAddYears(destination, source, static_cast<std::int16_t>(amount));
    }
    std::abort();
}

static void SetCompiledSdkVersion(std::byte* process, std::uint32_t version) {
    std::memcpy(process + 16, &version, sizeof(version));
}

static void EnterTitleDirectory(const char* titleId) {
    const auto directory = std::filesystem::temp_directory_path() / ("rtc-test-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(directory / "app0" / "sce_sys");
    std::filesystem::current_path(directory);
    if (titleId == nullptr) return;
    struct Entry { std::uint16_t key, format; std::uint32_t length, maximum, offset; };
    const std::uint32_t header[5] = {0x46535000, 0x0101, 52, 72, 2};
    const Entry entries[2] = {{0, 0x0204, 8, 8, 0}, {6, 0x0204, 10, 12, 8}};
    std::ofstream file("app0/sce_sys/param.sfo", std::ios::binary);
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    file.write(reinterpret_cast<const char*>(entries), sizeof(entries));
    file << std::string("TITLE\0TITLE_ID\0\0\0\0\0\0", 20) << std::string("Example\0", 8) << titleId << std::string("\0\0\0", 3);
}

int main(int argc, char** argv) {
    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    SetCompiledSdkVersion(process.data(), 0x01000051);
    if (argc == 2 && std::string_view(argv[1]) == "ps5-title") {
        EnterTitleDirectory(nullptr);
        RtcDateTime untouched{0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaaaaaau};
        RtcDateTime converted = untouched;
        Require(sceRtcGetDayOfWeek(10000, 1, 1) == static_cast<int>(0x80B50008) && sceRtcGetDayOfWeek(0, 13, 1) == static_cast<int>(0x80B50009));
        Require(sceRtcSetTime_t(&converted, -1) == static_cast<int>(0x80B50003) && Equal(converted, untouched));
        char text[64];
        RtcTick tick{std::numeric_limits<std::uint64_t>::max()};
        Require(sceRtcFormatRFC3339(text, &tick, 1) == static_cast<int>(0x80B50003));
        return 0;
    }
    EnterTitleDirectory("CUSA00000");
    constexpr int invalidPointer = static_cast<int>(0x80B50002);
    constexpr int invalidValue = static_cast<int>(0x80B50003);
    constexpr int badParse = static_cast<int>(0x80B50007);
    constexpr int invalidYear = static_cast<int>(0x80B50008);
    constexpr int invalidMonth = static_cast<int>(0x80B50009);
    constexpr int invalidDay = static_cast<int>(0x80B5000A);
    constexpr int invalidHour = static_cast<int>(0x80B5000B);
    constexpr int invalidMicrosecond = static_cast<int>(0x80B5000E);
    constexpr std::uint64_t unixEpochTick = 62135596800000000ull;
    constexpr std::uint64_t leapDayTick = 63844806896789000ull;
    constexpr std::uint64_t maxTick = 315537897599999999ull;

    Require(sceRtcGetTickResolution() == 1000000);
    Require(sceRtcIsLeapYear(2000) == 1 && sceRtcIsLeapYear(1900) == 0 && sceRtcIsLeapYear(2024) == 1);
    Require(sceRtcIsLeapYear(0) == invalidYear);
    Require(sceRtcIsLeapYear(10000) == 1);
    Require(sceRtcIsLeapYear(9999) == 0 && sceRtcIsLeapYear(9996) == 1);
    Require(sceRtcGetDaysInMonth(2023, 2) == 28 && sceRtcGetDaysInMonth(2024, 2) == 29 && sceRtcGetDaysInMonth(2024, 4) == 30);
    Require(sceRtcGetDaysInMonth(2024, 13) == invalidMonth);
    Require(sceRtcGetDaysInMonth(10000, 1) == 31);
    Require(sceRtcGetDaysInMonth(9999, 1) == 31);
    Require(sceRtcGetDayOfWeek(1, 1, 1) == 1 && sceRtcGetDayOfWeek(2026, 9, 26) == 6);
    Require(sceRtcGetDayOfWeek(10000, 1, 1) == 6);
    Require(sceRtcGetDayOfWeek(2023, 2, 29) == invalidDay);

    RtcDateTime leapDay{2024, 2, 29, 12, 34, 56, 789000};
    Require(sceRtcCheckValid(&leapDay) == 0);
    Require(sceRtcCheckValid(nullptr) == invalidPointer);
    RtcDateTime invalid = leapDay;
    invalid.year = 2023;
    Require(sceRtcCheckValid(&invalid) == invalidDay);
    invalid = leapDay;
    invalid.hour = 24;
    Require(sceRtcCheckValid(&invalid) == invalidHour);
    invalid = leapDay;
    invalid.microsecond = 1000000;
    Require(sceRtcCheckValid(&invalid) == invalidMicrosecond);

    RtcTick tick{};
    Require(sceRtcGetTick(&leapDay, &tick) == 0 && tick.tick == leapDayTick);
    RtcDateTime converted{};
    Require(sceRtcSetTick(&converted, &tick) == 0 && Equal(converted, leapDay));
    tick.tick = maxTick;
    Require(sceRtcSetTick(&converted, &tick) == 0 && Equal(converted, RtcDateTime{9999, 12, 31, 23, 59, 59, 999999}));
    tick.tick = maxTick + 1;
    Require(sceRtcSetTick(&converted, &tick) == 0 && Equal(converted, RtcDateTime{10000, 1, 1, 0, 0, 0, 0}));

    Require(sceRtcGetCurrentClock(nullptr, 0) == invalidPointer);
    Require(sceRtcGetCurrentClock(&converted, 0) == 0 && sceRtcCheckValid(&converted) == 0);
    Require(sceRtcGetCurrentClock(&converted, 60) == 0 && sceRtcCheckValid(&converted) == 0);
    Require(sceRtcGetCurrentClock(&converted, -300) == 0 && sceRtcCheckValid(&converted) == 0);
    Require(sceRtcGetCurrentClockLocalTime(nullptr) == invalidPointer);
    Require(sceRtcGetCurrentClockLocalTime(&converted) == 0 && sceRtcCheckValid(&converted) == 0);

    RtcDateTime epoch{1970, 1, 1, 0, 0, 0, 0};
    std::int64_t seconds = -1;
    Require(sceRtcGetTime_t(&epoch, &seconds) == 0 && seconds == 0);
    RtcDateTime millennium{2000, 1, 1, 0, 0, 0, 0};
    Require(sceRtcGetTime_t(&millennium, &seconds) == 0 && seconds == 946684800);
    Require(sceRtcSetTime_t(&converted, 946684800) == 0 && Equal(converted, millennium));
    Require(sceRtcSetTime_t(&converted, -1) == 0 && Equal(converted, RtcDateTime{2106, 2, 7, 6, 28, 15, 0}));
    Require(sceRtcSetTime_t(&converted, 0x100000001ll) == 0 && Equal(converted, RtcDateTime{1970, 1, 1, 0, 0, 1, 0}));
    Require(sceRtcSetTime_t(&converted, -0x80000000ll) == 0 && Equal(converted, RtcDateTime{2038, 1, 19, 3, 14, 8, 0}));
    const RtcDateTime past2106{2106, 2, 7, 6, 28, 16, 0};
    Require(sceRtcGetTime_t(&past2106, &seconds) == 0 && seconds == 0x100000000ll);
    const RtcDateTime beforeEpoch{1969, 12, 31, 23, 59, 59, 0};
    seconds = -1;
    Require(sceRtcGetTime_t(&beforeEpoch, &seconds) == invalidYear && seconds == 0);
    const RtcDateTime badMonth{1969, 13, 1, 0, 0, 0, 0};
    seconds = -1;
    Require(sceRtcGetTime_t(&badMonth, &seconds) == invalidMonth && seconds == -1);
    std::uint64_t fileTime = 0;
    Require(sceRtcGetWin32FileTime(&epoch, &fileTime) == 0 && fileTime == 116444736000000000ull);
    Require(sceRtcSetWin32FileTime(&converted, 116444736000000000ull) == 0 && Equal(converted, epoch));
    const RtcDateTime before1601{1600, 12, 31, 23, 59, 59, 0};
    fileTime = 1;
    Require(sceRtcGetWin32FileTime(&before1601, &fileTime) == invalidYear && fileTime == 0);
    Require(sceRtcSetWin32FileTime(&converted, std::numeric_limits<std::uint64_t>::max()) == 0 && Equal(converted, RtcDateTime{60056, 5, 28, 5, 36, 10, 955161}));
    std::uint32_t dosTime = 0xffffffffu;
    const RtcDateTime dosDate{2024, 2, 29, 12, 34, 57, 789000};
    Require(sceRtcGetDosTime(&dosDate, &dosTime) == 0 && dosTime == 0x585d645cu);
    const RtcDateTime dosFirst{1980, 1, 1, 0, 0, 0, 0};
    Require(sceRtcGetDosTime(&dosFirst, &dosTime) == 0 && dosTime == 0x00210000u);
    const RtcDateTime dosLast{2107, 12, 31, 23, 59, 59, 0};
    Require(sceRtcGetDosTime(&dosLast, &dosTime) == 0 && dosTime == 0xff9fbf7du);
    Require(sceRtcGetDosTime(&dosDate, nullptr) == invalidPointer);
    Require(sceRtcGetDosTime(nullptr, &dosTime) == invalidPointer);
    const RtcDateTime dosBadMonth{2024, 13, 1, 0, 0, 0, 0};
    Require(sceRtcGetDosTime(&dosBadMonth, &dosTime) == invalidMonth);
    const RtcDateTime dosEarly{1979, 12, 31, 0, 0, 0, 0};
    Require(sceRtcGetDosTime(&dosEarly, &dosTime) == invalidYear && dosTime == 0);
    const RtcDateTime dosLate{2108, 1, 1, 0, 0, 0, 0};
    Require(sceRtcGetDosTime(&dosLate, &dosTime) == invalidYear && dosTime == 0xff9fbf7du);

    Require(sceRtcGetYear(nullptr) == invalidPointer);
    Require(sceRtcSetYear(nullptr, 2024) == invalidPointer);
    Require(sceRtcSetYear(&converted, 0) == invalidYear);
    Require(sceRtcSetYear(&converted, 10000) == invalidYear);
    Require(sceRtcSetYear(&converted, 1) == 0 && sceRtcGetYear(&converted) == 1);
    Require(sceRtcSetYear(&converted, 9999) == 0 && sceRtcGetYear(&converted) == 9999);
    Require(sceRtcSetYear(&converted, 2026) == 0 && sceRtcGetYear(&converted) == 2026);

    Require(sceRtcGetMonth(nullptr) == invalidPointer);
    Require(sceRtcSetMonth(nullptr, 6) == invalidPointer);
    Require(sceRtcSetMonth(&converted, 0) == invalidMonth);
    Require(sceRtcSetMonth(&converted, 13) == invalidMonth);
    Require(sceRtcSetMonth(&converted, 1) == 0 && sceRtcGetMonth(&converted) == 1);
    Require(sceRtcSetMonth(&converted, 12) == 0 && sceRtcGetMonth(&converted) == 12);

    Require(sceRtcGetDay(nullptr) == invalidPointer);
    Require(sceRtcSetDay(nullptr, 15) == invalidPointer);
    Require(sceRtcSetDay(&converted, 0) == invalidDay);
    Require(sceRtcSetDay(&converted, 32) == invalidDay);
    Require(sceRtcSetDay(&converted, 1) == 0 && sceRtcGetDay(&converted) == 1);
    Require(sceRtcSetDay(&converted, 31) == 0 && sceRtcGetDay(&converted) == 31);

    Require(sceRtcGetHour(nullptr) == invalidPointer);
    Require(sceRtcSetHour(nullptr, 12) == invalidPointer);
    Require(sceRtcSetHour(&converted, -1) == invalidHour);
    Require(sceRtcSetHour(&converted, 24) == invalidHour);
    Require(sceRtcSetHour(&converted, 0) == 0 && sceRtcGetHour(&converted) == 0);
    Require(sceRtcSetHour(&converted, 23) == 0 && sceRtcGetHour(&converted) == 23);

    constexpr int invalidMinute = static_cast<int>(0x80B5000C);
    constexpr int invalidSecond = static_cast<int>(0x80B5000D);

    Require(sceRtcGetMinute(nullptr) == invalidPointer);
    Require(sceRtcSetMinute(nullptr, 30) == invalidPointer);
    Require(sceRtcSetMinute(&converted, -1) == invalidMinute);
    Require(sceRtcSetMinute(&converted, 60) == invalidMinute);
    Require(sceRtcSetMinute(&converted, 0) == 0 && sceRtcGetMinute(&converted) == 0);
    Require(sceRtcSetMinute(&converted, 59) == 0 && sceRtcGetMinute(&converted) == 59);

    Require(sceRtcGetSecond(nullptr) == invalidPointer);
    Require(sceRtcSetSecond(nullptr, 45) == invalidPointer);
    Require(sceRtcSetSecond(&converted, -1) == invalidSecond);
    Require(sceRtcSetSecond(&converted, 60) == invalidSecond);
    Require(sceRtcSetSecond(&converted, 0) == 0 && sceRtcGetSecond(&converted) == 0);
    Require(sceRtcSetSecond(&converted, 59) == 0 && sceRtcGetSecond(&converted) == 59);

    Require(sceRtcGetMicrosecond(nullptr) == invalidPointer);
    Require(sceRtcSetMicrosecond(nullptr, 123456) == invalidPointer);
    Require(sceRtcSetMicrosecond(&converted, -1) == invalidMicrosecond);
    Require(sceRtcSetMicrosecond(&converted, 1000000) == invalidMicrosecond);
    Require(sceRtcSetMicrosecond(&converted, 0) == 0 && sceRtcGetMicrosecond(&converted) == 0);
    Require(sceRtcSetMicrosecond(&converted, 999999) == 0 && sceRtcGetMicrosecond(&converted) == 999999);
    converted = RtcDateTime{1, 1, 1, 1, 1, 1, 1};
    Require(sceRtcSetDosTime(&converted, 0x585d645cu) == 0 && Equal(converted, RtcDateTime{2024, 2, 29, 12, 34, 56, 0}));
    Require(sceRtcSetDosTime(&converted, 0x7f9fbf7du) == 0 && Equal(converted, RtcDateTime{2043, 12, 31, 23, 59, 58, 0}));
    Require(sceRtcSetDosTime(&converted, 0) == 0 && Equal(converted, RtcDateTime{1980, 0, 0, 0, 0, 0, 0}));
    Require(sceRtcSetDosTime(nullptr, 0) == invalidPointer);
    Require(sceRtcSetDosTime(&converted, 0xff9fbf7du) == 0 && Equal(converted, RtcDateTime{2107, 12, 31, 23, 59, 58, 0}));
    Require(sceRtcSetDosTime(&converted, 0x80210000u) == 0 && Equal(converted, RtcDateTime{2044, 1, 1, 0, 0, 0, 0}));

    char text[32];
    tick.tick = leapDayTick;
    Require(sceRtcFormatRFC3339(text, &tick, 0) == 0 && std::strcmp(text, "2024-02-29T12:34:56.78Z") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, 90) == 0 && std::strcmp(text, "2024-02-29T14:04:56.78+01:30") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, -300) == 0 && std::strcmp(text, "2024-02-29T07:34:56.78-05:00") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, 1439) == 0 && std::strcmp(text, "2024-03-01T12:33:56.78+23:59") == 0);
    Require(sceRtcFormatRFC3339(text, &tick, -1439) == 0 && std::strcmp(text, "2024-02-28T12:35:56.78-23:59") == 0);
    for (int offset : {0, 1, -1, 59, -59, 60, -60, 90, -300, 1439, -1439}) {
        RtcTick parsed{};
        Require(sceRtcFormatRFC3339(text, &tick, offset) == 0);
        Require(sceRtcParseRFC3339(&parsed, text) == 0 && parsed.tick == leapDayTick - 9000);
    }
    for (int offset : {1440, -1440, 6000, -6000, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        std::memset(text, 'x', sizeof(text));
        char original[sizeof(text)];
        std::memcpy(original, text, sizeof(text));
        Require(sceRtcFormatRFC3339(text, &tick, offset) == invalidValue);
        Require(std::memcmp(text, original, sizeof(text)) == 0);
    }
    RtcTick edge{maxTick + 1};
    Require(sceRtcFormatRFC3339(text, &edge, 0) == invalidYear);
    edge.tick = std::numeric_limits<std::uint64_t>::max();
    Require(sceRtcFormatRFC3339(text, &edge, 0) == invalidYear);
    edge.tick = maxTick;
    Require(sceRtcFormatRFC3339(text, &edge, 1) == invalidYear);
    edge.tick = 59999999;
    Require(sceRtcFormatRFC3339(text, &edge, -1) == invalidYear);
    edge.tick = 60000000;
    Require(sceRtcFormatRFC3339(text, &edge, -1) == 0 && std::strcmp(text, "0001-01-01T00:00:00.00-00:01") == 0);
    edge.tick = std::numeric_limits<std::uint64_t>::max();
    Require(sceRtcFormatRFC3339(text, &edge, 1) == 0 && std::strcmp(text, "0001-01-01T00:00:59.99+00:01") == 0);
    edge.tick = 1ull << 63;
    Require(sceRtcFormatRFC3339(text, &edge, -1) == invalidYear);
    edge.tick = maxTick + 1;
    Require(sceRtcFormatRFC3339(text, &edge, -1) == 0 && std::strcmp(text, "9999-12-31T23:59:00.00-00:01") == 0);
    Require(sceRtcFormatRFC3339(nullptr, &tick, 1440) == invalidPointer);
    Require(sceRtcFormatRFC3339(text, nullptr, 1440) == invalidValue);
    {
        RtcTick before{}, after{}, parsed{};
        Require(sceRtcGetCurrentTick(&before) == 0);
        Require(sceRtcFormatRFC3339(text, nullptr, 0) == 0);
        Require(sceRtcGetCurrentTick(&after) == 0);
        Require(sceRtcParseRFC3339(&parsed, text) == 0 && parsed.tick + 10000 > before.tick && parsed.tick <= after.tick);
    }
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T14:04:56.789+01:30") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29t12:34:56.789z") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "1970-01-01T00:00:00Z") == 0 && tick.tick == unixEpochTick);
    Require(sceRtcParseRFC3339(&tick, "2023-02-29T00:00:00Z") == invalidDay);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56") == badParse);
    const std::uint64_t secondTick = leapDayTick - 789000ull;
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56Zjunk") == 0 && tick.tick == secondTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T14:34:56+02:00 trailing") == 0 && tick.tick == secondTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.Z") == 0 && tick.tick == secondTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.1234567Z") == 0 && tick.tick == secondTick + 123456ull);
    const struct {
        const char* text;
        std::int64_t minutes;
    } unrangedOffsets[] = {
        {"2024-02-29T12:34:56.789+00:99", 99},
        {"2024-02-29T12:34:56.789-00:99", -99},
        {"2024-02-29T12:34:56.789+00:60", 60},
        {"2024-02-29T12:34:56.789-00:60", -60},
        {"2024-02-29T12:34:56.789+24:00", 1440},
        {"2024-02-29T12:34:56.789-24:00", -1440},
        {"2024-02-29T12:34:56.789+99:59", 5999},
        {"2024-02-29T12:34:56.789-99:99", -6039},
    };
    for (const auto& offset : unrangedOffsets) {
        Require(sceRtcParseRFC3339(&tick, offset.text) == 0);
        Require(tick.tick == leapDayTick - static_cast<std::uint64_t>(offset.minutes * 60000000));
    }
    for (const char* text : {"2024-02-29 12:34:56Z", "2024-02-29T12:34:56+0100", "2024-02-29T12:34:56+01", "2024-02-29T12:34:56 Z", " 2024-02-29T12:34:56Z", "2024-02-29T12:34:56..Z", "2024-2-29T12:34:56Z"}) {
        tick.tick = 123;
        Require(sceRtcParseRFC3339(&tick, text) == badParse);
        Require(tick.tick == 123);
    }
    tick.tick = 123;
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:60Z") == 0 && tick.tick == 123);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:60+01:00") == 0 && tick.tick == 123ull - 3600000000ull);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:61Z") == static_cast<int>(0x80B5000D));
    Require(sceRtcParseRFC3339(&tick, "0000-13-40T99:99:99Z") == static_cast<int>(0x80B50008));
    Require(sceRtcParseRFC3339(&tick, "0001-01-01T00:00:00+01:00") == 0 && tick.tick == 0xFFFFFFFF296C5C00ull);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789+00:00") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789-00:00") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789+00:59") == 0 && tick.tick == leapDayTick - 3540000000ull);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789-00:59") == 0 && tick.tick == leapDayTick + 3540000000ull);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789+23:59") == 0 && tick.tick == leapDayTick - 86340000000ull);
    Require(sceRtcParseRFC3339(&tick, "2024-02-29T12:34:56.789-23:59") == 0 && tick.tick == leapDayTick + 86340000000ull);
    Require(sceRtcParseRFC3339(nullptr, "1970-01-01T00:00:00Z") == invalidPointer);

    for (const char* text : {"2024-02-29T12:34:56.789", "2024-02-29 12:34:56", "2024-02-29 12:34:56Z", "2024-02-29", "2024/02/29T12:34:56Z", "2024-02-29T12:34:56."}) {
        tick.tick = 123;
        Require(sceRtcParseDateTime(&tick, text) == badParse);
        Require(tick.tick == 123);
    }
    Require(sceRtcParseDateTime(&tick, " \t 2024-02-29T12:34:56.789Z") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseDateTime(&tick, "2024-02-29T12:34:56.789Zjunk") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseDateTime(&tick, "2024-02-29T12:34:56.789+99:99") == 0 && tick.tick == leapDayTick - 6039ull * 60000000ull);
    tick.tick = 123;
    Require(sceRtcParseDateTime(&tick, "2024-02-29T12:34:60-01:00") == 0 && tick.tick == 123ull + 3600000000ull);
    Require(sceRtcParseDateTime(&tick, "2024-13-01T00:00:00Z") == static_cast<int>(0x80B50009));
    Require(sceRtcParseDateTime(&tick, "2024-02-29T14:04:56.789+01:30") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseDateTime(&tick, "2024-02-29t12:34:56.789z") == 0 && tick.tick == leapDayTick);
    Require(sceRtcParseDateTime(&tick, "1970-01-01T00:00:00Z") == 0 && tick.tick == unixEpochTick);
    Require(sceRtcParseDateTime(&tick, "2023-02-29T00:00:00Z") == invalidDay);
    Require(sceRtcParseDateTime(nullptr, "1970-01-01T00:00:00Z") == invalidPointer);
    Require(sceRtcParseDateTime(&tick, "Thu, 29 Feb 2024 12:34:56") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu, 29 Feb 2024 12:34:56 GMT") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu, 29 Feb 2024 14:04:56 +0130") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu, 29 Feb 2024 11:04:56 -0130") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu, 01 Jan 1970 00:00:00 +0000") == 0 && tick.tick == unixEpochTick);
    Require(sceRtcParseDateTime(&tick, "Thu Feb 29 12:34:56 2024") == 0 && tick.tick == leapDayTick - 789000ull);
    Require(sceRtcParseDateTime(&tick, "Thu Jan  1 00:00:00 1970") == 0 && tick.tick == unixEpochTick);
    const auto at = [](int year, int month, int day, int hour, int minute, int second) {
        const RtcDateTime time{static_cast<std::uint16_t>(year), static_cast<std::uint16_t>(month), static_cast<std::uint16_t>(day),
            static_cast<std::uint16_t>(hour), static_cast<std::uint16_t>(minute), static_cast<std::uint16_t>(second), 0};
        RtcTick result{};
        Require(sceRtcGetTick(&time, &result) == 0);
        return result.tick;
    };
    constexpr std::uint64_t untouched = 123;
    const std::uint64_t march = at(2024, 3, 1, 12, 34, 56);
    const struct {
        const char* text;
        int result;
        std::uint64_t local;
        std::int64_t minutes;
    } dateTimes[] = {
        {"thursday,29-feb-24 12:34:56", 0, leapDayTick - 789000ull, 0},
        {"Mon 29 February 2024 12:34:56 -0130", 0, leapDayTick - 789000ull, -90},
        {" \tFri, 01 Mar 2024 12:34:56 GMT", 0, march, 0},
        {"Thu, 1-Mar 2024 12:34:56 -9999", 0, march, -6039},
        {"Thu, 01 Mar 2024 12:34:56 +01:30", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 +0130x", 0, march, 90},
        {"Thu, 01 Mar 2024 12:34:56 PST8PDT", 0, march, -480},
        {"Thu, 01 Mar 2024 12:34:56 nzdt", 0, march, 780},
        {"Thu, 01 Mar 2024 12:34:56 KST", 0, march, 540},
        {"Thu, 01 Mar 2024 12:34:56 HST", 0, march, 420},
        {"Thu, 01 Mar 2024 12:34:56 DNT", 0, march, 60},
        {"Thu, 01 Mar 2024 12:34:56 NFT", 0, march, -150},
        {"Thu, 01 Mar 2024 12:34:56 NOR", 0, march, 60},
        {"Thu, 01 Mar 2024 12:34:56 SET", 0, march, 60},
        {"Thu, 01 Mar 2024 12:34:56 SETX", 0, march, 60},
        {"Thu, 01 Mar 2024 12:34:56 Jt", 0, march, 450},
        {"Thu, 01 Mar 2024 12:34:56 ut", 0, march, -420},
        {"Thu, 01 Mar 2024 12:34:56 xT", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 Ux", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 B", 0, march, 60},
        {"Thu, 01 Mar 2024 12:34:56 m", 0, march, 720},
        {"Thu, 01 Mar 2024 12:34:56 N", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 y", 0, march, -660},
        {"Thu, 01 Mar 2024 12:34:56 Z", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 J", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 12:34:56  +0100", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 12:34:56 (UTC)", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 12:34:56\tEST", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:56 ", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34:567 EST", 0, march, 0},
        {"Thu, 01 Mar 2024 12:34 EST", 0, at(2024, 3, 1, 12, 34, 0), 0},
        {"Thu, 29 Feb 2024 12:34", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 12:34", badParse, untouched, 0},
        {"Thu, 29 Feb 2024 12:34 ", 0, at(2024, 2, 29, 12, 34, 0), 0},
        {"Thu, 01 Mar 2024 12:34EST", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 1:2:3 EST", 0, at(2024, 3, 1, 1, 2, 3), -300},
        {"Thu, 01 Mar 2024 26:00:00 GMT", badParse, untouched, 0},
        {"Thu, 01 Mar 2024 25:00:00 +0100", 0, untouched, 60},
        {"Thu, 29 Feb 2023 12:34:56 EST", 0, untouched, -300},
        {"Thu, 01 Mar 49 12:34:56 GMT", 0, at(2049, 3, 1, 12, 34, 56), 0},
        {"Thu, 01 Mar 50 12:34:56 GMT", 0, at(1950, 3, 1, 12, 34, 56), 0},
        {"Mon, 01 Jan 0001 00:00:00 +0100", 0, 0, 60},
        {"Thu, 01 Mar 024 12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 01 Mar 10000 12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 001 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 01  Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 01 Mar 2024\t12:34:56 GMT", badParse, untouched, 0},
        {"Thu, 01 Sept 2024 12:34:56 GMT", badParse, untouched, 0},
        {"Thurs, 01 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"Thu , 01 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {", 01 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"\nThu, 01 Mar 2024 12:34:56 GMT", badParse, untouched, 0},
        {"THURSDAY february  9 1:2:3 2024 +0100", 0, at(2024, 2, 9, 1, 2, 3), 0},
        {"ThuMar  1 12:34:56 10000", 0, at(1000, 3, 1, 12, 34, 56), 0},
        {"Thu,\tFeb 29 12:34:56 2024", 0, leapDayTick - 789000ull, 0},
        {"Thu Feb 29 99:99:99 2024", 0, untouched, 0},
        {"Thu Feb  29 12:34:56 2024", badParse, untouched, 0},
        {"Thu Feb 29 12:34 2024", badParse, untouched, 0},
        {"Thu Feb 29 12:34:56 24", badParse, untouched, 0},
        {"Thu Feb 29 12:34:56\t2024", badParse, untouched, 0},
        {"Feb 29 12:34:56 2024", badParse, untouched, 0},
        {"\n2024-02-29T12:34:56Z", badParse, untouched, 0},
        {"+2024-02-29T12:34:56Z", badParse, untouched, 0},
        {"", badParse, untouched, 0},
    };
    for (const auto& dateTime : dateTimes) {
        tick.tick = untouched;
        Require(sceRtcParseDateTime(&tick, dateTime.text) == dateTime.result);
        Require(tick.tick == dateTime.local - static_cast<std::uint64_t>(dateTime.minutes * 60000000));
    }

    const struct {
        const char* name;
        std::int64_t minutes;
    } additionalZones[] = {
        {"LIGT", 600}, {"AWSST", 540}, {"CETDST", 120},
        {"DNT", 60}, {"NOR", 60}, {"SET", 60}, {"NFT", -150},
        {"SWT", 60}, {"MEZ", 60}, {"WETDST", 60}, {"METDST", 120}, {"EETDST", 180},
        {"AESST", 660}, {"ACSST", 630}, {"CAST", 570}, {"CADT", 630}, {"SAST", 570}, {"SADT", 630}, {"WST", 480}, {"WDT", 540},
    };
    for (const auto& zone : additionalZones) {
        char text[80];
        std::snprintf(text, sizeof(text), "Thu, 29 Feb 2024 12:34:56 %s", zone.name);
        tick.tick = untouched;
        Require(sceRtcParseDateTime(&tick, text) == 0);
        Require(tick.tick == leapDayTick - 789000ull - static_cast<std::uint64_t>(zone.minutes * 60000000));
        char lower[16]{};
        for (std::size_t i = 0; zone.name[i] != 0; ++i) lower[i] = static_cast<char>(zone.name[i] + ('a' - 'A'));
        std::snprintf(text, sizeof(text), "Thu, 29 Feb 2024 12:34:56 %s", lower);
        tick.tick = untouched;
        Require(sceRtcParseDateTime(&tick, text) == 0);
        Require(tick.tick == leapDayTick - 789000ull - static_cast<std::uint64_t>(zone.minutes * 60000000));
        std::snprintf(text, sizeof(text), "Thu, 29 Feb 2024 12:34:56 %sextra", zone.name);
        tick.tick = untouched;
        Require(sceRtcParseDateTime(&tick, text) == 0);
        Require(tick.tick == leapDayTick - 789000ull - static_cast<std::uint64_t>(zone.minutes * 60000000));
        std::snprintf(text, sizeof(text), "Wed, 01 Mar 2000 08:09:10 %s", zone.name);
        tick.tick = untouched;
        Require(sceRtcParseDateTime(&tick, text) == 0);
        Require(tick.tick == at(2000, 3, 1, 8, 9, 10) - static_cast<std::uint64_t>(zone.minutes * 60000000));
    }

    const struct {
        const char* name;
        std::int64_t minutes;
    } secondLetterZones[] = {
        {"MT", 0}, {"mt", 510}, {"IT", 0}, {"it", 210},
        {"BT", 0}, {"bt", 180}, {"NT", 0}, {"nt", -660},
    };
    for (const auto& zone : secondLetterZones) {
        char text[80];
        std::snprintf(text, sizeof(text), "Thu, 29 Feb 2024 12:34:56 %s", zone.name);
        Require(sceRtcParseDateTime(&tick, text) == 0);
        Require(tick.tick == leapDayTick - 789000ull - static_cast<std::uint64_t>(zone.minutes * 60000000));
    }

    RtcTick source{leapDayTick};
    RtcTick result{};
    Require(sceRtcTickAddSeconds(&result, &source, 3600) == 0 && result.tick == leapDayTick + 3600000000ull);
    Require(sceRtcTickAddDays(&result, &source, 1) == 0);
    Require(sceRtcSetTick(&converted, &result) == 0 && converted.month == 3 && converted.day == 1);
    Require(sceRtcTickAddYears(&result, &source, 1) == 0);
    Require(sceRtcSetTick(&converted, &result) == 0 && converted.year == 2025 && converted.month == 2 && converted.day == 28);
    RtcDateTime endOfJanuary{2024, 1, 31, 8, 0, 0, 0};
    Require(sceRtcGetTick(&endOfJanuary, &source) == 0);
    Require(sceRtcTickAddMonths(&result, &source, 1) == 0);
    Require(sceRtcSetTick(&converted, &result) == 0 && Equal(converted, RtcDateTime{2024, 2, 29, 8, 0, 0, 0}));
    source.tick = 5;
    Require(sceRtcTickAddTicks(&result, &source, -5) == 0 && result.tick == 0);
    source.tick = maxTick;
    Require(sceRtcTickAddTicks(nullptr, &source, 1) == invalidPointer);
    source.tick = leapDayTick;
    result.tick = 123;
    Require(sceRtcTickAddMicroseconds(&result, &source, 1500) == 0 && result.tick == leapDayTick + 1500);
    Require(sceRtcTickAddMinutes(&result, &source, 90) == 0 && result.tick == leapDayTick + 5400000000ull);
    Require(sceRtcTickAddHours(&result, &source, -2) == 0 && result.tick == leapDayTick - 7200000000ull);
    Require(sceRtcTickAddWeeks(&result, &source, 1) == 0 && result.tick == leapDayTick + 604800000000ull);
    Require(sceRtcSetTick(&converted, &result) == 0 && converted.month == 3 && converted.day == 7);
    result.tick = 123;
    Require(sceRtcTickAddHours(nullptr, &source, 1) == invalidPointer);
    Require(sceRtcTickAddMicroseconds(&result, nullptr, 1) == invalidPointer);
    source.tick = maxTick;
    Require(sceRtcTickAddTicks(&source, &source, 0) == 0 && source.tick == maxTick);
    Require(sceRtcTickAddTicks(&source, &source, -1) == 0 && source.tick == maxTick - 1);
    source.tick = maxTick;
    Require(sceRtcTickAddMonths(&source, &source, 0) == 0 && source.tick == maxTick);
    Require(sceRtcTickAddYears(&result, &source, -1) == 0);
    Require(sceRtcSetTick(&converted, &result) == 0 && Equal(converted, RtcDateTime{9998, 12, 31, 23, 59, 59, 999999}));
    source.tick = 0;
    Require(sceRtcTickAddMonths(&source, &source, 0) == 0 && source.tick == 0);

    constexpr std::int64_t min64 = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t max64 = std::numeric_limits<std::int64_t>::max();
    constexpr std::uint64_t addSentinel = 0xA5A5A5A5A5A5A5A5ull;
    const AddCase addCases[] = {
        {Add::Ticks, 0, -1, true, 0xffffffffffffffffull},
        {Add::Ticks, 1ull, -1, true, 0x0000000000000000ull},
        {Add::Ticks, 5, -1, true, 0x0000000000000004ull},
        {Add::Ticks, leapDayTick, 1, true, 0x00e2d2837bd62609ull},
        {Add::Ticks, maxTick - 1ull, 1, true, 0x0461040bcb9f1fffull},
        {Add::Ticks, maxTick, 1, true, 0x0461040bcb9f2000ull},
        {Add::Ticks, maxTick + 1, 1, true, 0x0461040bcb9f2001ull},
        {Add::Ticks, ~0ull, 1, true, 0x0000000000000000ull},
        {Add::Ticks, 0, max64, true, 0x7fffffffffffffffull},
        {Add::Ticks, 0, min64, true, 0x8000000000000000ull},
        {Add::Ticks, maxTick, max64, true, 0x8461040bcb9f1ffeull},
        {Add::Ticks, ~0ull, min64, true, 0x7fffffffffffffffull},
        {Add::Microseconds, 0, -1, true, 0xffffffffffffffffull},
        {Add::Microseconds, 1ull, -1, true, 0x0000000000000000ull},
        {Add::Microseconds, 5, -1, true, 0x0000000000000004ull},
        {Add::Microseconds, leapDayTick, 1, true, 0x00e2d2837bd62609ull},
        {Add::Microseconds, maxTick - 1ull, 1, true, 0x0461040bcb9f1fffull},
        {Add::Microseconds, maxTick, 1, true, 0x0461040bcb9f2000ull},
        {Add::Microseconds, maxTick + 1, 1, true, 0x0461040bcb9f2001ull},
        {Add::Microseconds, ~0ull, 1, true, 0x0000000000000000ull},
        {Add::Microseconds, 0, max64, true, 0x7fffffffffffffffull},
        {Add::Microseconds, 0, min64, true, 0x8000000000000000ull},
        {Add::Microseconds, maxTick, max64, true, 0x8461040bcb9f1ffeull},
        {Add::Microseconds, ~0ull, min64, true, 0x7fffffffffffffffull},
        {Add::Seconds, 0, -1, true, 0xfffffffffff0bdc0ull},
        {Add::Seconds, 1000000ull, -1, true, 0x0000000000000000ull},
        {Add::Seconds, 999999ull, -1, true, 0xffffffffffffffffull},
        {Add::Seconds, leapDayTick, 1, true, 0x00e2d2837be56848ull},
        {Add::Seconds, maxTick - 1000000ull, 1, true, 0x0461040bcb9f1fffull},
        {Add::Seconds, maxTick + 1 - 1000000ull, 1, true, 0x0461040bcb9f2000ull},
        {Add::Seconds, maxTick + 1, 1, true, 0x0461040bcbae6240ull},
        {Add::Seconds, 0ull - 1000000ull, 1, true, 0x0000000000000000ull},
        {Add::Seconds, 0, max64, true, 0xfffffffffff0bdc0ull},
        {Add::Seconds, 0, min64, true, 0x0000000000000000ull},
        {Add::Seconds, maxTick, max64, true, 0x0461040bcb8fddbfull},
        {Add::Seconds, ~0ull, min64, true, 0xffffffffffffffffull},
        {Add::Minutes, 0, -1, true, 0xfffffffffc6c7900ull},
        {Add::Minutes, 60000000ull, -1, true, 0x0000000000000000ull},
        {Add::Minutes, 59999999ull, -1, true, 0xffffffffffffffffull},
        {Add::Minutes, leapDayTick, 1, true, 0x00e2d2837f69ad08ull},
        {Add::Minutes, maxTick - 60000000ull, 1, true, 0x0461040bcb9f1fffull},
        {Add::Minutes, maxTick + 1 - 60000000ull, 1, true, 0x0461040bcb9f2000ull},
        {Add::Minutes, maxTick + 1, 1, true, 0x0461040bcf32a700ull},
        {Add::Minutes, 0ull - 60000000ull, 1, true, 0x0000000000000000ull},
        {Add::Minutes, 0, max64, true, 0xfffffffffc6c7900ull},
        {Add::Minutes, 0, min64, true, 0x0000000000000000ull},
        {Add::Minutes, maxTick, max64, true, 0x0461040bc80b98ffull},
        {Add::Minutes, ~0ull, min64, true, 0xffffffffffffffffull},
        {Add::Hours, 0, -1, true, 0xffffffff296c5c00ull},
        {Add::Hours, 3600000000ull, -1, true, 0x0000000000000000ull},
        {Add::Hours, 3599999999ull, -1, true, 0xffffffffffffffffull},
        {Add::Hours, leapDayTick, 1, true, 0x00e2d2845269ca08ull},
        {Add::Hours, maxTick - 3600000000ull, 1, true, 0x0461040bcb9f1fffull},
        {Add::Hours, maxTick + 1 - 3600000000ull, 1, true, 0x0461040bcb9f2000ull},
        {Add::Hours, maxTick + 1, 1, true, 0x0461040ca232c400ull},
        {Add::Hours, 0ull - 3600000000ull, 1, true, 0x0000000000000000ull},
        {Add::Hours, 0, 2147483647, true, 0x6b49d1ff296c5c00ull},
        {Add::Hours, 0, -2147483648, true, 0x94b62e0000000000ull},
        {Add::Hours, maxTick, 2147483647, true, 0x6faad60af50b7bffull},
        {Add::Hours, ~0ull, -2147483648, true, 0x94b62dffffffffffull},
        {Add::Days, 0, -1, true, 0xffffffebe228a000ull},
        {Add::Days, 86400000000ull, -1, true, 0x0000000000000000ull},
        {Add::Days, 86399999999ull, -1, true, 0xffffffffffffffffull},
        {Add::Days, leapDayTick, 1, true, 0x00e2d29799ad8608ull},
        {Add::Days, maxTick - 86400000000ull, 1, true, 0x0461040bcb9f1fffull},
        {Add::Days, maxTick + 1 - 86400000000ull, 1, true, 0x0461040bcb9f2000ull},
        {Add::Days, maxTick + 1, 1, true, 0x0461041fe9768000ull},
        {Add::Days, 0ull - 86400000000ull, 1, true, 0x0000000000000000ull},
        {Add::Days, 0, 2147483647, true, 0x0eebafebe228a000ull},
        {Add::Days, 0, -2147483648, true, 0xf114500000000000ull},
        {Add::Days, maxTick, 2147483647, true, 0x134cb3f7adc7bfffull},
        {Add::Days, ~0ull, -2147483648, true, 0xf1144fffffffffffull},
        {Add::Weeks, 0, -1, true, 0xffffff732f1c6000ull},
        {Add::Weeks, 604800000000ull, -1, true, 0x0000000000000000ull},
        {Add::Weeks, 604799999999ull, -1, true, 0xffffffffffffffffull},
        {Add::Weeks, leapDayTick, 1, true, 0x00e2d3104cb9c608ull},
        {Add::Weeks, maxTick - 604800000000ull, 1, true, 0x0461040bcb9f1fffull},
        {Add::Weeks, maxTick + 1 - 604800000000ull, 1, true, 0x0461040bcb9f2000ull},
        {Add::Weeks, maxTick + 1, 1, true, 0x046104989c82c000ull},
        {Add::Weeks, 0ull - 604800000000ull, 1, true, 0x0000000000000000ull},
        {Add::Weeks, 0, 2147483647, true, 0x6871cf732f1c6000ull},
        {Add::Weeks, 0, -2147483648, true, 0x978e300000000000ull},
        {Add::Weeks, maxTick, 2147483647, true, 0x6cd2d37efabb7fffull},
        {Add::Weeks, ~0ull, -2147483648, true, 0x978e2fffffffffffull},
        {Add::Months, 0, -1, false, 0},
        {Add::Months, 0, 0, true, 0x0000000000000000ull},
        {Add::Months, 0, 1, true, 0x0000026f9d14a000ull},
        {Add::Months, 0, -12, false, 0},
        {Add::Months, 0, 12, true, 0x00001cae8c13e000ull},
        {Add::Months, 0, 119988, false, 0},
        {Add::Months, 0, 2147483647, false, 0},
        {Add::Months, 0, -2147483648, false, 0},
        {Add::Months, 0, -1239376452, true, 0x017942765fc16000ull},
        {Add::Months, 0, -1239376451, false, 0},
        {Add::Months, at(1, 2, 1, 0, 0, 0), -1, true, 0x0000000000000000ull},
        {Add::Months, at(1, 2, 1, 0, 0, 0), 0, true, 0x0000026f9d14a000ull},
        {Add::Months, at(1, 2, 1, 0, 0, 0), 1, true, 0x000004a2e0a32000ull},
        {Add::Months, at(1, 2, 1, 0, 0, 0), -12, false, 0},
        {Add::Months, at(1, 2, 1, 0, 0, 0), 12, true, 0x00001f1e29288000ull},
        {Add::Months, at(1, 2, 1, 0, 0, 0), 119988, false, 0},
        {Add::Months, at(1, 2, 1, 0, 0, 0), 2147483647, false, 0},
        {Add::Months, at(1, 2, 1, 0, 0, 0), -2147483648, false, 0},
        {Add::Months, at(1, 2, 1, 0, 0, 0), -1239376452, false, 0},
        {Add::Months, at(1, 2, 1, 0, 0, 0), -1239376451, false, 0},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), -1, true, 0x00e2cdc8a612a000ull},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), 0, true, 0x00e2d03843274000ull},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), 1, true, 0x00e2d27fa48d2000ull},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), -12, true, 0x00e2b389b7136000ull},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), 12, true, 0x00e2ecfaed128000ull},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), 119988, false, 0},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), 2147483647, false, 0},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), -2147483648, false, 0},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), -1239376452, true, 0x025c12c2c0c00000ull},
        {Add::Months, at(2024, 1, 31, 8, 0, 0), -1239376451, false, 0},
        {Add::Months, leapDayTick, -1, true, 0x00e2d013dec18608ull},
        {Add::Months, leapDayTick, 0, true, 0x00e2d2837bd62608ull},
        {Add::Months, leapDayTick, 1, true, 0x00e2d4cadd3c0608ull},
        {Add::Months, leapDayTick, -12, true, 0x00e2b5c0d1eae608ull},
        {Add::Months, leapDayTick, 12, true, 0x00e2ef3207ea0608ull},
        {Add::Months, leapDayTick, 119988, false, 0},
        {Add::Months, leapDayTick, 2147483647, false, 0},
        {Add::Months, leapDayTick, -2147483648, false, 0},
        {Add::Months, leapDayTick, -1239376452, false, 0},
        {Add::Months, leapDayTick, -1239376451, false, 0},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), -1, true, 0x0460e4eda276a000ull},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), 0, true, 0x0460e75d3f8b4000ull},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), 1, true, 0x0460e9ccdc9fe000ull},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), -12, true, 0x0460caaeb3776000ull},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), 12, false, 0},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), 119988, false, 0},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), 2147483647, false, 0},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), -2147483648, false, 0},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), -1239376452, false, 0},
        {Add::Months, at(9999, 1, 1, 0, 0, 0), -1239376451, false, 0},
        {Add::Months, maxTick, -1, true, 0x0461019c2e8a7fffull},
        {Add::Months, maxTick, 0, true, 0x0461040bcb9f1fffull},
        {Add::Months, maxTick, 1, false, 0},
        {Add::Months, maxTick, -12, true, 0x0460e75d3f8b3fffull},
        {Add::Months, maxTick, 12, false, 0},
        {Add::Months, maxTick, 119988, false, 0},
        {Add::Months, maxTick, 2147483647, false, 0},
        {Add::Months, maxTick, -2147483648, false, 0},
        {Add::Months, maxTick, -1239376452, false, 0},
        {Add::Months, maxTick, -1239376451, false, 0},
        {Add::Months, maxTick + 1, -1, true, 0x0461019c2e8a8000ull},
        {Add::Months, maxTick + 1, 0, false, 0},
        {Add::Months, maxTick + 1, 1, false, 0},
        {Add::Months, maxTick + 1, -12, true, 0x0460e75d3f8b4000ull},
        {Add::Months, maxTick + 1, 12, false, 0},
        {Add::Months, maxTick + 1, 119988, false, 0},
        {Add::Months, maxTick + 1, 2147483647, false, 0},
        {Add::Months, maxTick + 1, -2147483648, false, 0},
        {Add::Months, maxTick + 1, -1239376452, false, 0},
        {Add::Months, maxTick + 1, -1239376451, false, 0},
        {Add::Months, ~0ull, -1, false, 0},
        {Add::Months, ~0ull, 0, false, 0},
        {Add::Months, ~0ull, 1, false, 0},
        {Add::Months, ~0ull, -12, false, 0},
        {Add::Months, ~0ull, 12, false, 0},
        {Add::Months, ~0ull, 119988, true, 0x02122fd805921fffull},
        {Add::Months, ~0ull, 2147483647, false, 0},
        {Add::Months, ~0ull, -2147483648, false, 0},
        {Add::Months, ~0ull, -1239376452, false, 0},
        {Add::Months, ~0ull, -1239376451, false, 0},
        {Add::Years, 0, -1, false, 0},
        {Add::Years, 0, 0, true, 0x0000000000000000ull},
        {Add::Years, 0, 1, true, 0x00001cae8c13e000ull},
        {Add::Years, 0, 9998, true, 0x0460e75d3f8b4000ull},
        {Add::Years, 0, 32767, false, 0},
        {Add::Years, 0, -32768, false, 0},
        {Add::Years, at(1, 2, 1, 0, 0, 0), -1, false, 0},
        {Add::Years, at(1, 2, 1, 0, 0, 0), 0, true, 0x0000026f9d14a000ull},
        {Add::Years, at(1, 2, 1, 0, 0, 0), 1, true, 0x00001f1e29288000ull},
        {Add::Years, at(1, 2, 1, 0, 0, 0), 9998, true, 0x0460e9ccdc9fe000ull},
        {Add::Years, at(1, 2, 1, 0, 0, 0), 32767, false, 0},
        {Add::Years, at(1, 2, 1, 0, 0, 0), -32768, false, 0},
        {Add::Years, at(2024, 1, 31, 8, 0, 0), -1, true, 0x00e2b389b7136000ull},
        {Add::Years, at(2024, 1, 31, 8, 0, 0), 0, true, 0x00e2d03843274000ull},
        {Add::Years, at(2024, 1, 31, 8, 0, 0), 1, true, 0x00e2ecfaed128000ull},
        {Add::Years, at(2024, 1, 31, 8, 0, 0), 9998, false, 0},
        {Add::Years, at(2024, 1, 31, 8, 0, 0), 32767, false, 0},
        {Add::Years, at(2024, 1, 31, 8, 0, 0), -32768, false, 0},
        {Add::Years, leapDayTick, -1, true, 0x00e2b5c0d1eae608ull},
        {Add::Years, leapDayTick, 0, true, 0x00e2d2837bd62608ull},
        {Add::Years, leapDayTick, 1, true, 0x00e2ef3207ea0608ull},
        {Add::Years, leapDayTick, 9998, false, 0},
        {Add::Years, leapDayTick, 32767, false, 0},
        {Add::Years, leapDayTick, -32768, false, 0},
        {Add::Years, at(9999, 1, 1, 0, 0, 0), -1, true, 0x0460caaeb3776000ull},
        {Add::Years, at(9999, 1, 1, 0, 0, 0), 0, true, 0x0460e75d3f8b4000ull},
        {Add::Years, at(9999, 1, 1, 0, 0, 0), 1, false, 0},
        {Add::Years, at(9999, 1, 1, 0, 0, 0), 9998, false, 0},
        {Add::Years, at(9999, 1, 1, 0, 0, 0), 32767, false, 0},
        {Add::Years, at(9999, 1, 1, 0, 0, 0), -32768, false, 0},
        {Add::Years, maxTick, -1, true, 0x0460e75d3f8b3fffull},
        {Add::Years, maxTick, 0, true, 0x0461040bcb9f1fffull},
        {Add::Years, maxTick, 1, false, 0},
        {Add::Years, maxTick, 9998, false, 0},
        {Add::Years, maxTick, 32767, false, 0},
        {Add::Years, maxTick, -32768, false, 0},
        {Add::Years, maxTick + 1, -1, true, 0x0460e75d3f8b4000ull},
        {Add::Years, maxTick + 1, 0, false, 0},
        {Add::Years, maxTick + 1, 1, false, 0},
        {Add::Years, maxTick + 1, 9998, false, 0},
        {Add::Years, maxTick + 1, 32767, false, 0},
        {Add::Years, maxTick + 1, -32768, false, 0},
        {Add::Years, ~0ull, -1, false, 0},
        {Add::Years, ~0ull, 0, false, 0},
        {Add::Years, ~0ull, 1, false, 0},
        {Add::Years, ~0ull, 9998, true, 0x02121329797e3fffull},
        {Add::Years, ~0ull, 32767, false, 0},
        {Add::Years, ~0ull, -32768, false, 0},
    };
    for (const AddCase& entry : addCases) {
        RtcTick destination{addSentinel};
        const RtcTick origin{entry.source};
        Require(CallAdd(entry.function, &destination, &origin, entry.amount) == 0);
        Require(destination.tick == (entry.written ? entry.result : addSentinel));
        RtcTick inPlace{entry.source};
        Require(CallAdd(entry.function, &inPlace, &inPlace, entry.amount) == 0);
        Require(inPlace.tick == (entry.written ? entry.result : entry.source));
    }
    for (Add function : {Add::Ticks, Add::Microseconds, Add::Seconds, Add::Minutes, Add::Hours, Add::Days, Add::Weeks, Add::Months, Add::Years}) {
        RtcTick destination{addSentinel};
        const RtcTick origin{5};
        Require(CallAdd(function, nullptr, &origin, 1) == invalidPointer);
        Require(CallAdd(function, &destination, nullptr, 1) == invalidPointer && destination.tick == addSentinel);
        Require(CallAdd(function, nullptr, nullptr, 1) == invalidPointer);
    }

    RtcTick now{};
    Require(sceRtcGetCurrentTick(&now) == 0 && now.tick > leapDayTick);
    const RtcTick fixed{leapDayTick};
    RtcTick local{};
    RtcTick back{};
    Require(sceRtcConvertUtcToLocalTime(&fixed, &local) == 0);
    Require(sceRtcConvertLocalTimeToUtc(&local, &back) == 0 && back.tick == fixed.tick);

    char rfc2822[32];
    tick.tick = leapDayTick;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 0) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 12:34:56 +0000") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 90) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 14:04:56 +0130") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -300) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 07:34:56 -0500") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -30) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 12:04:56 -0030") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 65) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 13:39:56 +0105") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 1439) == 0 && std::strcmp(rfc2822, "Fri, 01 Mar 2024 12:33:56 +2359") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -1439) == 0 && std::strcmp(rfc2822, "Wed, 28 Feb 2024 12:35:56 -2359") == 0);
    const char* week[] = {"Sun, 03 Mar", "Mon, 04 Mar", "Tue, 05 Mar", "Wed, 06 Mar", "Thu, 07 Mar", "Fri, 08 Mar", "Sat, 09 Mar"};
    for (int day = 0; day < 7; ++day) {
        const RtcDateTime date{2024, 3, static_cast<std::uint16_t>(3 + day), 0, 0, 0, 0};
        Require(sceRtcGetTick(&date, &tick) == 0 && sceRtcFormatRFC2822(rfc2822, &tick, 0) == 0);
        Require(std::strncmp(rfc2822, week[day], std::strlen(week[day])) == 0);
    }
    tick.tick = 0;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 0) == 0 && std::strcmp(rfc2822, "Mon, 01 Jan 0001 00:00:00 +0000") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -1) == invalidValue);
    tick.tick = maxTick;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 0) == 0 && std::strcmp(rfc2822, "Fri, 31 Dec 9999 23:59:59 +0000") == 0);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 1) == invalidYear);
    tick.tick = maxTick + 1;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 0) == invalidYear);
    tick.tick = std::numeric_limits<std::uint64_t>::max();
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 0) == invalidValue);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 1) == invalidValue);
    tick.tick = 0ull - 60000000ull;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 1) == invalidValue);
    tick.tick = 1ull << 63;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, 0) == invalidValue);
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -1) == invalidYear);
    tick.tick = maxTick + 1;
    Require(sceRtcFormatRFC2822(rfc2822, &tick, -1) == 0 && std::strcmp(rfc2822, "Fri, 31 Dec 9999 23:59:00 -0001") == 0);
    tick.tick = leapDayTick;
    for (int offset : {1440, -1440, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        std::memset(rfc2822, 'x', sizeof(rfc2822));
        char original[sizeof(rfc2822)];
        std::memcpy(original, rfc2822, sizeof(rfc2822));
        Require(sceRtcFormatRFC2822(rfc2822, &tick, offset) == invalidValue);
        Require(std::memcmp(rfc2822, original, sizeof(rfc2822)) == 0);
    }
    Require(sceRtcFormatRFC2822(nullptr, &tick, 0) == invalidPointer);
    {
        RtcTick before{}, after{}, parsed{};
        Require(sceRtcGetCurrentTick(&before) == 0);
        Require(sceRtcFormatRFC2822(rfc2822, nullptr, 60) == 0);
        Require(sceRtcGetCurrentTick(&after) == 0);
        Require(std::strlen(rfc2822) == 31 && std::strcmp(rfc2822 + 26, "+0100") == 0);
        Require(sceRtcParseDateTime(&parsed, rfc2822) == 0 && parsed.tick + 1000000 > before.tick && parsed.tick <= after.tick);
        Require(sceRtcFormatRFC2822(rfc2822, nullptr, 1440) == invalidValue);
    }

    SetTimeZone("XXX-5:30");
    Require(sceRtcFormatRFC2822LocalTime(rfc2822, &tick) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 18:04:56 +0530") == 0);
    Require(sceRtcFormatRFC3339LocalTime(text, &tick) == 0 && std::strcmp(text, "2024-02-29T18:04:56.78+05:30") == 0);
    SetTimeZone("XXX+3");
    Require(sceRtcFormatRFC2822LocalTime(rfc2822, &tick) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 09:34:56 -0300") == 0);
    Require(sceRtcFormatRFC3339LocalTime(text, &tick) == 0 && std::strcmp(text, "2024-02-29T09:34:56.78-03:00") == 0);
    SetTimeZone("UTC0");
    Require(sceRtcFormatRFC2822LocalTime(rfc2822, &tick) == 0 && std::strcmp(rfc2822, "Thu, 29 Feb 2024 12:34:56 +0000") == 0);
    Require(sceRtcFormatRFC3339LocalTime(text, &tick) == 0 && std::strcmp(text, "2024-02-29T12:34:56.78Z") == 0);
    Require(sceRtcFormatRFC2822LocalTime(nullptr, &tick) == invalidPointer);
    Require(sceRtcFormatRFC3339LocalTime(nullptr, &tick) == invalidPointer);
    {
        RtcTick before{}, after{}, parsed{};
        Require(sceRtcGetCurrentTick(&before) == 0);
        Require(sceRtcFormatRFC2822LocalTime(rfc2822, nullptr) == 0);
        Require(sceRtcFormatRFC3339LocalTime(text, nullptr) == 0);
        Require(sceRtcGetCurrentTick(&after) == 0);
        Require(sceRtcParseDateTime(&parsed, rfc2822) == 0 && parsed.tick + 1000000 > before.tick && parsed.tick <= after.tick);
        Require(sceRtcParseRFC3339(&parsed, text) == 0 && parsed.tick + 10000 > before.tick && parsed.tick <= after.tick);
    }
    for (const char* zone : {"UTC0", "XXX-5:30", "XXX+3"}) {
        SetTimeZone(zone);
        for (std::uint64_t invalidTick : {maxTick + 1, std::uint64_t{1} << 63u, std::numeric_limits<std::uint64_t>::max()}) {
            source.tick = invalidTick;
            result.tick = 123;
            Require(sceRtcConvertUtcToLocalTime(&source, &result) == invalidValue && result.tick == 123);
            Require(sceRtcConvertLocalTimeToUtc(&source, &result) == invalidValue && result.tick == 123);
            Require(sceRtcConvertUtcToLocalTime(&source, &source) == invalidValue && source.tick == invalidTick);
            Require(sceRtcConvertLocalTimeToUtc(&source, &source) == invalidValue && source.tick == invalidTick);
            std::memset(text, 'x', sizeof(text));
            char originalText[sizeof(text)];
            std::memcpy(originalText, text, sizeof(text));
            Require(sceRtcFormatRFC3339LocalTime(text, &source) == invalidValue);
            Require(std::memcmp(text, originalText, sizeof(text)) == 0);
            std::memset(rfc2822, 'x', sizeof(rfc2822));
            char originalRfc2822[sizeof(rfc2822)];
            std::memcpy(originalRfc2822, rfc2822, sizeof(rfc2822));
            Require(sceRtcFormatRFC2822LocalTime(rfc2822, &source) == invalidValue);
            Require(std::memcmp(rfc2822, originalRfc2822, sizeof(rfc2822)) == 0);
        }
        source.tick = leapDayTick;
        Require(sceRtcConvertUtcToLocalTime(&source, &result) == 0);
        Require(sceRtcConvertLocalTimeToUtc(&result, &result) == 0 && result.tick == source.tick);
    }
    SetTimeZone("UTC0");

#ifndef _WIN32
    SetTimeZone("STD0DST,J1/0,J2/0");
    const std::time_t beforeTransition = -1;
    const std::time_t atTransition = 0;
    std::tm beforeState{}, afterState{};
    Require(localtime_r(&beforeTransition, &beforeState) != nullptr && beforeState.tm_isdst == 0);
    Require(localtime_r(&atTransition, &afterState) != nullptr && afterState.tm_isdst == 1);
    for (const std::uint64_t distance : {1000000ull, 500000ull, 1ull}) {
        source.tick = unixEpochTick - distance;
        Require(sceRtcConvertUtcToLocalTime(&source, &result) == 0 && result.tick == source.tick);
        Require(sceRtcConvertUtcToLocalTime(&source, &source) == 0 && source.tick == unixEpochTick - distance);
    }
    for (const std::uint64_t distance : {0ull, 500000ull}) {
        source.tick = unixEpochTick + distance;
        Require(sceRtcConvertUtcToLocalTime(&source, &result) == 0 && result.tick == source.tick + 3600000000ull);
    }
    tick.tick = unixEpochTick - 500000ull;
    Require(sceRtcFormatRFC3339LocalTime(text, &tick) == 0 && std::strcmp(text, "1969-12-31T23:59:59.50Z") == 0);
    Require(sceRtcFormatRFC2822LocalTime(rfc2822, &tick) == 0 && std::strcmp(rfc2822, "Wed, 31 Dec 1969 23:59:59 +0000") == 0);
    SetTimeZone("UTC0");
#endif

    {
        char precise[40];
        RtcTick first{leapDayTick}, second{leapDayTick + 1};
        Require(sceRtcFormatRFC3339Precise(precise, &first, 0, 0) == 0 && std::strcmp(precise, "2024-02-29T12:34:56Z") == 0);
        Require(sceRtcFormatRFC3339Precise(precise, &first, 0, 3) == 0 && std::strcmp(precise, "2024-02-29T12:34:56.789Z") == 0);
        Require(sceRtcFormatRFC3339Precise(precise, &first, 0, 6) == 0 && std::strcmp(precise, "2024-02-29T12:34:56.789000Z") == 0);
        Require(sceRtcFormatRFC3339Precise(precise, &second, 90, 6) == 0 && std::strcmp(precise, "2024-02-29T14:04:56.789001+01:30") == 0);
        Require(sceRtcFormatRFC3339Precise(precise, &first, -300, 1) == 0 && std::strcmp(precise, "2024-02-29T07:34:56.7-05:00") == 0);
        Require(sceRtcFormatRFC3339Precise(precise, &first, 0, 7) == invalidValue);
        Require(sceRtcFormatRFC3339Precise(precise, &first, 0, -1) == invalidValue);
        Require(sceRtcFormatRFC3339Precise(precise, &first, 1440, 0) == invalidValue);
        Require(sceRtcFormatRFC3339Precise(precise, &first, -1440, 0) == invalidValue);
        Require(sceRtcFormatRFC3339Precise(nullptr, &first, 0, 0) == invalidPointer);
        Require(sceRtcFormatRFC3339Precise(precise, nullptr, 0, 6) == 0 && precise[4] == '-');
        SetTimeZone("UTC0");
        Require(sceRtcFormatRFC3339PreciseLocalTime(precise, &first, 2) == 0 && std::strcmp(precise, "2024-02-29T12:34:56.78Z") == 0);
        Require(sceRtcFormatRFC3339PreciseLocalTime(precise, &first, 7) == invalidValue);
        Require(sceRtcFormatRFC3339PreciseLocalTime(nullptr, &first, 0) == invalidPointer);
        Require(sceRtcCompareTick(&first, &second) == -1 && sceRtcCompareTick(&second, &first) == 1 && sceRtcCompareTick(&first, &first) == 0);
        Require(sceRtcCompareTick(nullptr, &first) == invalidPointer && sceRtcCompareTick(&first, nullptr) == invalidPointer);
        RtcTick current{};
        Require(sceRtcGetCurrentAdNetworkTick(&current) == 0 && current.tick > unixEpochTick);
        Require(sceRtcGetCurrentAdNetworkTick(nullptr) == invalidPointer);
        RtcTick tooEarly{unixEpochTick - 1};
        Require(sceRtcSetCurrentTick(&tooEarly) == invalidValue && sceRtcSetCurrentTick(nullptr) == invalidPointer);
        const RtcTick systemClock{leapDayTick};
        const RtcTick networkClock{leapDayTick + 10000000};
        const RtcTick adClock{leapDayTick + 20000000};
        const RtcTick debugClock{leapDayTick + 30000000};
        Require(sceRtcSetCurrentTick(&systemClock) == 0);
        Require(sceRtcSetCurrentNetworkTick(&networkClock) == 0);
        Require(sceRtcGetCurrentAdNetworkTick(&current) == 0 && current.tick >= networkClock.tick && current.tick < networkClock.tick + 1000000);
        Require(sceRtcGetCurrentDebugNetworkTick(&current) == 0 && current.tick >= networkClock.tick && current.tick < networkClock.tick + 1000000);
        Require(sceRtcSetCurrentAdNetworkTick(&adClock) == 0);
        Require(sceRtcSetCurrentDebugNetworkTick(&debugClock) == 0);
        Require(sceRtcSetCurrentNetworkTick(&tooEarly) == invalidValue && sceRtcSetCurrentNetworkTick(nullptr) == invalidPointer);
        Require(sceRtcSetCurrentAdNetworkTick(&tooEarly) == invalidValue && sceRtcSetCurrentAdNetworkTick(nullptr) == invalidPointer);
        Require(sceRtcSetCurrentDebugNetworkTick(&tooEarly) == invalidValue && sceRtcSetCurrentDebugNetworkTick(nullptr) == invalidPointer);
        Require(sceRtcGetCurrentTick(&current) == 0 && current.tick >= systemClock.tick && current.tick < systemClock.tick + 1000000);
        Require(sceRtcGetCurrentNetworkTick(&current) == 0 && current.tick >= networkClock.tick && current.tick < networkClock.tick + 1000000);
        Require(sceRtcGetCurrentAdNetworkTick(&current) == 0 && current.tick >= adClock.tick && current.tick < adClock.tick + 1000000);
        Require(sceRtcGetCurrentDebugNetworkTick(&current) == 0 && current.tick >= debugClock.tick && current.tick < debugClock.tick + 1000000);
        Require(sceRtcGetCurrentRawNetworkTick(&current) == 0 && current.tick > unixEpochTick);
    }
    SetCompiledSdkVersion(process.data(), 0x02ffffff);
    Require(sceRtcGetDayOfWeek(0, 13, 1) == invalidYear && sceRtcGetDayOfWeek(10000, 1, 1) == 6);
    SetCompiledSdkVersion(process.data(), 0x03000000);
    Require(sceRtcGetDayOfWeek(0, 13, 1) == invalidMonth && sceRtcGetDayOfWeek(10000, 1, 1) == invalidYear);
    SetCompiledSdkVersion(process.data(), 0x05050001);
    Require(sceRtcGetDayOfWeek(0, 13, 1) == invalidMonth && sceRtcGetDayOfWeek(10000, 13, 1) == invalidMonth && sceRtcGetDayOfWeek(2024, 0, 1) == invalidMonth);
    Require(sceRtcGetDayOfWeek(0, 1, 0) == invalidYear && sceRtcGetDayOfWeek(10000, 2, 30) == invalidYear && sceRtcGetDayOfWeek(65536, 1, 1) == invalidYear);
    Require(sceRtcGetDayOfWeek(2024, 2, 30) == invalidDay && sceRtcGetDayOfWeek(9999, 12, 31) == 5 && sceRtcGetDayOfWeek(1, 1, 1) == 1);
    Require(sceRtcGetDaysInMonth(0, 13) == invalidYear && sceRtcGetDaysInMonth(10000, 1) == 31 && sceRtcIsLeapYear(10000) == 1);
    RtcDateTime sentinel{0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaaaaaau};
    RtcDateTime ranged = sentinel;
    Require(sceRtcSetTime_t(&ranged, -1) == invalidValue && Equal(ranged, sentinel));
    Require(sceRtcSetTime_t(&ranged, std::numeric_limits<std::int64_t>::min()) == invalidValue && Equal(ranged, sentinel));
    Require(sceRtcSetTime_t(&ranged, 4294967296ll) == 0 && Equal(ranged, RtcDateTime{2106, 2, 7, 6, 28, 16, 0}));
    Require(sceRtcSetTime_t(&ranged, 253402300800ll) == 0 && Equal(ranged, RtcDateTime{10000, 1, 1, 0, 0, 0, 0}));
    Require(sceRtcSetTime_t(&ranged, 18446744073709ll) == 0 && Equal(ranged, RtcDateTime{1969, 12, 31, 23, 59, 59, 448384}));
    Require(sceRtcSetTime_t(&ranged, std::numeric_limits<std::int64_t>::max()) == 0 && Equal(ranged, RtcDateTime{1969, 12, 31, 23, 59, 59, 0}));
    char ranges[64];
    RtcTick shifted{0};
    Require(sceRtcFormatRFC3339(ranges, &shifted, -1) == invalidValue);
    shifted.tick = 59999999;
    Require(sceRtcFormatRFC3339(ranges, &shifted, -1) == invalidValue);
    shifted.tick = 60000000;
    Require(sceRtcFormatRFC3339(ranges, &shifted, -1) == 0 && std::strcmp(ranges, "0001-01-01T00:00:00.00-00:01") == 0);
    shifted.tick = std::numeric_limits<std::uint64_t>::max();
    Require(sceRtcFormatRFC3339(ranges, &shifted, 1) == invalidValue && sceRtcFormatRFC3339(ranges, &shifted, 1439) == invalidValue);
    shifted.tick = std::numeric_limits<std::uint64_t>::max() - 60000000 + 1;
    Require(sceRtcFormatRFC3339(ranges, &shifted, 1) == invalidValue);
    shifted.tick = std::numeric_limits<std::uint64_t>::max() - 60000000;
    Require(sceRtcFormatRFC3339(ranges, &shifted, 1) == invalidYear);
    shifted.tick = 1ull << 63;
    Require(sceRtcFormatRFC3339(ranges, &shifted, 0) == invalidYear && sceRtcFormatRFC3339(ranges, &shifted, -1) == invalidYear);
    shifted.tick = maxTick + 1;
    Require(sceRtcFormatRFC3339(ranges, &shifted, -1) == 0 && std::strcmp(ranges, "9999-12-31T23:59:00.00-00:01") == 0);
    Require(sceRtcInit() == 0);
    Require(sceRtcEnd() == 0);
}
