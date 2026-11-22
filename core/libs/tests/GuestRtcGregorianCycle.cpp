#include "prx/libSceRtc/include/Rtc.hpp"
#include "ProcessParametersFixture.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

int main() {
    std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    const std::uint32_t sdk = 0x01000051;
    std::memcpy(process.data() + 16, &sdk, sizeof(sdk));
    using namespace std::chrono;
    const sys_days first = year{2000}/January/1;
    const sys_days last = year{2400}/January/1;
    for (sys_days current = first; current < last; current += days{1}) {
        const year_month_day date{current};
        const auto y = static_cast<int>(date.year());
        const auto m = static_cast<unsigned>(date.month());
        const auto d = static_cast<unsigned>(date.day());
        Require(sceRtcGetDayOfWeek(y, m, d) == static_cast<int>(weekday{current}.c_encoding()));
        Require(sceRtcIsLeapYear(y) == date.year().is_leap());
        RtcDateTime input{static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(m), static_cast<std::uint16_t>(d), 12, 34, 56, 789123};
        RtcTick tick{};
        const auto elapsed = duration_cast<microseconds>(current.time_since_epoch()).count();
        const auto expected = 62135596800000000ull + elapsed + 45296789123ull;
        Require(sceRtcGetTick(&input, &tick) == 0 && tick.tick == expected);
        RtcDateTime result{};
        Require(sceRtcSetTick(&result, &tick) == 0);
        Require(result.year == input.year && result.month == input.month && result.day == input.day &&
                result.hour == input.hour && result.minute == input.minute && result.second == input.second &&
                result.microsecond == input.microsecond);
    }
}
