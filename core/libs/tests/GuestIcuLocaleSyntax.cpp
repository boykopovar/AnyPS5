#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "ICU locale syntax check failed at line %u\n", location.line());
    std::abort();
}

template<class Exception, class Function>
void RequireThrow(Function function) {
    bool thrown = false;
    try {
        function();
    } catch (const Exception&) {
        thrown = true;
    }
    Require(thrown);
}

void CheckVariant() {
    struct Case {
        const char* locale;
        const char* expected;
    };
    constexpr Case cases[] = {
        {"en_US_POSIX", "POSIX"},
        {"en_US_posix", "POSIX"},
        {"de_DE", ""},
        {"de_DE@collation=phonebook", ""},
        {"", ""},
        {"en-US-u-va-posIX", "POSIX"},
        {"en-US-u-co-search-va-posIX-kc", "POSIX"},
        {"en-US-fonIPA-u-va-posIX", "FONIPA_POSIX"}
    };
    for (const auto& item : cases) {
        const std::string_view expected(item.expected);
        const auto expectedLength = static_cast<std::int32_t>(expected.size());
        for (std::int32_t capacity = -1; capacity <= expectedLength + 2; ++capacity) {
            std::array<char, 40> buffer;
            buffer.fill('Q');
            std::int32_t status = 0;
            Require(uloc_getVariant_nid_postfix(item.locale, buffer.data(), capacity, &status) == expectedLength);
            Require(status == (capacity < expectedLength ? 15 : capacity == expectedLength ? -124 : 0));
            const auto copied = std::min(std::max(capacity, 0), expectedLength);
            Require(std::equal(expected.begin(), expected.begin() + copied, buffer.begin()));
            if (capacity > expectedLength) Require(buffer[expectedLength] == '\0');
            const auto untouched = capacity > expectedLength ? expectedLength + 1 : copied;
            Require(std::all_of(buffer.begin() + untouched, buffer.end(), [](char value) { return value == 'Q'; }));
        }
    }
    std::array<char, 32> buffer;
    buffer.fill('Q');
    std::int32_t status = 0;
    Require(uloc_getVariant_nid_postfix("en_US_POSIX", nullptr, 0, &status) == 5 && status == 15);
    status = 0;
    Require(uloc_getVariant_nid_postfix("en_US", nullptr, 0, &status) == 0 && status == -124);
    status = -127;
    Require(uloc_getVariant_nid_postfix("en_US_POSIX", buffer.data(), 32, &status) == 5 && status == -127);
    status = -124;
    Require(uloc_getVariant_nid_postfix("en_US_POSIX", buffer.data(), 32, &status) == 5 && status == 0);
    buffer.fill('Q');
    status = 7;
    Require(uloc_getVariant_nid_postfix(nullptr, nullptr, 32, &status) == 0 && status == 7);
    Require(uloc_getVariant_nid_postfix("en_US_POSIX", buffer.data(), 32, &status) == 0 && status == 7);
    Require(std::all_of(buffer.begin(), buffer.end(), [](char value) { return value == 'Q'; }));
    RequireThrow<std::invalid_argument>([&] { uloc_getVariant_nid_postfix("en_US", buffer.data(), 32, nullptr); });
    status = 0;
    RequireThrow<std::runtime_error>([&] { uloc_getVariant_nid_postfix(nullptr, buffer.data(), 32, &status); });
    RequireThrow<std::invalid_argument>([&] { uloc_getVariant_nid_postfix("en_US", nullptr, 1, &status); });
}

