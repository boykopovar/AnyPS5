#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "ucmndata.h"
#include "udataswp.h"
#include "uresdata.h"
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#undef uloc_getCountry

namespace {

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t BufferOverflowError = 15;
constexpr std::int32_t StringNotTerminatedWarning = -124;

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu supplemental region: " + message);
}

struct Result {
    std::int32_t length;
    std::int32_t status;
    char region[8];
};

Result Region(const char* locale, bool infer, std::int32_t capacity = 8, std::int32_t status = ZeroError) {
    Result result{0, status, {}};
    std::memset(result.region, 'x', sizeof(result.region));
    result.length = ulocimp_getRegionForSupplementalData_nid_postfix(locale, infer ? 1 : 0, result.region, capacity,
                                                                      &result.status);
    return result;
}

void RequireRegion(const char* locale, bool infer, const std::string& expected) {
    const auto result = Region(locale, infer);
    const std::string name = std::string(locale) + (infer ? " with inference" : "");
    Require(result.status == ZeroError, name + " failed");
    Require(result.length == static_cast<std::int32_t>(expected.size()), name + " returned the wrong length");
    Require(std::string(result.region) == expected, name + " returned " + result.region);
    for (std::size_t index = expected.size(); index < sizeof(result.region); ++index)
        Require(result.region[index] == 0, name + " did not zero the rest of the buffer");
}

struct SingleResourcePackage {
    DataHeader header;
    std::uint32_t count;
    std::uint32_t reserved;
    const char* name;
    const void* resource;
};

