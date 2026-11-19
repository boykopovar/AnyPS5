#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "unicode/uloc.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

static_assert(std::is_same_v<decltype(&uloc_getDisplayKeywordValue_nid_postfix),
                             std::int32_t (APS5_VABI*)(const char*, const char*, const char*, char16_t*, std::int32_t, std::int32_t*)>);

constexpr std::size_t Page = 0x4000;
constexpr std::size_t MappingBytes = Page * 2;
constexpr char16_t Sentinel = 0x5a5a;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "uloc_getDisplayKeywordValue check failed at line %u\n", location.line());
    std::abort();
}

template <class Function>
void RequireInvalidArgument(Function&& function, int marker, std::source_location location = std::source_location::current()) {
    errno = marker;
    try {
        function();
    } catch (const std::invalid_argument&) {
        Require(errno == marker, location);
        return;
    } catch (const std::exception&) {
        std::fprintf(stderr, "unexpected exception type at line %u\n", location.line());
        std::abort();
    }
    std::fprintf(stderr, "expected std::invalid_argument at line %u\n", location.line());
    std::abort();
}

struct Mapping {
    std::int64_t physical = 0;
    std::uint8_t* data = nullptr;

    Mapping() {
        Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, MappingBytes, 0, 0, &physical) == 0);
        void* address = nullptr;
        Require(sceKernelMapDirectMemory(&address, MappingBytes, 3, 0, physical, 0) == 0);
        data = static_cast<std::uint8_t*>(address);
        Require(data != nullptr);
        Require(GuestArena::GuestArenaContains_nid_postfix(data, MappingBytes));
    }

    ~Mapping() {
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(data), MappingBytes) == 0);
        Require(sceKernelReleaseDirectMemory(physical, MappingBytes) == 0);
    }

    void Protect(std::size_t offset, std::size_t bytes, int prot) {
        Require(sceKernelMprotect(data + offset, bytes, prot) == 0);
    }
};

void CheckDifferential(const char* locale, const char* keyword, const char* displayLocale,
                       std::int32_t capacity, std::int32_t incoming) {
    const std::size_t allocSize = capacity > 0 ? static_cast<std::size_t>(capacity) + 4 : 4;
    std::vector<char16_t> guestDest(allocSize, Sentinel);
    std::vector<char16_t> hostDest(allocSize, Sentinel);

    std::int32_t guestStatus = incoming;
    UErrorCode hostStatus = static_cast<UErrorCode>(incoming);

    char16_t* guestPtr = capacity > 0 ? guestDest.data() + 2 : nullptr;
    char16_t* hostPtr = capacity > 0 ? hostDest.data() + 2 : nullptr;

    errno = 37;
    const std::int32_t guestLen = uloc_getDisplayKeywordValue_nid_postfix(
        locale, keyword, displayLocale, guestPtr, capacity, &guestStatus);
    Require(errno == 37);

    const char* effectiveDisplay = displayLocale ? displayLocale : uloc_getDefault();
    const std::int32_t hostLen = uloc_getDisplayKeywordValue(
        locale, keyword, effectiveDisplay, hostPtr, capacity, &hostStatus);

    Require(guestLen == hostLen);
    Require(guestStatus == static_cast<std::int32_t>(hostStatus));
    Require(guestDest == hostDest);
}

void CheckExpected(const char* locale, const char* keyword, const char* displayLocale,
                   const std::u16string& expected) {
    std::int32_t status = U_ZERO_ERROR;
    const std::int32_t preflight = uloc_getDisplayKeywordValue_nid_postfix(
        locale, keyword, displayLocale, nullptr, 0, &status);
    Require(preflight == static_cast<std::int32_t>(expected.size()));
    Require(status == (expected.empty() ? U_STRING_NOT_TERMINATED_WARNING : U_BUFFER_OVERFLOW_ERROR));

    std::vector<char16_t> dest(expected.size() + 3, Sentinel);
    status = U_ZERO_ERROR;
    const std::int32_t length = uloc_getDisplayKeywordValue_nid_postfix(
        locale, keyword, displayLocale, dest.data() + 1, static_cast<std::int32_t>(expected.size()) + 1, &status);
    Require(length == static_cast<std::int32_t>(expected.size()));
    Require(status == U_USING_DEFAULT_WARNING);
    Require(dest[0] == Sentinel);
    Require(std::equal(expected.begin(), expected.end(), dest.begin() + 1));
    Require(dest[expected.size() + 1] == u'\0');
    Require(dest[expected.size() + 2] == Sentinel);
}

