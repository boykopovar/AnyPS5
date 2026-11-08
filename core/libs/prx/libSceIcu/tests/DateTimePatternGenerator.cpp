#include "prx/libSceIcu/libSceIcu.hpp"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udatpg_open_nid_postfix), UDateTimePatternGenerator* (APS5_VABI*)(const char*, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU DateTimePatternGenerator check failed at line %u\n", location.line());
    std::abort();
}

void TestPreexistingFailure() {
    for (const std::int32_t error : {1, 2, 7, 15, 1000, INT32_MAX}) {
        std::int32_t status = error;
        Require(udatpg_open_nid_postfix(nullptr, &status) == nullptr);
        Require(status == error);

        status = error;
        Require(udatpg_open_nid_postfix("", &status) == nullptr);
        Require(status == error);

        status = error;
        Require(udatpg_open_nid_postfix("en_US", &status) == nullptr);
        Require(status == error);

        status = error;
        Require(udatpg_open_nid_postfix("root", &status) == nullptr);
        Require(status == error);

        status = error;
        Require(udatpg_open_nid_postfix("de_DE@calendar=buddhist", &status) == nullptr);
        Require(status == error);
    }
}

}

int main() {
    TestPreexistingFailure();
}
