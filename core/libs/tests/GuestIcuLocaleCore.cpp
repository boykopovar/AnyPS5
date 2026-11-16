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
#include <stdexcept>

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
        {"x-private", "x-private"}, {"zh_Hans_CN", "zh"}, {"eng_USA", "en"},
        {"DEU", "de"}, {"zho_Hant_TWN", "zh"}, {"tlh", "tlh"}, {"xyz_US", "xyz"},
    });
    Check(uloc_getCountry, true, {
        {"en", ""}, {"en_US", "US"}, {"en-US", "US"}, {"zh_Hant_TW", "TW"},
        {"_US", "US"}, {"en_419", "419"}, {"en_001", "001"},
        {"en__POSIX", ""}, {"sr_Latn_RS@currency=EUR", "RS"}, {"eng_USA", "US"},
        {"de-deu", "DE"}, {"zho_Hant_TWN", "TW"}, {"sr_Latn_SRB", "RS"}, {"en_XYZ", "XYZ"},
        {"en_XEA", "XEA"}, {"en_a1", "A1"}, {"en_12", "12"}, {"en_1ab", "1AB"},
        {"en__US", ""}, {"en_Latn__US", ""},
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
        {"eng_USA_POSIX", "en_US_POSIX"}, {"fra-can", "fr_CA"},
        {"en_a1_POSIX", "en_A1_POSIX"}, {"en__US", "en__US"},
    });
    Check(uloc_canonicalize, false, {
        {"EN-us", "en_US"}, {"eN_LatN_us", "en_Latn_US"},
        {"fr_FR.utf8", "fr_FR"}, {"i-klingon", "tlh"},
        {"x-private", "@x=private"}, {"root", ""},
        {"de_DE@currency=EUR;collation=phonebook", "de_DE@collation=phonebook;currency=EUR"},
        {"fr@collation=phonebook", "fr@collation=phonebook"},
        {"deu_DEU@currency=EUR", "de_DE@currency=EUR"},
        {"en-12", "en_12"}, {"en__US", "en__US"},
    });
    CheckKeyword("en_US@currency=USD", "currency", "USD");
    CheckKeyword("de_DE@currency=EUR;collation=phonebook", "collation", "phonebook");
    CheckKeyword("en@Currency=USD", "CURRENCY", "USD");
    CheckKeyword("en@currency=USD;currency=EUR", "currency", "USD");
    CheckKeyword("en@calendar=buddhist", "calendar", "buddhist");
    CheckKeyword("en_US", "currency", "");
    CheckKeyword("en@currency=USD;badstuff", "calendar", "", 1);
    CheckKeyword("en@currency=", "currency", "", 1);
    CheckKeyword("en@ currency = USD ", "currency", "USD");
    CheckKeyword("en@currency=USD; calendar = buddhist ", "calendar", "buddhist");
    CheckKeyword("en@currency=a+b/c_d-e", "currency", "a+b/c_d-e");
    CheckKeyword("en@cur.rency=USD;calendar=buddhist", "calendar", "", 1);
    CheckKeyword("en@badstuff;currency=USD", "currency", "", 1);
    CheckKeyword("en@currency=USD", "cur-rency", "", 1);
    CheckKeyword("en_US@currency=USD", "cur rency", "", 1);
    CheckKeyword("en@currency= ", "currency", "", 1);
    CheckKeyword("en@currency=USD;", "calendar", "", 1);
    CheckKeyword("en@", "currency", "", 1);
    CheckKeyword("en@currency=.US", "currency", "", 1);
    CheckKeyword("en@currency= .S", "currency", "", 1);
    CheckKeyword("en-x-foo", "x", "foo");
    CheckKeyword("EN-X-FOO", "X", "foo");
    CheckKeyword("de-DE-1901-x-ab", "x", "ab");
    CheckKeyword("en-a-bbb-x-foo-bar", "a", "bbb");
    CheckKeyword("en-a-bbb-x-foo-bar", "x", "foo-bar");
    CheckKeyword("und-x-priv", "x", "priv");
    CheckKeyword("en-u-attr2-attr1", "attribute", "attr1-attr2");
    CheckKeyword("en-x-foo", "calendar", "");
    CheckKeyword("C", "currency", "");
    CheckKeyword("en-x", "x", "");
    CheckKeyword("en_x_foo", "x", "");
    CheckKeyword("en-u-ca-buddhist", "cur-rency", "", 1);
    CheckKeyword("en-u-abcdefg1-abcdefg2-abcdefg3-abcdefg4-abcdefg5-abcdefg6-abcdefg7-abcdefg8-abcdefg9-abcdefga-abcdefgb-abcdefgc",
                 "attribute", "", 1);

    std::array<char, 16> output{};
    output.fill('Q');
    std::int32_t status = 0;
    for (const LocaleFunction function : std::array<LocaleFunction, 5>{
             uloc_getLanguage, uloc_getCountry, uloc_getScript, uloc_getBaseName, uloc_canonicalize}) {
        status = 0;
        bool thrown = false;
        try {
            function(nullptr, output.data(), 16, &status);
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        assert(thrown && status == 0 && output[0] == 'Q');
        status = 15;
        assert(function(nullptr, output.data(), 16, &status) == 0);
        assert(status == 15 && output[0] == 'Q');
    }
    status = 0;
    bool keywordThrown = false;
    try {
        uloc_getKeywordValue("en-u-ca-buddhist", "calendar", output.data(), 16, &status);
    } catch (const std::runtime_error&) {
        keywordThrown = true;
    }
    assert(keywordThrown && status == 0 && output[0] == 'Q');
    status = 15;
    assert(uloc_getKeywordValue("en-u-ca-buddhist", "calendar", output.data(), 16, &status) == 0);
    assert(status == 15 && output[0] == 'Q');
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
