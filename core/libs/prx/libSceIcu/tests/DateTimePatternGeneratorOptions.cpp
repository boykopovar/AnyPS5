#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<
    decltype(&udatpg_getBestPatternWithOptions_nid_postfix),
    std::int32_t (APS5_VABI*)(
        UDateTimePatternGenerator*,
        const char16_t*,
        std::int32_t,
        UDateTimePatternMatchOptions,
        char16_t*,
        std::int32_t,
        std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU DateTimePatternGeneratorOptions check failed at line %u\n", location.line());
    std::abort();
}

void TestIncomingFailure() {
    constexpr char16_t Fill = 0xa55a;
    for (std::int32_t incoming : {1, 7, 15, 16}) {
        std::int32_t status = incoming;
        std::array<char16_t, 8> buffer;
        buffer.fill(Fill);
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, 0, UDATPG_MATCH_NO_OPTIONS, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == incoming);
        for (char16_t unit : buffer) {
            Require(unit == Fill);
        }
    }

    for (std::int32_t incoming : {1, 7, 15, 16}) {
        std::int32_t status = incoming;
        std::array<char16_t, 8> buffer;
        buffer.fill(Fill);
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, 10, UDATPG_MATCH_NO_OPTIONS, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == incoming);
        for (char16_t unit : buffer) {
            Require(unit == Fill);
        }
    }
}

void TestNullSkeletonNonzeroLength() {
    constexpr char16_t Fill = 0xa55a;
    constexpr std::int32_t IllegalArgument = 1;

    for (std::int32_t length : {1, 2, 100, -1, -5}) {
        std::int32_t status = 0;
        std::array<char16_t, 8> buffer;
        buffer.fill(Fill);
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, length, UDATPG_MATCH_NO_OPTIONS, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == IllegalArgument);
        for (char16_t unit : buffer) {
            Require(unit == Fill);
        }
    }

    {
        std::int32_t status = -124;
        std::array<char16_t, 8> buffer;
        buffer.fill(Fill);
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, 4, UDATPG_MATCH_NO_OPTIONS, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == IllegalArgument);
        for (char16_t unit : buffer) {
            Require(unit == Fill);
        }
    }

    {
        std::int32_t status = 0;
        const auto result = udatpg_getBestPatternWithOptions_nid_postfix(
            nullptr, nullptr, 1, UDATPG_MATCH_NO_OPTIONS, nullptr, 0, &status);
        Require(result == 0);
        Require(status == IllegalArgument);
    }
}

}

int main() {
    TestIncomingFailure();
    TestNullSkeletonNonzeroLength();
    std::puts("ICU DateTimePatternGeneratorOptions tests passed");
}
