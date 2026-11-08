#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udatpg_getSkeleton_nid_postfix),
              std::int32_t (APS5_VABI*)(UDateTimePatternGenerator*, const char16_t*, std::int32_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU pattern generator check failed at line %u\n", location.line());
    std::abort();
}

void PreexistingError() {
    constexpr std::int32_t IllegalArgument = 1;
    constexpr std::int32_t BufferOverflow = 15;
    constexpr std::int32_t CustomError = 42;

    for (std::int32_t error : {IllegalArgument, BufferOverflow, CustomError}) {
        std::int32_t status = error;
        std::array<char16_t, 8> buffer = {u'x', u'y', u'z', 0};
        const std::int32_t result = udatpg_getSkeleton_nid_postfix(nullptr, nullptr, 0, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == error);
        Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == 0);

        status = error;
        const char16_t pattern[] = u"yyyyMMdd";
        const std::int32_t withPattern = udatpg_getSkeleton_nid_postfix(nullptr, pattern, 8, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(withPattern == 0);
        Require(status == error);
        Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == 0);

        status = error;
        const std::int32_t invalidArgs = udatpg_getSkeleton_nid_postfix(nullptr, nullptr, 5, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(invalidArgs == 0);
        Require(status == error);
        Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == 0);
    }
}

void NullPatternNonzeroLength() {
    constexpr std::int32_t IllegalArgument = 1;
    constexpr std::int32_t NotTerminated = -124;

    for (std::int32_t length : {1, 2, 10, -1, -5}) {
        std::int32_t status = 0;
        std::array<char16_t, 4> buffer = {u'a', u'b', u'c', 0};
        const std::int32_t result = udatpg_getSkeleton_nid_postfix(nullptr, nullptr, length, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == IllegalArgument);
        Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == 0);
    }

    for (std::int32_t warning : {-1, -5, NotTerminated}) {
        std::int32_t status = warning;
        std::array<char16_t, 4> buffer = {u'a', u'b', u'c', 0};
        const std::int32_t result = udatpg_getSkeleton_nid_postfix(nullptr, nullptr, 3, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
        Require(result == 0);
        Require(status == IllegalArgument);
        Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == 0);
    }
}

} // namespace

int main() {
    PreexistingError();
    NullPatternNonzeroLength();
    std::puts("ICU date-time pattern generator tests passed");
}