void DifferentialMatrix() {
    const std::array<const char*, 9> locales{
        nullptr,
        "",
        "en_US",
        "fr_FR@calendar=gregorian",
        "de_DE@collation=PHONEBOOK;currency=EUR",
        "de-DE-u-co-phonebk-ca-gregory",
        "ja_JP@calendar=japanese;numbers=jpan",
        "en_US@currency=USD",
        "en_US@malformed"
    };
    const std::array<const char*, 7> keywords{
        nullptr,
        "",
        "calendar",
        "collation",
        "currency",
        "CURRENCY",
        "numbers"
    };
    const std::array<const char*, 3> displays{nullptr, "en_US", "fr_FR"};
    const std::array<std::int32_t, 6> capacities{-5, 0, 1, 3, 9, 32};
    const std::array<std::int32_t, 3> statuses{U_ZERO_ERROR, U_USING_FALLBACK_WARNING, U_ILLEGAL_ARGUMENT_ERROR};

    for (const char* locale : locales) {
        for (const char* keyword : keywords) {
            for (const char* display : displays) {
                for (const std::int32_t capacity : capacities) {
                    for (const std::int32_t status : statuses) {
                        CheckDifferential(locale, keyword, display, capacity, status);
                    }
                }
            }
        }
    }
}

void ExpectedValuesAndEdgeCases() {
    CheckExpected("fr_FR@calendar=gregorian", "calendar", "en_US", u"gregorian");
    CheckExpected("de_DE@collation=PHONEBOOK;currency=EUR", "collation", "en_US", u"PHONEBOOK");
    CheckExpected("de_DE@collation=PHONEBOOK;currency=EUR", "currency", "en_US", u"EUR");
    CheckExpected("de_DE@collation=PHONEBOOK;currency=EUR", "CURRENCY", nullptr, u"EUR");
    CheckExpected("de-DE-u-co-phonebk", "co", "en_US", u"phonebk");
    CheckExpected("de_DE@collation=phonebook", "collation", "en_US", u"phonebook");
    CheckExpected("en_US", "calendar", "en_US", u"");

    Require(uloc_getDisplayKeywordValue_nid_postfix("fr_FR@calendar=gregorian", "calendar", "en_US", nullptr, 0, nullptr) == 0);

    std::int32_t status = U_ZERO_ERROR;
    Require(uloc_getDisplayKeywordValue_nid_postfix("fr_FR@calendar=gregorian", "calendar", "en_US", nullptr, 4, &status) == 0);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR);

    status = U_ZERO_ERROR;
    char16_t exact[11];
    std::fill_n(exact, 11, Sentinel);
    const std::int32_t exactLen = uloc_getDisplayKeywordValue_nid_postfix(
        "fr_FR@calendar=gregorian", "calendar", "en_US", exact + 1, 9, &status);
    Require(exactLen == 9);
    Require(status == U_STRING_NOT_TERMINATED_WARNING);
    Require(exact[0] == Sentinel && exact[10] == Sentinel);
    Require(std::u16string(exact + 1, 9) == u"gregorian");

    status = U_ZERO_ERROR;
    char16_t currencyOverflow[6];
    std::fill_n(currencyOverflow, 6, Sentinel);
    const std::int32_t currLen = uloc_getDisplayKeywordValue_nid_postfix(
        "en_US@currency=EUR", "currency", "en_US", currencyOverflow + 1, 2, &status);
    Require(currLen == 3);
    Require(status == U_BUFFER_OVERFLOW_ERROR);
    for (const char16_t unit : currencyOverflow) Require(unit == Sentinel);

    status = U_ZERO_ERROR;
    char16_t largeCapDest[12];
    std::fill_n(largeCapDest, 12, Sentinel);
    const std::int32_t hugeLen = uloc_getDisplayKeywordValue_nid_postfix(
        "fr_FR@calendar=gregorian", "calendar", "en_US",
        largeCapDest + 1, std::numeric_limits<std::int32_t>::max(), &status);
    Require(hugeLen == 9);
    Require(status == U_USING_DEFAULT_WARNING);
    Require(largeCapDest[0] == Sentinel);
    Require(std::u16string(largeCapDest + 1) == u"gregorian");
    Require(largeCapDest[11] == Sentinel);
}

