#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udtitvfmt_open_nid_postfix),
                             UDateIntervalFormat* (APS5_VABI*)(const char*, const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU date interval format check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

void PreexistingFailureShortCircuits() {
    const auto* badLocale = reinterpret_cast<const char*>(std::uintptr_t{1});
    const auto* badSkeleton = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const auto* badTz = reinterpret_cast<const char16_t*>(std::uintptr_t{1});

    for (std::int32_t incoming : {1, 2, 7, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        auto status = incoming;
        Require(udtitvfmt_open_nid_postfix("en_US", u"yMMMd", -1, u"UTC", -1, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(udtitvfmt_open_nid_postfix(nullptr, nullptr, 5, nullptr, -2, &status) == nullptr);
        Require(status == incoming);

        status = incoming;
        Require(udtitvfmt_open_nid_postfix(badLocale, badSkeleton, -99, badTz, -99, &status) == nullptr);
        Require(status == incoming);
    }
}

void ArgumentValidation() {
    const auto* badLocale = reinterpret_cast<const char*>(std::uintptr_t{1});
    const auto* badSkeleton = reinterpret_cast<const char16_t*>(std::uintptr_t{1});
    const auto* badTz = reinterpret_cast<const char16_t*>(std::uintptr_t{1});

    for (std::int32_t incoming : {0, -128, -124, -1, std::numeric_limits<std::int32_t>::min()}) {
        for (std::int32_t badNullLength : {-1, 1, 4, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            auto status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, nullptr, badNullLength, nullptr, 0, &status) == nullptr);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, u"yMMMd", -1, nullptr, badNullLength, &status) == nullptr);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, nullptr, 0, nullptr, badNullLength, &status) == nullptr);
            Require(status == IllegalArgument);
        }

        for (std::int32_t badNonNullLength : {-2, -100, std::numeric_limits<std::int32_t>::min()}) {
            auto status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, badSkeleton, badNonNullLength, nullptr, 0, &status) == nullptr);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, u"yMMMd", 5, badTz, badNonNullLength, &status) == nullptr);
            Require(status == IllegalArgument);

            status = incoming;
            Require(udtitvfmt_open_nid_postfix(badLocale, badSkeleton, badNonNullLength, badTz, badNonNullLength, &status) == nullptr);
            Require(status == IllegalArgument);
        }
    }
}

void NullStatusThrows() {
    RequireThrows([&] { udtitvfmt_open_nid_postfix("en_US", u"yMMMd", -1, nullptr, 0, nullptr); });
    RequireThrows([&] { udtitvfmt_open_nid_postfix(nullptr, nullptr, 1, nullptr, 0, nullptr); });
}

} // namespace

int main() {
    PreexistingFailureShortCircuits();
    ArgumentValidation();
    NullStatusThrows();
    std::puts("ICU date interval format tests passed");
}
