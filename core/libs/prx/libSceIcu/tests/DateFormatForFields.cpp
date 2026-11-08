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

static_assert(std::is_same_v<decltype(&udat_formatForFields_nid_postfix),
                             std::int32_t (APS5_VABI*)(const void*, double, char16_t*, std::int32_t, void*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr char16_t Sentinel = 0xa55a;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU udat_formatForFields check failed at line %u\n", location.line());
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
    int dummyFormat = 0;
    int dummyIterator = 0;
    for (const std::int32_t initialStatus : {1, 2, 7, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        std::array<char16_t, 4> buffer{Sentinel, Sentinel, Sentinel, Sentinel};
        std::int32_t status = initialStatus;
        Require(udat_formatForFields_nid_postfix(nullptr, 0.0, nullptr, 0, nullptr, &status) == -1);
        Require(status == initialStatus);

        status = initialStatus;
        Require(udat_formatForFields_nid_postfix(&dummyFormat, 1700000000000.0, nullptr, 5, &dummyIterator, &status) == -1);
        Require(status == initialStatus);

        status = initialStatus;
        Require(udat_formatForFields_nid_postfix(&dummyFormat, -12345.5, buffer.data(), -3, &dummyIterator, &status) == -1);
        Require(status == initialStatus);
        for (const char16_t unit : buffer) Require(unit == Sentinel);
    }
}

void IllegalBufferArguments() {
    int dummyFormat = 0;
    int dummyIterator = 0;
    for (const std::int32_t initialStatus : {0, -128, -124, -1, std::numeric_limits<std::int32_t>::min()}) {
        for (const std::int32_t length : {-100, -1, 1, 8, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            std::int32_t status = initialStatus;
            Require(udat_formatForFields_nid_postfix(nullptr, 0.0, nullptr, length, nullptr, &status) == -1);
            Require(status == IllegalArgument);

            status = initialStatus;
            Require(udat_formatForFields_nid_postfix(&dummyFormat, 987654.25, nullptr, length, &dummyIterator, &status) == -1);
            Require(status == IllegalArgument);
        }

        for (const std::int32_t length : {-1, -8, std::numeric_limits<std::int32_t>::min()}) {
            std::array<char16_t, 4> buffer{Sentinel, Sentinel, Sentinel, Sentinel};
            std::int32_t status = initialStatus;
            Require(udat_formatForFields_nid_postfix(nullptr, 0.0, buffer.data(), length, nullptr, &status) == -1);
            Require(status == IllegalArgument);
            for (const char16_t unit : buffer) Require(unit == Sentinel);

            status = initialStatus;
            Require(udat_formatForFields_nid_postfix(&dummyFormat, -42.0, buffer.data(), length, &dummyIterator, &status) == -1);
            Require(status == IllegalArgument);
            for (const char16_t unit : buffer) Require(unit == Sentinel);
        }
    }
}

void NullStatusThrows() {
    std::array<char16_t, 2> buffer{Sentinel, Sentinel};
    RequireThrows([&] { udat_formatForFields_nid_postfix(nullptr, 0.0, nullptr, 0, nullptr, nullptr); });
    RequireThrows([&] { udat_formatForFields_nid_postfix(nullptr, 0.0, buffer.data(), -1, nullptr, nullptr); });
    Require(buffer[0] == Sentinel && buffer[1] == Sentinel);
}

} // namespace

int main() {
    FailureShortCircuit();
    IllegalBufferArguments();
    NullStatusThrows();
    std::puts("ICU udat_formatForFields tests passed");
}