void CheckKeywordUpdates() {
    struct Case {
        const char* initial;
        const char* keyword;
        const char* value;
        const char* expected;
    };
    constexpr Case cases[] = {
        {"en_US", "calendar", "japanese", "en_US@calendar=japanese"},
        {"en_US@", "calendar", "japanese", "en_US@calendar=japanese"},
        {"en_US@calendar=islamic", "calendar", "japanese", "en_US@calendar=japanese"},
        {"en_US@calendar=gregorian", "calendar", "japanese", "en_US@calendar=japanese"},
        {"de", "Currency", "CHF", "de@currency=CHF"},
        {"en_US@collation=phonebook", "calendar", "japanese", "en_US@calendar=japanese;collation=phonebook"},
        {"de@collation=phonebook;currency=CHF", "currency", "", "de@collation=phonebook"},
        {"de@collation=phonebook;currency=CHF", "currency", nullptr, "de@collation=phonebook"},
        {"de@collation=phonebook;currency=CHF", "collation", nullptr, "de@currency=CHF"},
        {"de@collation=phonebook;currency=CHF", "calendar", nullptr, "de@collation=phonebook;currency=CHF"},
        {"de@collation=phonebook", "collation", nullptr, "de"},
        {"en_US", "calendar", nullptr, "en_US"},
        {"en_US", "calendar", "", "en_US"},
        {"en_US@ calendar = islamic", "calendar", "japanese", "en_US@calendar=japanese"}
    };
    for (const auto& item : cases) {
        const auto initialLength = static_cast<std::int32_t>(std::strlen(item.initial));
        const auto expectedLength = static_cast<std::int32_t>(std::strlen(item.expected));
        for (std::int32_t capacity = 0; capacity <= std::max(initialLength, expectedLength) + 2; ++capacity) {
            std::array<char, 128> buffer;
            buffer.fill('Q');
            std::memcpy(buffer.data(), item.initial, initialLength + 1);
            const auto before = buffer;
            std::int32_t status = 0;
            const auto length = uloc_setKeywordValue_nid_postfix(item.keyword, item.value, buffer.data(), capacity, &status);
            if (capacity <= 1 || capacity < initialLength) {
                Require(length == 0 && status == 1 && buffer == before);
            } else if (capacity == expectedLength && std::strchr(item.initial, '@') == nullptr && (!item.value || !*item.value)) {
                Require(length == expectedLength && status == -124 && buffer == before);
            } else if (capacity <= expectedLength) {
                Require(length == expectedLength && status == 15 && buffer == before);
            } else {
                Require(length == expectedLength && status == 0);
                Require(std::strcmp(buffer.data(), item.expected) == 0);
                const auto untouched = std::max(initialLength, expectedLength) + 1;
                Require(std::equal(buffer.begin() + untouched, buffer.end(), before.begin() + untouched));
            }
        }
    }
    for (const auto warning : {-127, -124}) {
        std::array<char, 64> buffer{};
        std::strcpy(buffer.data(), "de");
        std::int32_t status = warning;
        Require(uloc_setKeywordValue_nid_postfix("Currency", "CHF", buffer.data(), 64, &status) == 15);
        Require(std::strcmp(buffer.data(), "de@currency=CHF") == 0 && status == (warning == -124 ? 0 : warning));
    }
}

void CheckKeywordErrors() {
    struct Case {
        const char* initial;
        const char* keyword;
        const char* value;
    };
    constexpr Case cases[] = {
        {"en_US", nullptr, "japanese"},
        {"en_US", "", "japanese"},
        {"en_US@calendar=gregorian", "cale ndar", "japanese"},
        {"en_US@calendar=gregorian", "calendar", "japa..nese"},
        {"en_US@calendar=gregorian;", "calendar", "japanese"},
        {"en_US@calendar=gregorian;currency=", "calendar", "japanese"},
        {"en_US@=", "calendar", "japanese"},
        {"en_US@=;calendar=gregorian", "calendar", nullptr}
    };
    for (const auto& item : cases) {
        std::array<char, 96> buffer;
        buffer.fill('Q');
        std::strcpy(buffer.data(), item.initial);
        const auto before = buffer;
        std::int32_t status = 0;
        Require(uloc_setKeywordValue_nid_postfix(item.keyword, item.value, buffer.data(), 96, &status) == 0);
        Require(status == 1 && buffer == before);
    }
    std::int32_t status = 0;
    Require(uloc_setKeywordValue_nid_postfix(nullptr, "japanese", nullptr, 32, &status) == 0 && status == 1);
    status = 0;
    Require(uloc_setKeywordValue_nid_postfix("", "japanese", nullptr, 32, &status) == 0 && status == 1);
    for (const std::int32_t capacity : {-1, 0, 1}) {
        status = 0;
        Require(uloc_setKeywordValue_nid_postfix("calendar", "japanese", nullptr, capacity, &status) == 0 && status == 1);
    }
    status = 7;
    Require(uloc_setKeywordValue_nid_postfix(nullptr, nullptr, nullptr, 32, &status) == 0 && status == 7);
    std::array<char, 32> buffer{};
    std::strcpy(buffer.data(), "en_US");
    const auto before = buffer;
    Require(uloc_setKeywordValue_nid_postfix("calendar", "japanese", buffer.data(), 32, &status) == 0);
    Require(status == 7 && buffer == before);
    RequireThrow<std::invalid_argument>([&] { uloc_setKeywordValue_nid_postfix("calendar", "japanese", buffer.data(), 32, nullptr); });
    status = 0;
    RequireThrow<std::invalid_argument>([&] { uloc_setKeywordValue_nid_postfix("calendar", "japanese", nullptr, 32, &status); });
}

}

int main() {
    CheckVariant();
    CheckKeywordUpdates();
    CheckKeywordErrors();
}
