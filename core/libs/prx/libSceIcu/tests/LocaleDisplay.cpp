#include "prx/libSceIcu/DefaultLocaleName.hpp"
#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/uloc.h"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uloc_getDisplayScript_nid_postfix),
    std::int32_t (APS5_VABI*)(const char*, const char*, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr char16_t Sentinel = 0x5a5a;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "ICU locale display check failed at line %u\n", location.line());
    std::abort();
}

struct Outcome {
    std::int32_t length;
    std::int32_t status;
    std::array<char16_t, 64> output;
};

Outcome CallNative(const char* locale, const char* displayLocale, std::int32_t capacity, std::int32_t incoming) {
    Outcome result{};
    result.status = incoming;
    result.output.fill(Sentinel);
    const auto* resolvedLocale = locale ? locale : Icu::DefaultLocaleName();
    const auto* resolvedDisplay = displayLocale ? displayLocale : Icu::DefaultLocaleName();
    auto status = static_cast<UErrorCode>(incoming);
    result.length = uloc_getDisplayScript(
        resolvedLocale,
        resolvedDisplay,
        capacity > 0 ? result.output.data() : nullptr,
        capacity,
        &status);
    result.status = status;
    return result;
}

Outcome CallWrapper(const char* locale, const char* displayLocale, std::int32_t capacity, std::int32_t incoming) {
    Outcome result{};
    result.status = incoming;
    result.output.fill(Sentinel);
    result.length = uloc_getDisplayScript_nid_postfix(
        locale,
        displayLocale,
        capacity > 0 ? result.output.data() : nullptr,
        capacity,
        &result.status);
    return result;
}

void CheckCase(const char* locale, const char* displayLocale, std::int32_t capacity, std::int32_t incoming = 0) {
    const auto expected = CallNative(locale, displayLocale, capacity, incoming);
    const auto actual = CallWrapper(locale, displayLocale, capacity, incoming);
    Require(actual.length == expected.length);
    Require(actual.status == expected.status);
    Require(actual.output == expected.output);
}

void CheckBehavior() {
    constexpr const char* locales[] = {"sr_Latn", "zh_Hant_TW", "az_Cyrl", "en_US", "x-klingon"};
    constexpr const char* displays[] = {"en", "fr", "de"};
    constexpr std::int32_t capacities[] = {0, 1, 2, 4, 8, 16, 63};
    for (const auto* locale : locales)
        for (const auto* display : displays)
            for (const auto capacity : capacities)
                CheckCase(locale, display, capacity);
    for (const auto capacity : capacities) {
        CheckCase(nullptr, "en", capacity);
        CheckCase("sr_Latn", nullptr, capacity);
        CheckCase(nullptr, nullptr, capacity);
    }
    CheckCase("sr_Latn", "en", -1);
    CheckCase("sr_Latn", "en", 8, -127);
    CheckCase("sr_Latn", "en", 8, 1);
}

template <typename Action>
void RequireInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

void CheckGuestPointers() {
    std::array<char16_t, 16> output{};
    std::int32_t status = 0;
    Require(uloc_getDisplayScript_nid_postfix("sr_Latn", "en", nullptr, 16, &status) == 0);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR);
    status = 0;
    Require(uloc_getDisplayScript_nid_postfix(
        reinterpret_cast<const char*>(0x10),
        reinterpret_cast<const char*>(0x10),
        nullptr,
        16,
        &status) == 0);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR);
    status = 0;
    Require(uloc_getDisplayScript_nid_postfix(
        reinterpret_cast<const char*>(0x10),
        reinterpret_cast<const char*>(0x10),
        output.data(),
        -1,
        &status) == 0);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR);
    status = 0;
    RequireInvalidArgument([&] { uloc_getDisplayScript_nid_postfix("sr_Latn", "en", output.data(), 16, nullptr); });
    errno = 73;
    RequireInvalidArgument([&] {
        uloc_getDisplayScript_nid_postfix("sr_Latn", "en", output.data(), 16, reinterpret_cast<std::int32_t*>(0x10));
    });
    Require(errno == 73);
    errno = 74;
    RequireInvalidArgument([&] {
        uloc_getDisplayScript_nid_postfix(reinterpret_cast<const char*>(0x10), "en", output.data(), 16, &status);
    });
    Require(errno == 74 && status == 0);
    errno = 75;
    RequireInvalidArgument([&] {
        uloc_getDisplayScript_nid_postfix("sr_Latn", reinterpret_cast<const char*>(0x10), output.data(), 16, &status);
    });
    Require(errno == 75 && status == 0);
    errno = 76;
    RequireInvalidArgument([&] {
        uloc_getDisplayScript_nid_postfix("sr_Latn", "en", reinterpret_cast<char16_t*>(0x10), 16, &status);
    });
    Require(errno == 76 && status == 0);
    status = 1;
    Require(uloc_getDisplayScript_nid_postfix(
        reinterpret_cast<const char*>(0x10),
        reinterpret_cast<const char*>(0x10),
        reinterpret_cast<char16_t*>(0x10),
        16,
        &status) == 0);
    Require(status == 1);
}

void CheckLocaleBounds() {
    std::array<char, ULOC_FULLNAME_CAPACITY> maximum{};
    maximum.fill('a');
    maximum.back() = 0;
    std::array<char16_t, 16> output{};
    std::int32_t status = 0;
    uloc_getDisplayScript_nid_postfix(maximum.data(), "en", output.data(), 16, &status);

    std::array<char, ULOC_FULLNAME_CAPACITY + 1> oversized{};
    oversized.fill('a');
    oversized.back() = 0;
    status = 0;
    RequireInvalidArgument([&] {
        uloc_getDisplayScript_nid_postfix(oversized.data(), "en", output.data(), 16, &status);
    });
    status = 0;
    RequireInvalidArgument([&] {
        uloc_getDisplayScript_nid_postfix("sr_Latn", oversized.data(), output.data(), 16, &status);
    });
}

}

int main() {
    CheckBehavior();
    CheckGuestPointers();
    CheckLocaleBounds();
}
