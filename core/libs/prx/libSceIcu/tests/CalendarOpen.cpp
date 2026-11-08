#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucal_open_nid_postfix),
                             UCalendar* (APS5_VABI*)(const char16_t*, std::int32_t, const char*, std::int32_t, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU calendar open check failed at line %u\n", location.line());
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

void FailingStatusShortCircuits() {
    const char16_t zoneId[] = u"UTC";
    const auto* unmappedZone = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const auto* unmappedLocale = reinterpret_cast<const char*>(std::uintptr_t{1});
    for (const std::int32_t incoming : {1, 2, 7, 15, 16, 0x10000, std::numeric_limits<std::int32_t>::max()}) {
        std::int32_t status = incoming;
        Require(ucal_open_nid_postfix(nullptr, -1, nullptr, 0, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(ucal_open_nid_postfix(zoneId, 3, "en_US", 1, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(ucal_open_nid_postfix(unmappedZone, -99, unmappedLocale, -1, &status) == nullptr);
        Require(status == incoming);
    }
}

void UnsupportedPathsThrow() {
    const char16_t zoneId[] = u"UTC";
    RequireThrows([&] { ucal_open_nid_postfix(nullptr, -1, nullptr, 0, nullptr); });
    RequireThrows([&] { ucal_open_nid_postfix(zoneId, 3, "en_US", 1, nullptr); });

    for (const std::int32_t incoming : {0, -1, -124, -128, std::numeric_limits<std::int32_t>::min()}) {
        std::int32_t status = incoming;
        RequireThrows([&] { ucal_open_nid_postfix(nullptr, -1, nullptr, 0, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { ucal_open_nid_postfix(zoneId, 3, "en_US", 1, &status); });
        Require(status == incoming);
    }
}

} // namespace

int main() {
    FailingStatusShortCircuits();
    UnsupportedPathsThrow();
    std::puts("ICU calendar open tests passed");
}
