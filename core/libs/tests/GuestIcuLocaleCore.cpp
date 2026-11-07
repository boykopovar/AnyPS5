#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string_view>

namespace {

using LocaleFunction = std::int32_t (APS5_VABI*)(const char*, char*, std::int32_t, std::int32_t*);

struct Case {
    const char* locale;
    const char* expected;
};

void Check(LocaleFunction function, bool wholeOnly, std::initializer_list<Case> cases) {
    for (const auto& item : cases) {
        const std::string_view expected(item.expected);
        for (const std::int32_t capacity : {0, 1, 2, 3, 4, 5, 8, 16, 64}) {
            std::array<char, 80> buffer{};
            buffer.fill('Q');
            std::int32_t status = 0;
            const auto length = function(item.locale, buffer.data(), capacity, &status);
            assert(length == static_cast<std::int32_t>(expected.size()));
            assert(status == (capacity < length ? 15 : capacity == length ? -124 : 0));
            const auto copied = wholeOnly && capacity < length ? 0 :
                                std::min(capacity, length);
            for (std::int32_t index = 0; index < copied; ++index)
                assert(buffer[index] == expected[static_cast<std::size_t>(index)]);
            if (capacity > length) assert(buffer[length] == '\0');
            for (std::size_t index = static_cast<std::size_t>(
                     capacity > length ? length + 1 : copied); index < buffer.size(); ++index) {
                assert(buffer[index] == 'Q');
            }
        }
    }
}

void CheckKeyword(const char* locale, const char* keyword, const char* expected,
                  std::int32_t expectedStatus = 0) {
    for (const std::int32_t capacity : {0, 1, 2, 3, 4, 5, 8, 64}) {
        std::array<char, 80> buffer{};
        buffer.fill('Q');
        std::int32_t status = 0;
        const auto length = uloc_getKeywordValue(locale, keyword, buffer.data(), capacity, &status);
        if (expectedStatus) {
            assert(length == 0 && status == expectedStatus);
            assert(buffer[0] == 'Q');
            continue;
        }
        const auto required = static_cast<std::int32_t>(std::strlen(expected));
        assert(length == required);
        assert(status == (capacity < required ? 15 : capacity == required ? -124 : 0));
        for (std::int32_t index = 0; index < std::min(capacity, required); ++index)
            assert(buffer[index] == expected[index]);
        if (capacity > required) assert(buffer[required] == '\0');
    }
}

}

int main() {
    Check(uloc_getLanguage, true, {
        {"en", "en"}, {"EN-us", "en"}, {"sr_Latn_RS@currency=EUR", "sr"},
        {"root", ""}, {"und", ""}, {"_US", ""}, {"i-klingon", "i-klingon"},
        {"x-private", "x-private"}, {"zh_Hans_CN", "zh"},
    });
    Check(uloc_getCountry, true, {
        {"en", ""}, {"en_US", "US"}, {"en-US", "US"}, {"zh_Hant_TW", "TW"},
        {"_US", "US"}, {"en_419", "419"}, {"en_001", "001"},
        {"en__POSIX", ""}, {"sr_Latn_RS@currency=EUR", "RS"},
    });
    Check(uloc_getScript, true, {
        {"en_US", ""}, {"zh_Hant_TW", "Hant"}, {"eN_LatN_us", "Latn"},
        {"sr_Latn_RS@currency=EUR", "Latn"}, {"_US", ""},
    });
    Check(uloc_getBaseName, false, {
        {"en-US", "en_US"}, {"en_US_POSIX", "en_US_POSIX"},
        {"zh_Hant_TW", "zh_Hant_TW"}, {"en__POSIX", "en__POSIX"},
        {"fr_FR.utf8", "fr_FR.utf8"}, {"sr_Latn_RS@currency=EUR", "sr_Latn_RS"},
        {"i-klingon", "tlh"}, {"x-private", ""}, {"root", ""}, {"und", ""},
    });
    Check(uloc_canonicalize, false, {
        {"EN-us", "en_US"}, {"eN_LatN_us", "en_Latn_US"},
        {"fr_FR.utf8", "fr_FR"}, {"i-klingon", "tlh"},
        {"x-private", "@x=private"}, {"root", ""},
        {"de_DE@currency=EUR;collation=phonebook", "de_DE@collation=phonebook;currency=EUR"},
        {"fr@collation=phonebook", "fr@collation=phonebook"},
    });
    CheckKeyword("en_US@currency=USD", "currency", "USD");
    CheckKeyword("de_DE@currency=EUR;collation=phonebook", "collation", "phonebook");
    CheckKeyword("en@Currency=USD", "CURRENCY", "USD");
    CheckKeyword("en@currency=USD;currency=EUR", "currency", "USD");
    CheckKeyword("en@calendar=buddhist", "calendar", "buddhist");
    CheckKeyword("en_US", "currency", "");
    CheckKeyword("en@currency=USD;badstuff", "calendar", "", 1);
    CheckKeyword("en@currency=", "currency", "", 1);

    std::array<char, 16> output{};
    output.fill('Q');
    std::int32_t status = 0;
    assert(uloc_getLanguage(nullptr, output.data(), 16, &status) == 0);
    assert(status == 1 && output[0] == 'Q');
    status = 0;
    assert(uloc_getScript("sr_Latn", nullptr, 8, &status) == 0);
    assert(status == 1);
    status = 0;
    assert(uloc_getCountry("en_US", output.data(), -1, &status) == 0);
    assert(status == 1);
    status = 15;
    assert(uloc_getBaseName("en_US", output.data(), 16, &status) == 0);
    assert(status == 15 && output[0] == 'Q');
}
