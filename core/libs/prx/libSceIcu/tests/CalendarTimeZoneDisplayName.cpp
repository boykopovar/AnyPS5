#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucal_getTimeZoneDisplayName_nid_postfix),
                             std::int32_t (APS5_VABI*)(const void*, std::int32_t, const char*, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU calendar time zone display name check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, location);
}

void FailureShortCircuit() {
    int opaqueCalendar = 0;
    for (std::int32_t incoming : {1, 2, 7, 10, 15, 16, 27, std::numeric_limits<std::int32_t>::max()}) {
        std::array<char16_t, 4> buffer{u'A', u'B', u'C', u'D'};
        std::int32_t status = incoming;
        Require(ucal_getTimeZoneDisplayName_nid_postfix(nullptr, 0, nullptr, buffer.data(), 4, &status) == -1);
        Require(status == incoming);
        Require(buffer == std::array<char16_t, 4>{u'A', u'B', u'C', u'D'});

        status = incoming;
        Require(ucal_getTimeZoneDisplayName_nid_postfix(&opaqueCalendar, 1, "en_US", buffer.data(), 0, &status) == -1);
        Require(status == incoming);
        Require(buffer == std::array<char16_t, 4>{u'A', u'B', u'C', u'D'});

        status = incoming;
        Require(ucal_getTimeZoneDisplayName_nid_postfix(&opaqueCalendar, -99, "fr_FR", nullptr, -5, &status) == -1);
        Require(status == incoming);
    }
}

void NonFailurePathsThrow() {
    int opaqueCalendar = 0;
    std::array<char16_t, 4> buffer{u'X', u'Y', u'Z', u'W'};
    RequireThrows([&] {
        ucal_getTimeZoneDisplayName_nid_postfix(&opaqueCalendar, 0, "en_US", buffer.data(), 4, nullptr);
    });
    Require(buffer == std::array<char16_t, 4>{u'X', u'Y', u'Z', u'W'});

    for (std::int32_t incoming : {0, -128, -124, -1, std::numeric_limits<std::int32_t>::min()}) {
        std::int32_t status = incoming;
        RequireThrows([&] {
            ucal_getTimeZoneDisplayName_nid_postfix(&opaqueCalendar, 0, "en_US", buffer.data(), 4, &status);
        });
        Require(status == incoming);
        Require(buffer == std::array<char16_t, 4>{u'X', u'Y', u'Z', u'W'});

        status = incoming;
        RequireThrows([&] {
            ucal_getTimeZoneDisplayName_nid_postfix(nullptr, 2, nullptr, nullptr, 0, &status);
        });
        Require(status == incoming);
    }
}

} // namespace

int main() {
    FailureShortCircuit();
    NonFailurePathsThrow();
    std::puts("ICU calendar time zone display name tests passed");
}
