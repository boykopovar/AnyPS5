#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udat_getSymbols_nid_postfix),
                             std::int32_t (APS5_VABI*)(const void*, std::int32_t, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU date format symbols check failed at line %u\n", location.line());
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

void NullFormatterReturnsMinusOne() {
    constexpr std::array<char16_t, 4> initial{u'A', u'B', u'C', u'D'};
    for (std::int32_t type : {0, 1, 2, 7, 27, -1, 100, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        for (std::int32_t index : {0, 1, -1, 12, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            for (std::int32_t length : {0, 4, -1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
                for (std::int32_t incoming : {0, -124, -1, 1, 7, 15, 16}) {
                    auto buffer = initial;
                    auto status = incoming;
                    Require(udat_getSymbols_nid_postfix(nullptr, type, index, buffer.data(), length, &status) == -1);
                    Require(status == incoming);
                    Require(buffer == initial);

                    status = incoming;
                    Require(udat_getSymbols_nid_postfix(nullptr, type, index, nullptr, length, &status) == -1);
                    Require(status == incoming);
                }
                auto buffer = initial;
                Require(udat_getSymbols_nid_postfix(nullptr, type, index, buffer.data(), length, nullptr) == -1);
                Require(buffer == initial);
                Require(udat_getSymbols_nid_postfix(nullptr, type, index, nullptr, length, nullptr) == -1);
            }
        }
    }
}

void NonNullFormatterThrows() {
    alignas(8) std::array<std::uint8_t, 64> storage{};
    std::array<char16_t, 4> buffer{u'X', u'Y', u'Z', u'W'};
    for (std::int32_t incoming : {0, -124, 1, 15}) {
        auto status = incoming;
        RequireThrows([&] { udat_getSymbols_nid_postfix(storage.data(), 0, 0, buffer.data(), 4, &status); });
        Require(status == incoming);
        Require(buffer[0] == u'X' && buffer[1] == u'Y' && buffer[2] == u'Z' && buffer[3] == u'W');
    }
    RequireThrows([&] { udat_getSymbols_nid_postfix(storage.data(), 7, -1, nullptr, 0, nullptr); });
}

}

int main() {
    NullFormatterReturnsMinusOne();
    NonNullFormatterThrows();
    std::puts("ICU date format symbols tests passed");
}
