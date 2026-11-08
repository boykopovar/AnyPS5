#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucal_get_nid_postfix), std::int32_t (APS5_VABI*)(const UCalendar*, UCalendarDateFields, std::int32_t*)>);

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "ICU calendar get assertion failed at line %u\n", location.line());
    std::abort();
}

} // namespace

int main() {
    constexpr std::array<std::int32_t, 5> failingStatuses{1, 2, 10, 15, 16};
    constexpr std::array<UCalendarDateFields, 6> sampleFields{0, 1, 2, 5, 10, 23};

    for (const auto failingStatus : failingStatuses) {
        for (const auto field : sampleFields) {
            std::int32_t status = failingStatus;
            const auto result = ucal_get_nid_postfix(nullptr, field, &status);
            Require(result == -1);
            Require(status == failingStatus);

            const auto* dummyCalendar = reinterpret_cast<const UCalendar*>(0x12345678);
            std::int32_t statusWithDummy = failingStatus;
            const auto resultWithDummy = ucal_get_nid_postfix(dummyCalendar, field, &statusWithDummy);
            Require(resultWithDummy == -1);
            Require(statusWithDummy == failingStatus);
        }
    }
}
