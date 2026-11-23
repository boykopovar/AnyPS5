#include "prx/libSceIcu/libSceIcu.hpp"
#include "uresimp.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<
    decltype(&ures_getFunctionalEquivalent_nid_postfix),
    std::int32_t (APS5_VABI*)(char*, std::int32_t, const char*, const char*, const char*, const char*, std::int8_t*, std::int8_t, std::int32_t*)>);

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

}

int main() {
    char buffer[64]{};
    std::int8_t available = 0;

    bool threw = false;
    try {
        ures_getFunctionalEquivalent_nid_postfix(buffer, sizeof(buffer), nullptr, "calendar", "calendar", "en_US", &available, 0, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    std::int32_t incomingFailure = 1;
    Require(ures_getFunctionalEquivalent_nid_postfix(buffer, sizeof(buffer), nullptr, "calendar", "calendar", "en_US", &available, 0, &incomingFailure) == 0);
    Require(incomingFailure == 1);

    std::int32_t status = 0;
    Require(ures_getFunctionalEquivalent_nid_postfix(nullptr, 8, nullptr, "calendar", "calendar", "en_US", &available, 0, &status) == 0);
    Require(status == 1);

    status = 0;
    Require(ures_getFunctionalEquivalent_nid_postfix(buffer, -1, nullptr, "calendar", "calendar", "en_US", &available, 0, &status) == 0);
    Require(status == 1);

    std::array<char, 64> actual{};
    std::array<char, 64> expected{};
    std::int8_t actualAvail = -1;
    std::int8_t expectedAvail = -1;
    std::int32_t actualStatus = 0;
    UErrorCode expectedStatus = U_ZERO_ERROR;

    const auto actualLen = ures_getFunctionalEquivalent_nid_postfix(
        actual.data(), static_cast<std::int32_t>(actual.size()), nullptr, "calendar", "calendar", "en_US@calendar=gregorian", &actualAvail, 0, &actualStatus);
    const auto expectedLen = ures_getFunctionalEquivalent(
        expected.data(), static_cast<std::int32_t>(expected.size()), nullptr, "calendar", "calendar", "en_US@calendar=gregorian", &expectedAvail, 0, &expectedStatus);

    Require(actualLen == expectedLen);
    Require(actualStatus == static_cast<std::int32_t>(expectedStatus));
    Require(actualAvail == expectedAvail);
    Require(std::memcmp(actual.data(), expected.data(), actual.size()) == 0);

    std::int32_t preflightStatus = 0;
    std::int8_t preflightAvail = -1;
    const auto preflightLen = ures_getFunctionalEquivalent_nid_postfix(
        nullptr, 0, nullptr, "calendar", "calendar", "en_US@calendar=gregorian", &preflightAvail, 0, &preflightStatus);
    Require(preflightLen == expectedLen);
    Require(preflightAvail == expectedAvail);

    std::int32_t defaultStatus = 0;
    uloc_setDefault_nid_postfix("de_DE@calendar=gregorian", &defaultStatus);
    Require(defaultStatus <= 0);
    std::array<char, 64> defaultActual{};
    std::array<char, 64> explicitActual{};
    std::int8_t defaultAvail = -1;
    std::int8_t explicitAvail = -1;
    std::int32_t defaultCallStatus = 0;
    std::int32_t explicitCallStatus = 0;
    const auto defaultLen = ures_getFunctionalEquivalent_nid_postfix(
        defaultActual.data(), static_cast<std::int32_t>(defaultActual.size()), nullptr, "calendar", "calendar", nullptr, &defaultAvail, 0, &defaultCallStatus);
    const auto explicitLen = ures_getFunctionalEquivalent_nid_postfix(
        explicitActual.data(), static_cast<std::int32_t>(explicitActual.size()), nullptr, "calendar", "calendar", "de_DE@calendar=gregorian", &explicitAvail, 0, &explicitCallStatus);
    Require(defaultLen == explicitLen);
    Require(defaultCallStatus == explicitCallStatus);
    Require(defaultAvail == explicitAvail);
    Require(std::memcmp(defaultActual.data(), explicitActual.data(), defaultActual.size()) == 0);
    defaultStatus = 0;
    uloc_setDefault_nid_postfix(nullptr, &defaultStatus);
    return 0;
}
