#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucal_getKeywordValuesForLocale_nid_postfix),
                             UEnumeration* (APS5_VABI*)(const char*, const char*, std::int8_t, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU calendar keyword values check failed at line %u\n", location.line());
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

}

int main() {
    const auto invalidKey = reinterpret_cast<const char*>(std::uintptr_t{1});
    const auto invalidLocale = reinterpret_cast<const char*>(std::uintptr_t{2});
    for (std::int32_t incoming : {1, 2, 7, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        for (std::int8_t commonlyUsed : {std::int8_t{0}, std::int8_t{1}, std::int8_t{-1}, std::numeric_limits<std::int8_t>::min(),
                                         std::numeric_limits<std::int8_t>::max()}) {
            std::int32_t status = incoming;
            Require(ucal_getKeywordValuesForLocale_nid_postfix("calendar", "en_US", commonlyUsed, &status) == nullptr);
            Require(status == incoming);
            Require(ucal_getKeywordValuesForLocale_nid_postfix(nullptr, nullptr, commonlyUsed, &status) == nullptr);
            Require(status == incoming);
            Require(ucal_getKeywordValuesForLocale_nid_postfix(invalidKey, invalidLocale, commonlyUsed, &status) == nullptr);
            Require(status == incoming);
        }
    }

    RequireThrows([&] { ucal_getKeywordValuesForLocale_nid_postfix("calendar", "en_US", 0, nullptr); });
    RequireThrows([&] { ucal_getKeywordValuesForLocale_nid_postfix(nullptr, nullptr, 1, nullptr); });

    std::puts("ICU calendar keyword values tests passed");
}
