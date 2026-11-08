#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udat_toPattern_nid_postfix),
                             std::int32_t (APS5_VABI*)(const UDateFormat*, std::int8_t, char16_t*, std::int32_t, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t NotTerminated = -124;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU udat_toPattern check failed at line %u\n", location.line());
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

void PreexistingFailure() {
    std::uintptr_t dummyFmt = 1;
    const auto* nonNullFmt = reinterpret_cast<const UDateFormat*>(&dummyFmt);
    char16_t buffer[4] = {u'a', u'b', u'c', u'd'};

    for (std::int32_t incoming : {IllegalArgument, 2, 7, 15, 16, std::numeric_limits<std::int32_t>::max()}) {
        for (std::int8_t localized : {std::int8_t{0}, std::int8_t{1}, std::int8_t{-1}, std::int8_t{42}}) {
            std::int32_t status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, 0, &status) == -1);
            Require(status == incoming);

            status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, 5, &status) == -1);
            Require(status == incoming);

            status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, -1, &status) == -1);
            Require(status == incoming);

            status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, buffer, -1, &status) == -1);
            Require(status == incoming);
            Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == u'd');

            status = incoming;
            Require(udat_toPattern_nid_postfix(nonNullFmt, localized, buffer, 4, &status) == -1);
            Require(status == incoming);
            Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == u'd');

            status = incoming;
            Require(udat_toPattern_nid_postfix(nonNullFmt, localized, nullptr, 3, &status) == -1);
            Require(status == incoming);
        }
    }
}

void BufferArguments() {
    std::uintptr_t dummyFmt = 1;
    const auto* nonNullFmt = reinterpret_cast<const UDateFormat*>(&dummyFmt);
    char16_t buffer[4] = {u'w', u'x', u'y', u'z'};

    for (std::int32_t incoming : {0, -1, NotTerminated, -128, std::numeric_limits<std::int32_t>::min()}) {
        for (std::int8_t localized : {std::int8_t{0}, std::int8_t{1}, std::int8_t{-1}}) {
            for (std::int32_t badLength : {-1, -2, 1, 8, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
                std::int32_t status = incoming;
                Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, badLength, &status) == -1);
                Require(status == IllegalArgument);

                status = incoming;
                Require(udat_toPattern_nid_postfix(nonNullFmt, localized, nullptr, badLength, &status) == -1);
                Require(status == IllegalArgument);
            }

            for (std::int32_t negativeLength : {-1, -5, std::numeric_limits<std::int32_t>::min()}) {
                std::int32_t status = incoming;
                Require(udat_toPattern_nid_postfix(nullptr, localized, buffer, negativeLength, &status) == -1);
                Require(status == IllegalArgument);
                Require(buffer[0] == u'w' && buffer[1] == u'x' && buffer[2] == u'y' && buffer[3] == u'z');

                status = incoming;
                Require(udat_toPattern_nid_postfix(nonNullFmt, localized, buffer, negativeLength, &status) == -1);
                Require(status == IllegalArgument);
                Require(buffer[0] == u'w' && buffer[1] == u'x' && buffer[2] == u'y' && buffer[3] == u'z');
            }
        }
    }
}

void NullFormatHandle() {
    char16_t buffer[4] = {u'm', u'n', u'o', u'p'};

    for (std::int32_t incoming : {0, -1, NotTerminated, -128, std::numeric_limits<std::int32_t>::min()}) {
        for (std::int8_t localized : {std::int8_t{0}, std::int8_t{1}, std::int8_t{-1}, std::int8_t{127}}) {
            std::int32_t status = incoming;
            Require(udat_toPattern_nid_postfix(nullptr, localized, nullptr, 0, &status) == -1);
            Require(status == IllegalArgument);

            for (std::int32_t length : {0, 1, 4, 32, std::numeric_limits<std::int32_t>::max()}) {
                status = incoming;
                Require(udat_toPattern_nid_postfix(nullptr, localized, buffer, length, &status) == -1);
                Require(status == IllegalArgument);
                Require(buffer[0] == u'm' && buffer[1] == u'n' && buffer[2] == u'o' && buffer[3] == u'p');
            }
        }
    }
}

void NullStatusThrows() {
    char16_t buffer[2] = {u'p', u'q'};
    RequireThrows([&] { udat_toPattern_nid_postfix(nullptr, 0, nullptr, 0, nullptr); });
    RequireThrows([&] { udat_toPattern_nid_postfix(nullptr, 1, buffer, 2, nullptr); });
    Require(buffer[0] == u'p' && buffer[1] == u'q');
}

} // namespace

int main() {
    PreexistingFailure();
    BufferArguments();
    NullFormatHandle();
    NullStatusThrows();
    std::puts("ICU udat_toPattern tests passed");
}
