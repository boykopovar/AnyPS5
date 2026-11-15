#include "prx/libSceNpUtility/libSceNpUtility.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    constexpr int appInfoInvalid = static_cast<int>(0x8055b204u);
    constexpr int titleMetadataInvalid = static_cast<int>(0x8055aa03u);
    unsigned char block[64]{};
    void* p = block;

    for (const std::uint32_t value : {0u, 1u, 0xffffffffu}) {
        Require(sceNpAppInfoIntCheckAvailability(value, nullptr, value, p, p) == appInfoInvalid, "CheckAvailability: null argument 2");
        Require(sceNpAppInfoIntCheckAvailability(value, p, value, nullptr, p) == appInfoInvalid, "CheckAvailability: null argument 4");
        Require(sceNpAppInfoIntCheckAvailability(value, p, value, p, nullptr) == appInfoInvalid, "CheckAvailability: null argument 5");

        Require(sceNpAppInfoIntCheckAvailabilityA(value, value, value, nullptr, p) == appInfoInvalid, "CheckAvailabilityA: null argument 4");
        Require(sceNpAppInfoIntCheckAvailabilityA(value, value, value, p, nullptr) == appInfoInvalid, "CheckAvailabilityA: null argument 5");

        Require(sceNpAppInfoIntCheckAvailabilityAll(value, nullptr, value, p, 1, p) == appInfoInvalid, "CheckAvailabilityAll: null argument 2");
        Require(sceNpAppInfoIntCheckAvailabilityAll(value, nullptr, value, nullptr, 0, p) == appInfoInvalid, "CheckAvailabilityAll: argument 2 is checked first");
        Require(sceNpAppInfoIntCheckAvailabilityAll(value, p, value, nullptr, 1, p) == appInfoInvalid, "CheckAvailabilityAll: null list with a count");
        Require(sceNpAppInfoIntCheckAvailabilityAll(value, p, value, nullptr, 0xffffffffffffffffull, p) == appInfoInvalid, "CheckAvailabilityAll: null list with a 64-bit count");
        Require(sceNpAppInfoIntCheckAvailabilityAll(value, p, value, p, 1, nullptr) == appInfoInvalid, "CheckAvailabilityAll: null argument 6");
        Require(sceNpAppInfoIntCheckAvailabilityAll(value, p, value, nullptr, 0, nullptr) == appInfoInvalid, "CheckAvailabilityAll: null argument 6 with an empty list");

        Require(sceNpAppInfoIntCheckAvailabilityAllA(value, value, value, nullptr, 1, p) == appInfoInvalid, "CheckAvailabilityAllA: null list with a count");
        Require(sceNpAppInfoIntCheckAvailabilityAllA(value, value, value, p, 1, nullptr) == appInfoInvalid, "CheckAvailabilityAllA: null argument 6");
        Require(sceNpAppInfoIntCheckAvailabilityAllA(value, value, value, nullptr, 0, nullptr) == appInfoInvalid, "CheckAvailabilityAllA: null argument 6 with an empty list");

        Require(sceNpAppInfoIntCheckServiceAvailability(value, nullptr, value, value, p, p) == appInfoInvalid, "CheckServiceAvailability: null argument 2");
        Require(sceNpAppInfoIntCheckServiceAvailability(value, p, value, value, nullptr, p) == appInfoInvalid, "CheckServiceAvailability: null argument 5");
        Require(sceNpAppInfoIntCheckServiceAvailability(value, p, value, value, p, nullptr) == appInfoInvalid, "CheckServiceAvailability: null argument 6");

        Require(sceNpAppInfoIntCheckServiceAvailabilityA(value, value, value, value, nullptr, p) == appInfoInvalid, "CheckServiceAvailabilityA: null argument 5");
        Require(sceNpAppInfoIntCheckServiceAvailabilityA(value, value, value, value, p, nullptr) == appInfoInvalid, "CheckServiceAvailabilityA: null argument 6");

        Require(sceNpAppInfoIntCheckServiceAvailabilityAll(value, nullptr, value, value, p, 1, p) == appInfoInvalid, "CheckServiceAvailabilityAll: null argument 2");
        Require(sceNpAppInfoIntCheckServiceAvailabilityAll(value, p, value, value, nullptr, 1, p) == appInfoInvalid, "CheckServiceAvailabilityAll: null list with a count");
        Require(sceNpAppInfoIntCheckServiceAvailabilityAll(value, p, value, value, p, 1, nullptr) == appInfoInvalid, "CheckServiceAvailabilityAll: null argument 7");
        Require(sceNpAppInfoIntCheckServiceAvailabilityAll(value, p, value, value, nullptr, 0, nullptr) == appInfoInvalid, "CheckServiceAvailabilityAll: null argument 7 with an empty list");

        Require(sceNpAppInfoIntCheckServiceAvailabilityAllA(value, value, value, value, nullptr, 1, p) == appInfoInvalid, "CheckServiceAvailabilityAllA: null list with a count");
        Require(sceNpAppInfoIntCheckServiceAvailabilityAllA(value, value, value, value, p, 1, nullptr) == appInfoInvalid, "CheckServiceAvailabilityAllA: null argument 7");
        Require(sceNpAppInfoIntCheckServiceAvailabilityAllA(value, value, value, value, nullptr, 0, nullptr) == appInfoInvalid, "CheckServiceAvailabilityAllA: null argument 7 with an empty list");

        Require(sceNpTitleMetadataIntGetInfo(value, nullptr, p) == titleMetadataInvalid, "TitleMetadataIntGetInfo: null argument 2");
        Require(sceNpTitleMetadataIntGetInfo(value, p, nullptr) == titleMetadataInvalid, "TitleMetadataIntGetInfo: null argument 3");
    }
    return 0;
}
