#include "prx/libSceIcu/include/InventoryExports.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

using GuestCall = const char* (APS5_VABI*)(const char*, const char*, std::int32_t*);
static_assert(std::is_same_v<decltype(&ucnv_getCanonicalName), GuestCall>);

}

#include "unicode/ucnv.h"

namespace {

constexpr auto HostGetCanonicalName = &ucnv_getCanonicalName;

}

#undef ucnv_getCanonicalName

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU converter canonical name: %s\n", message);
    std::abort();
}

bool Equal(const char* left, const char* right) {
    return (!left && !right) || (left && right && std::strcmp(left, right) == 0);
}

void Compare(const char* alias, const char* standard, std::int32_t initial) {
    UErrorCode nativeStatus = static_cast<UErrorCode>(initial);
    const char* native = HostGetCanonicalName(alias, standard, &nativeStatus);
    std::array<std::int32_t, 3> guestStatus{71, initial, 73};
    const char* guest = ucnv_getCanonicalName(alias, standard, &guestStatus[1]);
    Require(Equal(native, guest), "result differs from pinned ICU");
    Require(guestStatus[1] == static_cast<std::int32_t>(nativeStatus), "status differs from pinned ICU");
    Require(guestStatus[0] == 71 && guestStatus[2] == 73, "status write changed canaries");
}

void DefinedInputs() {
    for (std::int32_t status : {0, -128, 1}) {
        Compare(nullptr, "IANA", status);
        Compare("", nullptr, status);
        Compare("", "", status);
        Compare("UTF-8", "IANA", status);
        Compare("ibm-1208", "IBM", status);
        Compare("no-such-alias", "no-such-standard", status);
        const std::string longAlias(256, 'x');
        const std::string longStandard(256, 'y');
        Compare(longAlias.c_str(), longStandard.c_str(), status);
    }
}

void PointerOrder() {
    auto* invalidText = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    std::int32_t failed = 1;
    Require(ucnv_getCanonicalName(invalidText, invalidText, &failed) == nullptr, "failed status returns null");
    Require(failed == 1, "failed status remains unchanged");
    UErrorCode availability = U_ZERO_ERROR;
    HostGetCanonicalName("", "", &availability);
    std::int32_t status = 0;
    Require(ucnv_getCanonicalName("", invalidText, &status) == nullptr, "empty alias does not read standard");
    Require(status == static_cast<std::int32_t>(availability), "empty alias status matches pinned ICU");
    status = 0;
    if (U_FAILURE(availability)) {
        Require(ucnv_getCanonicalName(invalidText, invalidText, &status) == nullptr, "missing data returns null before strings");
        Require(status == static_cast<std::int32_t>(availability), "missing-data status matches pinned ICU");
    } else {
        bool threw = false;
        try {
            ucnv_getCanonicalName(invalidText, "IANA", &status);
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw, "unreadable alias is rejected when alias data is available");
        status = 0;
        threw = false;
        try {
            ucnv_getCanonicalName("UTF-8", invalidText, &status);
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw, "unreadable standard is rejected when alias data is available");
    }
    bool threw = false;
    try {
        ucnv_getCanonicalName("UTF-8", "IANA", reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10}));
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "unreadable status is rejected");
    threw = false;
    try {
        ucnv_getCanonicalName("UTF-8", "IANA", nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "null status is rejected");
}

}

int main() {
    DefinedInputs();
    PointerOrder();
}
