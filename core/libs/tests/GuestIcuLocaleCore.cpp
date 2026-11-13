#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
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

void CheckNameError(const char* locale, std::string_view written, std::int32_t expectedStatus) {
    for (const std::int32_t capacity : {0, 1, 2, 3, 4, 64}) {
        std::array<char, 80> buffer{};
        buffer.fill('Q');
        std::int32_t status = 0;
        const auto length = uloc_getName(locale, buffer.data(), capacity, &status);
        assert(length == static_cast<std::int32_t>(written.size()));
        assert(status == expectedStatus);
        const auto copied = std::min(capacity, length);
        for (std::int32_t index = 0; index < copied; ++index)
            assert(buffer[index] == written[static_cast<std::size_t>(index)]);
        for (std::size_t index = static_cast<std::size_t>(copied); index < buffer.size(); ++index)
            assert(buffer[index] == 'Q');
    }
}

std::string Repeat(std::size_t count, const char* item) {
    std::string result;
    for (std::size_t index = 0; index < count; ++index) {
        if (index) result += ';';
        result += item;
        result += static_cast<char>('a' + index % 26);
        result += static_cast<char>('a' + index / 26);
        result += "=v";
    }
    return result;
}

void CheckKeywords(const char* locale, std::initializer_list<const char*> expected) {
    std::int32_t status = 0;
    UEnumeration* keywords = uloc_openKeywords(locale, &status);
    assert(keywords && status == 0);
    assert(uenum_count_nid_postfix(keywords, &status) == static_cast<std::int32_t>(expected.size()));
    for (int pass = 0; pass < 2; ++pass) {
        for (const char* name : expected) {
            std::int32_t length = -1;
            const char* next = uenum_next_nid_postfix(keywords, &length, &status);
            assert(next && std::strcmp(next, name) == 0);
            assert(length == static_cast<std::int32_t>(std::strlen(name)));
        }
        std::int32_t length = -1;
        assert(!uenum_next_nid_postfix(keywords, &length, &status) && length == 0);
        keywords->reset(keywords, &status);
    }
    for (const char* name : expected) {
        std::int32_t length = -1;
        const char16_t* next = uenum_unext_nid_postfix(keywords, &length, &status);
        assert(next && length == static_cast<std::int32_t>(std::strlen(name)));
        for (std::int32_t index = 0; index <= length; ++index)
            assert(next[index] == static_cast<char16_t>(name[index]));
    }
    std::int32_t length = -1;
    assert(!uenum_unext_nid_postfix(keywords, &length, &status) && length == 0);
    assert(status == 0);
    uenum_close_nid_postfix(keywords);
}

void CheckNoKeywords(const char* locale, std::int32_t expectedStatus) {
    std::int32_t status = 0;
    assert(!uloc_openKeywords(locale, &status));
    assert(status == expectedStatus);
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

    Check(uloc_getName, false, {
        {"en-US", "en_US"}, {"zh_Hant_TW", "zh_Hant_TW"}, {"fr_FR.utf8", "fr_FR.utf8"},
        {"sr_Latn_RS@currency=EUR", "sr_Latn_RS@currency=EUR"},
        {"de_DE@currency=EUR;collation=phonebook", "de_DE@collation=phonebook;currency=EUR"},
        {"en@ Currency = USD ; calendar=buddhist ", "en@calendar=buddhist;currency=USD"},
        {"en@C u R=USD", "en@cur=USD"}, {"en@currency=USD;currency=EUR", "en@currency=USD"},
        {"en@currency=USD;", "en@currency=USD"}, {"en@currency=USD; ", "en@currency=USD"},
        {"en_US@posix", "en_US@posix"}, {"en@", "en@"}, {"en@a;b=c", "en"},
        {"fr_FR.utf8@currency=EUR", "fr_FR.utf8@currency=EUR"}, {"root", ""},
    });
    CheckNameError("en@currency=USD;badstuff", "en@", 3);
    CheckNameError("en@currency=", "en@", 3);
    CheckNameError("en@currency= ;calendar=x", "en@", 3);
    CheckNameError("en@=USD", "en@", 3);
    CheckNameError("en@ =USD", "en@", 3);
    CheckNameError("en@abcdefghijklmnopqrstuvwxy=1", "en@", 5);
    CheckNameError(("en@" + Repeat(26, "k")).c_str(), "en@", 5);
    const std::string longKeyword = "en@abcdefghijklmnopqrstuvwx=1";
    std::array<char, 128> name{};
    status = 0;
    assert(uloc_getName(longKeyword.c_str(), name.data(), 128, &status) == 29 && status == 0);
    assert(std::string_view(name.data()) == longKeyword);
    const std::string manyKeywords = "en@" + Repeat(25, "k");
    std::array<char, 256> many{};
    status = 0;
    assert(uloc_getName(manyKeywords.c_str(), many.data(), 256, &status) ==
           static_cast<std::int32_t>(manyKeywords.size()));
    assert(status == 0);
    status = 0;
    assert(uloc_getName("en_US", output.data(), -1, &status) == 5 && status == 15);
    status = 0;
    assert(uloc_getName("en_US", nullptr, 0, &status) == 5 && status == 15);
    status = 0;
    assert(uloc_getName("root", nullptr, 0, &status) == 0 && status == -124);
    status = -124;
    output.fill('Q');
    assert(uloc_getName("en", output.data(), 16, &status) == 2 && status == 0);
    assert(std::string_view(output.data()) == "en");
    status = 1;
    output.fill('Q');
    assert(uloc_getName("en_US", output.data(), 16, &status) == 0 && status == 1 && output[0] == 'Q');

    CheckKeywords("de_DE@currency=EUR;collation=phonebook", {"collation", "currency"});
    CheckKeywords("en@ Currency = USD ; calendar=buddhist ;currency=EUR", {"calendar", "currency"});
    CheckKeywords("en@", {});
    CheckKeywords("en@  ", {});
    CheckNoKeywords("en_US", 0);
    CheckNoKeywords("en@foo", 3);
    CheckNoKeywords("en@a;b=c", 3);
    CheckNoKeywords("en@currency=", 3);
    CheckNoKeywords(("en@" + Repeat(26, "k")).c_str(), 5);
    status = 1;
    assert(!uloc_openKeywords("en@currency=USD", &status) && status == 1);
    assert(!uloc_openKeywords("en@currency=USD", nullptr));
}
