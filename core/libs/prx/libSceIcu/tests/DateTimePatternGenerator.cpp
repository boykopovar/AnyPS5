#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

static_assert(std::is_same_v<decltype(&udatpg_getFieldDisplayName_nid_postfix),
                             std::int32_t (APS5_VABI*)(const UDateTimePatternGenerator*,
                                                      std::int32_t,
                                                      std::int32_t,
                                                      char16_t*,
                                                      std::int32_t,
                                                      std::int32_t*)>);

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU udatpg_getFieldDisplayName check failed at line %u\n", location.line());
    std::abort();
}

void CheckPreexistingFailure() {
    std::array<char16_t, 8> buffer = {u'a', u'b', u'c', u'd', 0, 0, 0, 0};
    std::int32_t status = 5;
    const auto ret = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
    Require(ret == -1);
    Require(status == 5);
    Require(buffer[0] == u'a' && buffer[1] == u'b' && buffer[2] == u'c' && buffer[3] == u'd');

    status = 1;
    const auto retNull = udatpg_getFieldDisplayName_nid_postfix(nullptr, 1, 2, nullptr, -10, &status);
    Require(retNull == -1);
    Require(status == 1);
}

void CheckInvalidArgument() {
    std::int32_t status = 0;
    const auto ret1 = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, nullptr, 1, &status);
    Require(ret1 == -1);
    Require(status == 1);

    status = 0;
    const auto ret2 = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, nullptr, -1, &status);
    Require(ret2 == -1);
    Require(status == 1);

    std::array<char16_t, 4> buffer = {u'x', u'y', u'z', 0};
    status = 0;
    const auto ret3 = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, buffer.data(), -1, &status);
    Require(ret3 == -1);
    Require(status == 1);
    Require(buffer[0] == u'x' && buffer[1] == u'y' && buffer[2] == u'z');

    status = -124;
    const auto ret4 = udatpg_getFieldDisplayName_nid_postfix(nullptr, 0, 0, nullptr, 4, &status);
    Require(ret4 == -1);
    Require(status == 1);
}

}

int main() {
    CheckPreexistingFailure();
    CheckInvalidArgument();
    std::puts("ICU udatpg_getFieldDisplayName tests passed");
}