void GuestMemorySafety() {
    Mapping mapping;
    const auto* badAddress = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    auto* badStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    auto* badDest = reinterpret_cast<char16_t*>(std::uintptr_t{0x10});

    std::int32_t status = U_ZERO_ERROR;
    char16_t dest[16]{};

    RequireInvalidArgument([&] {
        uloc_getDisplayKeywordValue_nid_postfix("fr_FR@calendar=gregorian", "calendar", "en_US", dest, 16, badStatus);
    }, 41);

    RequireInvalidArgument([&] {
        uloc_getDisplayKeywordValue_nid_postfix(badAddress, "calendar", "en_US", dest, 16, &status);
    }, 42);
    Require(status == U_ZERO_ERROR);

    RequireInvalidArgument([&] {
        uloc_getDisplayKeywordValue_nid_postfix("fr_FR@calendar=gregorian", badAddress, "en_US", dest, 16, &status);
    }, 43);
    Require(status == U_ZERO_ERROR);

    RequireInvalidArgument([&] {
        uloc_getDisplayKeywordValue_nid_postfix("fr_FR@calendar=gregorian", "calendar", badAddress, dest, 16, &status);
    }, 44);
    Require(status == U_ZERO_ERROR);

    RequireInvalidArgument([&] {
        uloc_getDisplayKeywordValue_nid_postfix("fr_FR@calendar=gregorian", "calendar", "en_US", badDest, 16, &status);
    }, 45);
    Require(status == U_ZERO_ERROR);

    mapping.Protect(0, MappingBytes, 3);
    {
        GuestArena::HostWrite access(mapping.data, MappingBytes);
        Require(access.Open());
        const char* str = "fr_FR@calendar=gregorian";
        std::memcpy(mapping.data + Page - 5, str, 5);
        std::memcpy(mapping.data + Page, str + 5, std::strlen(str) - 5 + 1);
    }
    status = U_ZERO_ERROR;
    const std::int32_t splitLen = uloc_getDisplayKeywordValue_nid_postfix(
        reinterpret_cast<const char*>(mapping.data + Page - 5), "calendar", "en_US", dest, 16, &status);
    Require(splitLen == 9 && status == U_USING_DEFAULT_WARNING);
    Require(std::u16string(dest) == u"gregorian");

    mapping.Protect(Page, Page, 0);
    status = U_ZERO_ERROR;
    RequireInvalidArgument([&] {
        uloc_getDisplayKeywordValue_nid_postfix(
            reinterpret_cast<const char*>(mapping.data + Page - 5), "calendar", "en_US", dest, 16, &status);
    }, 46);
    Require(status == U_ZERO_ERROR);

    mapping.Protect(0, MappingBytes, 3);
    {
        GuestArena::HostWrite access(mapping.data, Page);
        Require(access.Open());
        *reinterpret_cast<std::int32_t*>(mapping.data) = U_ZERO_ERROR;
    }
    mapping.Protect(0, Page, 1);
    RequireInvalidArgument([&] {
        uloc_getDisplayKeywordValue_nid_postfix(
            "fr_FR@calendar=gregorian", "calendar", "en_US",
            reinterpret_cast<char16_t*>(mapping.data + 16), 16, &status);
    }, 47);
    RequireInvalidArgument([&] {
        uloc_getDisplayKeywordValue_nid_postfix(
            "fr_FR@calendar=gregorian", "calendar", "en_US",
            dest, 16, reinterpret_cast<std::int32_t*>(mapping.data));
    }, 48);

    std::int32_t failingStatus = U_ILLEGAL_ARGUMENT_ERROR;
    {
        mapping.Protect(0, Page, 3);
        GuestArena::HostWrite access(mapping.data, Page);
        Require(access.Open());
        *reinterpret_cast<std::int32_t*>(mapping.data) = failingStatus;
    }
    mapping.Protect(0, Page, 1);
    errno = 49;
    Require(uloc_getDisplayKeywordValue_nid_postfix(
                badAddress, badAddress, badAddress, badDest, -1,
                reinterpret_cast<std::int32_t*>(mapping.data)) == 0);
    Require(errno == 49);
}

} // namespace

int main() {
    DifferentialMatrix();
    ExpectedValuesAndEdgeCases();
    GuestMemorySafety();
    return 0;
}
