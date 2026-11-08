#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udtitvfmt_formatCalendarToResult_nid_postfix),
                             void (APS5_VABI*)(const UDateIntervalFormat*, UCalendar*, UCalendar*, UFormattedDateInterval*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU date interval format calendar-to-result check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false);
}

} // namespace

int main() {
    std::int32_t formatterStorage = 0x11111111;
    std::int32_t fromStorage = 0x22222222;
    std::int32_t toStorage = 0x33333333;
    std::int32_t resultStorage = 0x46444956;
    const auto* formatter = reinterpret_cast<const UDateIntervalFormat*>(&formatterStorage);
    auto* fromCalendar = reinterpret_cast<UCalendar*>(&fromStorage);
    auto* toCalendar = reinterpret_cast<UCalendar*>(&toStorage);
    auto* result = reinterpret_cast<UFormattedDateInterval*>(&resultStorage);

    for (const std::int32_t incoming : {1, 2, 3, 7, 15, 16, 27, 0x7fffffff}) {
        std::int32_t status = incoming;
        udtitvfmt_formatCalendarToResult_nid_postfix(nullptr, nullptr, nullptr, nullptr, &status);
        Require(status == incoming);

        status = incoming;
        udtitvfmt_formatCalendarToResult_nid_postfix(formatter, fromCalendar, toCalendar, result, &status);
        Require(status == incoming);
        Require(formatterStorage == 0x11111111 && fromStorage == 0x22222222 && toStorage == 0x33333333 && resultStorage == 0x46444956);
    }

    for (const std::int32_t incoming : {0, -128, -124, -1}) {
        std::int32_t status = incoming;
        udtitvfmt_formatCalendarToResult_nid_postfix(nullptr, nullptr, nullptr, nullptr, &status);
        Require(status == IllegalArgument);

        status = incoming;
        udtitvfmt_formatCalendarToResult_nid_postfix(formatter, fromCalendar, toCalendar, nullptr, &status);
        Require(status == IllegalArgument);
        Require(formatterStorage == 0x11111111 && fromStorage == 0x22222222 && toStorage == 0x33333333);
    }

    RequireThrows([&] { udtitvfmt_formatCalendarToResult_nid_postfix(nullptr, nullptr, nullptr, nullptr, nullptr); });
    RequireThrows([&] { udtitvfmt_formatCalendarToResult_nid_postfix(formatter, fromCalendar, toCalendar, result, nullptr); });

    std::puts("ICU date interval format calendar-to-result tests passed");
}
