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

static_assert(std::is_same_v<decltype(&udat_format_nid_postfix),
                             std::int32_t (APS5_VABI*)(const UDateFormat*, double, char16_t*, std::int32_t, UFieldPosition*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr char16_t Fill = 0xa55a;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU date format check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

void FailureStatus() {
    std::array<char16_t, 4> buffer;
    buffer.fill(Fill);
    auto* format = reinterpret_cast<const UDateFormat*>(&buffer);
    auto* position = reinterpret_cast<UFieldPosition*>(&buffer);

    for (std::int32_t incoming : {1, 2, 7, 10, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        std::int32_t status = incoming;
        Require(udat_format_nid_postfix(nullptr, 0.0, nullptr, 0, nullptr, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(udat_format_nid_postfix(format, 1700000000000.0, buffer.data(), 4, position, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(udat_format_nid_postfix(nullptr, -1.0, nullptr, 1, position, &status) == -1);
        Require(status == incoming);

        status = incoming;
        Require(udat_format_nid_postfix(format, 0.0, buffer.data(), -1, nullptr, &status) == -1);
        Require(status == incoming);
    }
    for (char16_t unit : buffer) Require(unit == Fill);
}

void InvalidBufferArguments() {
    std::array<char16_t, 4> buffer;
    buffer.fill(Fill);
    auto* format = reinterpret_cast<const UDateFormat*>(&buffer);
    auto* position = reinterpret_cast<UFieldPosition*>(&buffer);

    for (std::int32_t incoming : {0, -1, -124, -128, std::numeric_limits<std::int32_t>::min()}) {
        for (std::int32_t badLength : {1, 2, 64, -1, -5, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            std::int32_t status = incoming;
            Require(udat_format_nid_postfix(nullptr, 0.0, nullptr, badLength, nullptr, &status) == -1);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udat_format_nid_postfix(format, 123456.78, nullptr, badLength, position, &status) == -1);
            Require(status == IllegalArgument);
        }
        for (std::int32_t negativeLength : {-1, -2, -100, std::numeric_limits<std::int32_t>::min()}) {
            std::int32_t status = incoming;
            Require(udat_format_nid_postfix(nullptr, 0.0, buffer.data(), negativeLength, nullptr, &status) == -1);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udat_format_nid_postfix(format, -98765.0, buffer.data(), negativeLength, position, &status) == -1);
            Require(status == IllegalArgument);
        }
    }
    for (char16_t unit : buffer) Require(unit == Fill);
}

void UnsupportedPathsThrow() {
    std::array<char16_t, 4> buffer;
    buffer.fill(Fill);
    auto* format = reinterpret_cast<const UDateFormat*>(&buffer);
    auto* position = reinterpret_cast<UFieldPosition*>(&buffer);

    RequireThrows([&] { udat_format_nid_postfix(nullptr, 0.0, nullptr, 0, nullptr, nullptr); });
    RequireThrows([&] { udat_format_nid_postfix(format, 0.0, buffer.data(), -1, position, nullptr); });

    for (std::int32_t incoming : {0, -1, -124}) {
        std::int32_t status = incoming;
        RequireThrows([&] { udat_format_nid_postfix(format, 0.0, nullptr, 0, nullptr, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { udat_format_nid_postfix(format, 0.0, buffer.data(), 0, position, &status); });
        Require(status == incoming);

        status = incoming;
        RequireThrows([&] { udat_format_nid_postfix(format, 0.0, buffer.data(), 4, nullptr, &status); });
        Require(status == incoming);
    }
    for (char16_t unit : buffer) Require(unit == Fill);
}

} // namespace

int main() {
    FailureStatus();
    InvalidBufferArguments();
    UnsupportedPathsThrow();
    std::puts("ICU date format tests passed");
}