bool RegisterLanguageInfo(const char* path) {
    std::ifstream input(path, std::ios::binary);
    static std::vector<char> resource{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (resource.empty()) return false;
    const auto length = static_cast<std::int32_t>(resource.size());
    UErrorCode status = U_ZERO_ERROR;
    auto* swapper = udata_openSwapperForInputData(resource.data(), length, U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, &status);
    if (U_SUCCESS(status)) {
        ures_swap(swapper, resource.data(), length, resource.data(), &status);
        udata_closeSwapper(swapper);
    }
    if (U_FAILURE(status)) return false;
    alignas(16) static const SingleResourcePackage package{
        {{sizeof(DataHeader), 0xda, 0x27}, {sizeof(UDataInfo), 0, U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, U_SIZEOF_UCHAR,
          0, {'T', 'o', 'C', 'P'}, {1, 0, 0, 0}, {}}},
        1, 0, U_ICUDATA_NAME "/langInfo.res", resource.data()
    };
    std::int32_t guestStatus = ZeroError;
    udata_setCommonData_nid_postfix(&package, &guestStatus);
    return guestStatus == ZeroError;
}

template <typename TCall>
void RequireThrows(TCall call, const std::string& message) {
    try {
        call();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

}

int main(int argc, char** argv) {
    if (argc > 2) return 1;
    const bool likelyData = argc == 2;
    Require(!likelyData || RegisterLanguageInfo(argv[1]), "could not register langInfo.res");

    RequireRegion("en", true, likelyData ? "US" : "");
    RequireRegion("zh_Hant", true, likelyData ? "TW" : "");
    RequireRegion("sr_Cyrl", true, likelyData ? "RS" : "");
    RequireRegion("und", true, likelyData ? "US" : "");
    RequireRegion("en@rg=frxxxx", true, likelyData ? "US" : "");
    RequireRegion("en_US", false, "US");
    RequireRegion("en_US", true, "US");
    RequireRegion("en_US@rg=gbzzzz", false, "GB");
    RequireRegion("en_US@rg=GBZZZZ", false, "GB");
    RequireRegion("en_US@rg=gBzZzZ", true, "GB");
    RequireRegion("en_US@calendar=gregorian;rg=gbzzzz", false, "GB");
    RequireRegion("en-US-u-rg-gbzzzz", false, "GB");
    RequireRegion("en_US@rg=gbxxxx", false, "US");
    RequireRegion("en_US@rg=gbzz", false, "US");
    RequireRegion("en_US@rg=gbzzzzz", false, "US");
    RequireRegion("en_US@rg=gbzzzzzzzz", false, "US");
    RequireRegion("en_419", false, "419");
    RequireRegion("en", false, "");
    RequireRegion("", false, "");
    RequireRegion("en@rg=gbzzzz", false, "GB");
    RequireRegion("en@rg=gbzzzz", true, "GB");

    auto failed = Region("en_US", false, 8, IllegalArgumentError);
    Require(failed.length == 0, "an incoming failure did not return 0");
    Require(failed.status == IllegalArgumentError, "an incoming failure was changed");
    Require(failed.region[0] == 'x', "an incoming failure wrote the buffer");

    auto warned = Region("en_US", false, 8, StringNotTerminatedWarning);
    Require(warned.length == 2 && warned.status == ZeroError, "a not-terminated warning was not cleared");

    static const std::int32_t readOnlyFailure = IllegalArgumentError;
    static const std::int32_t readOnlyZero = ZeroError;
    auto* failureStatus = const_cast<std::int32_t*>(&readOnlyFailure);
    auto* zeroStatus = const_cast<std::int32_t*>(&readOnlyZero);
    Require(!GuestWritable_nid_no_patch(failureStatus, sizeof(std::int32_t)) &&
                !GuestWritable_nid_no_patch(zeroStatus, sizeof(std::int32_t)),
            "the read-only statuses are writable");
    char readOnlyRegion[8];
    std::memset(readOnlyRegion, 'x', sizeof(readOnlyRegion));
    Require(ulocimp_getRegionForSupplementalData_nid_postfix("en_US", 0, readOnlyRegion, 8, failureStatus) == 0,
            "a read-only incoming failure did not return 0");
    Require(readOnlyRegion[0] == 'x', "a read-only incoming failure wrote the buffer");
    Require(ulocimp_getRegionForSupplementalData_nid_postfix("en_US@rg=gbzzzz", 0, readOnlyRegion, 8, zeroStatus) == 2,
            "a read-only status that stays zero returned the wrong length");
    Require(std::string(readOnlyRegion) == "GB", "a read-only status that stays zero returned the wrong region");
    RequireThrows([&] { ulocimp_getRegionForSupplementalData_nid_postfix("en_US", 0, readOnlyRegion, 2, zeroStatus); },
                  "a read-only status that needs the not-terminated warning did not throw");
    RequireThrows([&] { ulocimp_getRegionForSupplementalData_nid_postfix("en_US", 0, readOnlyRegion, 1, zeroStatus); },
                  "a read-only status that needs the overflow error did not throw");

    auto exact = Region("en_US", false, 2);
    Require(exact.length == 2, "an exact buffer returned the wrong length");
    Require(exact.status == StringNotTerminatedWarning, "an exact buffer did not warn");
    Require(exact.region[0] == 'U' && exact.region[1] == 'S' && exact.region[2] == 'x', "an exact buffer was wrong");

    auto small = Region("en_US", false, 1);
    Require(small.length == 2 && small.status == BufferOverflowError, "a small buffer did not overflow");
    Require(small.region[0] == 'U' && small.region[1] == 'x', "a small buffer was written past its capacity");

    std::int32_t status = ZeroError;
    Require(ulocimp_getRegionForSupplementalData_nid_postfix("en_US", 0, nullptr, 0, &status) == 2,
            "preflighting returned the wrong length");
    Require(status == BufferOverflowError, "preflighting did not overflow");

    status = ZeroError;
    Require(ulocimp_getRegionForSupplementalData_nid_postfix("en", 0, nullptr, 0, &status) == 0,
            "preflighting an empty region returned the wrong length");
    Require(status == StringNotTerminatedWarning, "preflighting an empty region did not warn");

    std::int32_t defaultStatus = ZeroError;
    char defaultCountry[8] = {};
    std::int32_t expectedLength = 0;
    bool defaultThrows = false;
    try {
        expectedLength = uloc_getCountry(nullptr, defaultCountry, 8, &defaultStatus);
    } catch (const std::exception&) {
        defaultThrows = true;
    }
    if (defaultThrows) {
        RequireThrows(
            [] {
                std::int32_t error = ZeroError;
                char buffer[8] = {};
                ulocimp_getRegionForSupplementalData_nid_postfix(nullptr, 0, buffer, 8, &error);
            },
            "a null locale did not throw like uloc_getCountry");
    } else {
        status = ZeroError;
        char region[8] = {};
        const auto defaultLength = ulocimp_getRegionForSupplementalData_nid_postfix(nullptr, 0, region, 8, &status);
        Require(status == defaultStatus, "a null locale did not use the default locale's status");
        Require(defaultLength == expectedLength && std::string(region) == defaultCountry,
                "a null locale did not use the default locale");
    }

    RequireThrows([] { ulocimp_getRegionForSupplementalData_nid_postfix("en_US", 0, nullptr, 0, nullptr); },
                  "a null status did not throw");
    RequireThrows(
        [] {
            std::int32_t error = ZeroError;
            ulocimp_getRegionForSupplementalData_nid_postfix("en_US", 0, nullptr, 8, &error);
        },
        "a null buffer with a capacity did not throw");
    RequireThrows(
        [] {
            std::int32_t error = ZeroError;
            char buffer[8] = {};
            ulocimp_getRegionForSupplementalData_nid_postfix("en_US", 0, buffer, -1, &error);
        },
        "a negative capacity did not throw");
    return 0;
}
