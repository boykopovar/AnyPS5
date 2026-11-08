#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udat_open_nid_postfix),
                             UDateFormat* (APS5_VABI*)(std::int32_t, std::int32_t, const char*, const char16_t*, std::int32_t,
                                                       const char16_t*, std::int32_t, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU date format open check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

void FailureShortCircuit() {
    const char* badLocale = reinterpret_cast<const char*>(std::uintptr_t{1});
    const char16_t* badTz = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const char16_t* badPattern = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    for (std::int32_t incoming : {1, 2, 7, 15, 16, 65536, std::numeric_limits<std::int32_t>::max()}) {
        for (std::int32_t timeStyle : {-2, -1, 0, 1, 2, 3, 128}) {
            for (std::int32_t dateStyle : {-2, -1, 0, 1, 2, 3, 128}) {
                std::int32_t status = incoming;
                Require(udat_open_nid_postfix(timeStyle, dateStyle, nullptr, nullptr, 0, nullptr, 0, &status) == nullptr);
                Require(status == incoming);
                Require(udat_open_nid_postfix(timeStyle, dateStyle, badLocale, badTz, -5, badPattern, -9, &status) == nullptr);
                Require(status == incoming);
            }
        }
    }
}

void NonFailureThrows() {
    const char16_t zone[] = u"UTC";
    const char16_t pattern[] = u"yyyy-MM-dd";
    RequireThrows<std::invalid_argument>([&] {
        udat_open_nid_postfix(0, 0, "en_US", zone, 3, pattern, 10, nullptr);
    });
    for (std::int32_t incoming : {std::numeric_limits<std::int32_t>::min(), -128, -124, -1, 0}) {
        for (std::int32_t timeStyle : {-2, 0, 2}) {
            std::int32_t status = incoming;
            RequireThrows<std::exception>([&] {
                udat_open_nid_postfix(timeStyle, 0, "en_US", zone, 3, pattern, 10, &status);
            });
            Require(status == incoming);
            RequireThrows<std::exception>([&] {
                udat_open_nid_postfix(timeStyle, 0, nullptr, nullptr, -1, nullptr, -1, &status);
            });
            Require(status == incoming);
        }
    }
}

} // namespace

int main() {
    FailureShortCircuit();
    NonFailureThrows();
    std::puts("ICU date format open tests passed");
}
