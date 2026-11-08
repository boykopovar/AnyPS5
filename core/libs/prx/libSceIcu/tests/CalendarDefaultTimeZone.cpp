#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&ucal_getDefaultTimeZone_nid_postfix), std::int32_t (APS5_VABI*)(char16_t*, std::int32_t, std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU ucal_getDefaultTimeZone check failed at line %u\n", location.line());
    std::abort();
}

void TestNullStatus() {
    Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, 0, nullptr) == 0);
    Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, 16, nullptr) == 0);
    Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, -1, nullptr) == 0);

    char16_t buffer[8] = {u'a', u'b', u'c', u'd', u'e', u'f', u'g', u'\0'};
    Require(ucal_getDefaultTimeZone_nid_postfix(buffer, 8, nullptr) == 0);
    Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == u'd');
}

void TestFailingStatus() {
    constexpr std::array<std::int32_t, 4> failureCodes{1, 7, 15, 65535};
    for (std::int32_t code : failureCodes) {
        std::int32_t status = code;
        Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, 0, &status) == 0);
        Require(status == code);

        status = code;
        Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, 32, &status) == 0);
        Require(status == code);

        status = code;
        Require(ucal_getDefaultTimeZone_nid_postfix(nullptr, -1, &status) == 0);
        Require(status == code);

        char16_t buffer[8] = {u'x', u'y', u'z', u'w', u'1', u'2', u'3', u'\0'};
        status = code;
        Require(ucal_getDefaultTimeZone_nid_postfix(buffer, 8, &status) == 0);
        Require(status == code);
        Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z' && buffer[3] == u'w');
    }
}

}

int main() {
    TestNullStatus();
    TestFailingStatus();
    std::puts("ICU ucal_getDefaultTimeZone tests passed");
}
