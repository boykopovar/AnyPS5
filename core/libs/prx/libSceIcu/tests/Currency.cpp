#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/CurrencyOracle.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr double CurrentDate = 1700000000000.0;
constexpr std::uint32_t AllCurrencies = 0x7fffffff;
static_assert(std::is_same_v<decltype(&ucurr_getNumericCode_nid_postfix), std::int32_t (APS5_VABI*)(const char16_t*)>);
static_assert(std::is_same_v<decltype(&ucurr_isAvailable_nid_postfix), std::int8_t (APS5_VABI*)(const char16_t*, double, double, std::int32_t*)>);

void Require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) throw std::runtime_error("currency assertion at line " + std::to_string(where.line()));
}

template<class T>
void RequireThrows(T&& action) {
    bool threw = false;
    try { action(); }
    catch (const std::runtime_error& error) { threw = std::string_view(error.what()).ends_with(" not implemented"); }
    Require(threw);
}

std::u16string Name(const char16_t* code, std::int32_t style, std::int8_t& choice) {
    std::int32_t status = 0;
    std::int32_t length = -99;
    const auto* value = ucurr_getName_nid_postfix(code, "en", style, &choice, &length, &status);
    Require(value && length >= 0 && status <= 0);
    return {value, static_cast<std::size_t>(length)};
}

std::vector<std::string> Values(UEnumeration* enumeration) {
    Require(enumeration);
    std::int32_t status = 0;
    const auto count = uenum_count_nid_postfix(enumeration, &status);
    Require(status <= 0 && count >= 0);
    std::vector<std::string> result;
    for (;;) {
        std::int32_t length = -99;
        const auto* value = uenum_next_nid_postfix(enumeration, &length, &status);
        Require(status <= 0);
        if (!value) break;
        Require(length >= 0);
        result.emplace_back(value, static_cast<std::size_t>(length));
        Require(result.size() <= static_cast<std::size_t>(count));
    }
    Require(result.size() == static_cast<std::size_t>(count));
    uenum_close_nid_postfix(enumeration);
    return result;
}

void Api(unsigned operation) {
    std::int32_t status = 0;
    std::int32_t length = -99;
    std::int8_t choice = 73;
    std::array<char16_t, 4> result{};
    switch (operation) {
        case 0:
            Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3);
            Require(std::u16string_view(result.data()) == u"USD" && status == 0);
            break;
        case 1:
            Require(Name(u"USD", 0, choice) == u"$" && choice == 0);
            break;
        case 2: {
            const auto* text = ucurr_getPluralName_nid_postfix(u"USD", "en", &choice, "one", &length, &status);
            Require(text && std::u16string_view(text, length) == u"US dollar" && choice == 73 && status == 0);
            break;
        }
        case 3: Require(ucurr_getDefaultFractionDigits_nid_postfix(u"USD", &status) == 2 && status == 0); break;
        case 4: Require(ucurr_getDefaultFractionDigitsForUsage_nid_postfix(u"JPY", 1, &status) == 0 && status == 0); break;
        case 5: Require(ucurr_getRoundingIncrement_nid_postfix(u"CHF", &status) == 0.0 && status == 0); break;
        case 6: Require(ucurr_getRoundingIncrementForUsage_nid_postfix(u"CHF", 1, &status) == 0.05 && status == 0); break;
        case 7: Require(ucurr_getNumericCode_nid_postfix(u"USD") == 840); break;
        case 8: Require(ucurr_isAvailable_nid_postfix(u"USD", -1e15, 1e15, &status) == 1 && status == 0); break;
        case 9: Require(ucurr_countCurrencies_nid_postfix("en_US", CurrentDate, &status) >= 1 && status == 0); break;
        case 10:
            Require(ucurr_forLocaleAndDate_nid_postfix("en_US", CurrentDate, 1, result.data(), 4, &status) == 3);
            Require(std::u16string_view(result.data()) == u"USD" && status == 0);
            break;
        case 11: Require(Values(ucurr_openISOCurrencies_nid_postfix(AllCurrencies, &status)).size() > 200 && status == 0); break;
        case 12: {
            const auto values = Values(ucurr_getKeywordValuesForLocale_nid_postfix("currency", "en_US", 1, &status));
            Require(std::find(values.begin(), values.end(), "USD") != values.end() && status == 0);
            break;
        }
        case 13: {
            const auto key = ucurr_register_nid_postfix(u"JPY", "en_US", &status);
            Require(key && status == 0);
            Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"JPY");
            Require(ucurr_unregister_nid_postfix(key, &status) == 1 && status == 0);
            break;
        }
        case 14: Require(ucurr_unregister_nid_postfix(nullptr, &status) == 0 && status == 0); break;
        default: Require(false);
    }
}

void Metadata() {
    Api(3);
    for (const auto& item : {std::pair{u"JPY", 0}, {u"USD", 2}, {u"TND", 3}, {u"CLF", 4}}) {
        std::int32_t status = 0;
        Require(ucurr_getDefaultFractionDigits_nid_postfix(item.first, &status) == item.second && status == 0);
    }
    std::int32_t status = 0;
    Require(ucurr_getDefaultFractionDigits_nid_postfix(nullptr, &status) == 2 && status == 1);
    status = 0;
    Require(ucurr_getDefaultFractionDigits_nid_postfix(u"jpy", &status) == 2 && status == 0);
    Require(ucurr_getNumericCode_nid_postfix(u"jpy") == 392);
    Require(ucurr_getNumericCode_nid_postfix(nullptr) == 0 && ucurr_getNumericCode_nid_postfix(u"US") == 0);
    status = 0;
    Require(ucurr_getDefaultFractionDigitsForUsage_nid_postfix(reinterpret_cast<const char16_t*>(1), 9, &status) == 0 && status == 16);
    status = 0;
    Require(ucurr_getRoundingIncrementForUsage_nid_postfix(nullptr, 9, &status) == 0.0 && status == 1);
    for (unsigned operation : {4, 5, 6, 7}) Api(operation);
}

void Locale() {
    Api(0);
    for (std::int32_t capacity : {0, 1, 2, 3, 4, INT32_MAX}) {
        std::array<char16_t, 6> result;
        result.fill(0x5a5a);
        std::int32_t status = 0;
        Require(ucurr_forLocale_nid_postfix("en_US", result.data(), capacity, &status) == 3);
        if (capacity <= 3) {
            Require(status == (capacity == 3 ? -124 : 15));
            Require(std::all_of(result.begin(), result.end(), [](char16_t value) { return value == 0x5a5a; }));
        } else {
            Require(status == 0 && std::u16string_view(result.data()) == u"USD" && result[4] == 0x5a5a && result[5] == 0x5a5a);
        }
    }
    std::array<char16_t, 4> result{};
    std::int32_t status = 0;
    Require(ucurr_forLocale_nid_postfix("en_US@currency=eur", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"EUR");
    status = 0;
    Require(ucurr_forLocale_nid_postfix(reinterpret_cast<const char*>(1), nullptr, 1, &status) == 0 && status == 1);
    status = 1;
    Require(ucurr_forLocale_nid_postfix(reinterpret_cast<const char*>(1), reinterpret_cast<char16_t*>(1), 4, &status) == 0 && status == 1);
}

void Names() {
    Api(1);
    std::int8_t choice = 73;
    Require(Name(u"USD", 1, choice) == u"US Dollar" && choice == 0);
    const char16_t unknown[] = u"ZZZtail";
    std::int32_t length = -99;
    std::int32_t status = 0;
    const auto* found = ucurr_getName_nid_postfix(unknown, "en", 1, &choice, &length, &status);
    Require(found == unknown && length == 7 && status == -127 && choice == 0);
    status = 0;
    choice = 73;
    found = ucurr_getPluralName_nid_postfix(u"USD", "en", &choice, "one", &length, &status);
    Require(found && std::u16string_view(found, length) == u"US dollar" && choice == 73 && status == 0);
    found = ucurr_getPluralName_nid_postfix(u"USD", "en", nullptr, "not-a-plural", &length, &status);
    Require(found && std::u16string_view(found, length) == u"US dollars" && status == 0);
    status = 0;
    length = -99;
    Require(ucurr_getName_nid_postfix(reinterpret_cast<const char16_t*>(1), reinterpret_cast<const char*>(1), -1, reinterpret_cast<std::int8_t*>(1), &length, &status) == nullptr);
    Require(status == 1 && length == -99);
}

void Historical() {
    Api(9);
    Api(10);
    std::int32_t status = 0;
    Require(ucurr_isAvailable_nid_postfix(u"USD", 1e15, -1e15, &status) == 0 && status == 1);
    status = 0;
    Require(ucurr_isAvailable_nid_postfix(u"ZZZ", 1e15, -1e15, &status) == 0 && status == 0);
    Require(ucurr_isAvailable_nid_postfix(nullptr, -1e15, 1e15, &status) == 0 && status == 0);
    for (std::int32_t capacity : {0, 1, 2, 3}) {
        std::array<char16_t, 4> result;
        result.fill(0x5a5a);
        status = 0;
        Require(ucurr_forLocaleAndDate_nid_postfix("en_US", CurrentDate, 1, result.data(), capacity, &status) == 0 && status == 0);
        Require(std::all_of(result.begin(), result.end(), [](char16_t value) { return value == 0x5a5a; }));
    }
    status = 0;
    Require(ucurr_countCurrencies_nid_postfix(reinterpret_cast<const char*>(1), CurrentDate, nullptr) == 0);
    Require(ucurr_forLocaleAndDate_nid_postfix(reinterpret_cast<const char*>(1), CurrentDate, 0, nullptr, 4, &status) == 0 && status == 1);
}

void Enumeration() {
    Api(11);
    std::int32_t status = 7;
    auto* enumeration = ucurr_openISOCurrencies_nid_postfix(AllCurrencies, &status);
    Require(enumeration && status == 7);
    Require(Values(enumeration).size() > 200);
    Require(Values(ucurr_openISOCurrencies_nid_postfix(AllCurrencies, nullptr)).size() > 200);
    status = 0;
    const auto values = Values(ucurr_getKeywordValuesForLocale_nid_postfix(reinterpret_cast<const char*>(1), "en_US", 1, &status));
    Require(std::find(values.begin(), values.end(), "USD") != values.end() && status == 0);
}

void MissingHistoricalDate(bool from) {
    const char* locale = from ? "en_US" : "zh_CN";
    const std::u16string_view expected = from ? u"USN" : u"CNH";
    std::array<char16_t, 6> result;
    result.fill(0x5a5a);
    std::int32_t status = 0;
    Require(ucurr_forLocaleAndDate_nid_postfix(locale, CurrentDate, 2, result.data(), 4, &status) == 3);
    Require(std::u16string_view(result.data()) == expected && status == 0 && result[4] == 0x5a5a && result[5] == 0x5a5a);
    Require(ucurr_countCurrencies_nid_postfix(locale, CurrentDate, &status) == 2 && status == 0);
}

void Registration() {
    Api(13);
    std::int32_t status = 0;
    const auto first = ucurr_register_nid_postfix(u"JPYignored", "en_US", &status);
    const auto second = ucurr_register_nid_postfix(u"CHF", "en_US", &status);
    Require(first && second && first != second && status == 0);
    std::array<char16_t, 4> result{};
    Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"CHF");
    Require(ucurr_unregister_nid_postfix(second, &status) == 1);
    Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"JPY");
    Require(ucurr_unregister_nid_postfix(first, &status) == 1);
    Require(ucurr_unregister_nid_postfix(first, &status) == 0);
    Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"USD");
    Require(ucurr_unregister_nid_postfix(reinterpret_cast<const void*>(1), &status) == 0 && status == 0);
    status = 1;
    Require(ucurr_register_nid_postfix(reinterpret_cast<const char16_t*>(1), reinterpret_cast<const char*>(1), &status) == nullptr && status == 1);
}

class Guarded {
public:
    unsigned char* data;
    std::size_t size;
    Guarded() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data);
        DWORD previous;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const auto page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        size = static_cast<std::size_t>(page);
        auto* memory = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(memory != MAP_FAILED);
        data = static_cast<unsigned char*>(memory);
        Require(mprotect(data + size, size, PROT_NONE) == 0);
#endif
    }
    ~Guarded() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;
    void ReadOnly() {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(data, size, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, size, PROT_READ) == 0);
#endif
    }
};

void GuardedInputs() {
    Guarded memory;
    auto* code = reinterpret_cast<char16_t*>(memory.data + memory.size - 6);
    std::memcpy(code, u"JPY", 6);
    std::int32_t status = 0;
    Require(ucurr_getDefaultFractionDigits_nid_postfix(code, &status) == 0 && status == 0);
    std::memcpy(code, u"USD", 6);
    std::int32_t length = -99;
    const auto* text = ucurr_getName_nid_postfix(code, "en", 0, nullptr, &length, &status);
    Require(text && std::u16string_view(text, length) == u"$" && status == 0);
    const auto key = ucurr_register_nid_postfix(code, "en_CA", &status);
    Require(key && status == 0 && ucurr_unregister_nid_postfix(key, &status) == 1);
    std::memcpy(code, u"ZZZ", 6);
    RequireThrows([&] { ucurr_getName_nid_postfix(code, "en", 0, nullptr, &length, &status); });
    auto* empty = reinterpret_cast<char16_t*>(memory.data + memory.size - 2);
    *empty = 0;
    status = 0;
    Require(ucurr_getDefaultFractionDigits_nid_postfix(empty, &status) == 2 && status == 1);
    auto* inaccessible = reinterpret_cast<std::int32_t*>(memory.data + memory.size);
    Require(Values(ucurr_openISOCurrencies_nid_postfix(AllCurrencies, inaccessible)).size() > 200);
}

void GuardedOutputs() {
    Guarded memory;
    auto* output = reinterpret_cast<char16_t*>(memory.data + memory.size - 4);
    output[0] = output[1] = 0x5a5a;
    std::int32_t status = 0;
    errno = 73;
    *__error_nid_postfix() = 73;
    RequireThrows([&] { ucurr_forLocale_nid_postfix("en_US", output, 4, &status); });
    Require(output[0] == 0x5a5a && output[1] == 0x5a5a && status == 0 && errno == 73 && *__error_nid_postfix() == 73);
    Require(ucurr_forLocale_nid_postfix("en_US", reinterpret_cast<char16_t*>(1), 3, &status) == 3 && status == -124);
    status = 0;
    std::int8_t choice = 73;
    RequireThrows([&] { ucurr_getName_nid_postfix(u"USD", "en", 0, &choice, reinterpret_cast<std::int32_t*>(1), &status); });
    Require(choice == 73 && status == 0);
    std::int32_t length = -99;
    const auto* text = ucurr_getPluralName_nid_postfix(u"USD", "en", reinterpret_cast<std::int8_t*>(1), "one", &length, &status);
    Require(text && std::u16string_view(text, length) == u"US dollar" && status == 0);
    text = ucurr_getName_nid_postfix(u"USD", "en", 0, nullptr, nullptr, &status);
    Require(text && std::u16string_view(text) == u"$" && status == 0);
    RequireThrows([&] { ucurr_getName_nid_postfix(u"ZZZ", "en", 0, nullptr, nullptr, &status); });
    status = 7;
    Require(ucurr_getDefaultFractionDigits_nid_postfix(reinterpret_cast<const char16_t*>(1), &status) == 0 && status == 7);
    RequireThrows([&] { ucurr_getRoundingIncrement_nid_postfix(reinterpret_cast<const char16_t*>(1), &status); });
    Require(status == 7);
    auto* readOnlyStatus = reinterpret_cast<std::int32_t*>(memory.data);
    *readOnlyStatus = 0;
    memory.ReadOnly();
    RequireThrows([&] { ucurr_register_nid_postfix(u"JPY", "en_US", readOnlyStatus); });
    Require(ucurr_unregister_nid_postfix(nullptr, readOnlyStatus) == 0 && *readOnlyStatus == 0);
    status = 0;
    std::array<char16_t, 4> result{};
    Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"USD");
}

void ScalarOracle() {
    CurrencyOracle::SetDefault(uloc_getDefault());
    std::size_t observations = 0;
    for (const auto* code : {u"USD", u"JPY", u"CHF", u"TND", u"CLF", u"ZZZ", u"jpy", u"JPYignored", u"", static_cast<const char16_t*>(nullptr)}) {
        for (std::int32_t incoming : {-128, -127, -124, -1, 0, 1, 7, 15}) {
            for (std::int32_t usage : {0, 1}) {
                for (unsigned operation = 0; operation < 6; ++operation) {
                    const auto expected = CurrencyOracle::Scalar(operation, code, "en_US", usage, -1e15, 1e15, incoming);
                    auto status = incoming;
                    double actual = 0;
                    switch (operation) {
                        case 0: actual = ucurr_getDefaultFractionDigits_nid_postfix(code, &status); break;
                        case 1: actual = ucurr_getDefaultFractionDigitsForUsage_nid_postfix(code, usage, &status); break;
                        case 2: actual = ucurr_getRoundingIncrement_nid_postfix(code, &status); break;
                        case 3: actual = ucurr_getRoundingIncrementForUsage_nid_postfix(code, usage, &status); break;
                        case 4: actual = ucurr_getNumericCode_nid_postfix(code); break;
                        case 5: actual = ucurr_isAvailable_nid_postfix(code, -1e15, 1e15, &status); break;
                    }
                    Require(actual == expected.value && status == expected.status);
                    ++observations;
                }
            }
        }
    }
    std::fprintf(stderr, "%zu scalar direct-source observations\n", observations);
}

void CodeOracle() {
    CurrencyOracle::SetDefault(uloc_getDefault());
    std::size_t observations = 0;
    for (const auto* locale : {"en_US", "de_DE", "ja_JP", "en_US@currency=eur", "und", "zz_ZZ", "en_US@rg=sezzzz"}) {
        for (std::int32_t incoming : {-128, -127, -124, 0, 1, 7}) {
            for (std::int32_t capacity : {-1, 0, 1, 2, 3, 4, INT32_MAX}) {
                const auto expected = CurrencyOracle::Code(locale, CurrentDate, 1, capacity, incoming, false);
                std::array<char16_t, 8> result;
                result.fill(0x5a5a);
                auto status = incoming;
                const auto length = ucurr_forLocale_nid_postfix(locale, result.data(), capacity, &status);
                Require(length == expected.length && status == expected.status && result == expected.text);
                ++observations;
            }
        }
    }
    std::fprintf(stderr, "%zu locale/capacity direct-source observations\n", observations);
}

void NameOracle() {
    CurrencyOracle::SetDefault(uloc_getDefault());
    std::size_t observations = 0;
    for (const auto* code : {u"USD", u"JPY", u"CHF", u"usd", u"ZZZtail"}) {
        for (const auto* locale : {"en", "fr", "de", "ja", "zz_ZZ"}) {
            for (std::int32_t incoming : {-128, -127, -124, 0, 1}) {
                for (std::int32_t style = 0; style < 5; ++style) {
                    const auto expected = CurrencyOracle::Name(code, locale, style, "other", incoming, false);
                    auto status = incoming;
                    std::int32_t length = -99;
                    std::int8_t choice = 73;
                    const auto* text = ucurr_getName_nid_postfix(code, locale, style, &choice, &length, &status);
                    Require((text == nullptr) == expected.null && (text && text == code) == expected.alias);
                    Require(length == expected.length && choice == expected.choice && status == expected.status);
                    if (text) Require(std::u16string_view(text, length) == expected.text);
                    ++observations;
                }
                for (const auto* plural : {"one", "other", "few", "unknown"}) {
                    const auto expected = CurrencyOracle::Name(code, locale, 1, plural, incoming, true);
                    auto status = incoming;
                    std::int32_t length = -99;
                    std::int8_t choice = 73;
                    const auto* text = ucurr_getPluralName_nid_postfix(code, locale, &choice, plural, &length, &status);
                    Require((text == nullptr) == expected.null && (text && text == code) == expected.alias);
                    Require(length == expected.length && choice == expected.choice && status == expected.status);
                    if (text) Require(std::u16string_view(text, length) == expected.text);
                    ++observations;
                }
            }
        }
    }
    std::fprintf(stderr, "%zu name/plural direct-source observations\n", observations);
}

void EnumerationOracle() {
    CurrencyOracle::SetDefault(uloc_getDefault());
    for (const auto type : {0u, 0x7fffffffu, 1u, 2u, 4u, 8u, 0xffffffffu}) {
        for (bool keywords : {false, true}) {
            const auto expected = CurrencyOracle::Enumeration(type, "en_US", keywords, 0);
            std::int32_t status = 0;
            auto* enumeration = keywords ? ucurr_getKeywordValuesForLocale_nid_postfix("currency", "en_US", 1, &status) : ucurr_openISOCurrencies_nid_postfix(type, &status);
            Require(enumeration && status == expected.status && uenum_count_nid_postfix(enumeration, &status) == expected.count);
            std::vector<std::string> values;
            for (;;) {
                std::int32_t length = -99;
                const auto* value = uenum_next_nid_postfix(enumeration, &length, &status);
                if (!value) { Require(length == expected.endLength); break; }
                values.emplace_back(value, static_cast<std::size_t>(length));
            }
            Require(values == expected.values && status <= 0);
            enumeration->reset(enumeration, &status);
            std::vector<std::u16string> unicode;
            for (;;) {
                std::int32_t length = -99;
                const auto* value = uenum_unext_nid_postfix(enumeration, &length, &status);
                if (!value) { Require(length == expected.unicodeEndLength); break; }
                unicode.emplace_back(value, static_cast<std::size_t>(length));
            }
            Require(unicode == expected.unicode && status <= 0);
            uenum_close_nid_postfix(enumeration);
        }
    }
}

void NullPlural() {
    for (const auto* locale : {"en", "fr_FR", "ru_RU"}) {
        const auto expected = CurrencyOracle::Name(u"USD", locale, 1, nullptr, 0, true);
        std::int32_t status = 0;
        std::int32_t length = -99;
        std::int8_t choice = 73;
        const auto* text = ucurr_getPluralName_nid_postfix(u"USD", locale, &choice, nullptr, &length, &status);
        Require(text && !expected.null && std::u16string_view(text, length) == expected.text
            && status == expected.status && length == expected.length && choice == expected.choice && choice == 73);
    }
}

void KeywordOracle() {
    CurrencyOracle::SetDefault(uloc_getDefault());
    for (const auto* locale : {"en_US", "fr_FR", "zh_CN", "en_QQ", "und", "root"}) {
        for (bool commonlyUsed : {false, true}) {
            for (std::int32_t incoming : {0, -128}) {
                const auto expected = CurrencyOracle::Enumeration(0, locale, true, incoming, commonlyUsed);
                auto status = incoming;
                auto* enumeration = ucurr_getKeywordValuesForLocale_nid_postfix(reinterpret_cast<const char*>(1), locale, commonlyUsed, &status);
                Require(enumeration && status == expected.status && uenum_count_nid_postfix(enumeration, &status) == expected.count);
                std::vector<std::string> values;
                for (;;) {
                    std::int32_t length = -99;
                    const auto* value = uenum_next_nid_postfix(enumeration, &length, &status);
                    if (!value) { Require(length == expected.endLength); break; }
                    values.emplace_back(value, static_cast<std::size_t>(length));
                }
                Require(values == expected.values && status <= 0);
                enumeration->reset(enumeration, &status);
                std::vector<std::u16string> unicode;
                for (;;) {
                    std::int32_t length = -99;
                    const auto* value = uenum_unext_nid_postfix(enumeration, &length, &status);
                    if (!value) { Require(length == expected.unicodeEndLength); break; }
                    unicode.emplace_back(value, static_cast<std::size_t>(length));
                }
                Require(unicode == expected.unicode && status <= 0);
                uenum_close_nid_postfix(enumeration);
            }
        }
    }
}

void EnumerationWriteBounds() {
    Guarded memory;
    auto* readOnly = reinterpret_cast<std::int32_t*>(memory.data);
    *readOnly = 0;
    memory.ReadOnly();
    Require(CurrencyOracle::EnumerationWriteBounds(readOnly, reinterpret_cast<std::int32_t*>(1)));
    std::int32_t status = 0;
    auto* enumeration = ucurr_getKeywordValuesForLocale_nid_postfix("currency", "en_US", 1, &status);
    Require(enumeration && status == 0);
    std::int32_t length = -99;
    auto* value = uenum_next_nid_postfix(enumeration, &length, readOnly);
    Require(value && std::string_view(value, length) == "USD" && *readOnly == 0);
    enumeration->reset(enumeration, &status);
    RequireThrows([&] { uenum_next_nid_postfix(enumeration, readOnly, &status); });
    value = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(value && std::string_view(value, length) == "USD" && status == 0);
    enumeration->reset(enumeration, &status);
    RequireThrows([&] { uenum_unext_nid_postfix(enumeration, readOnly, &status); });
    const auto* unicode = uenum_unext_nid_postfix(enumeration, &length, &status);
    Require(unicode && std::u16string_view(unicode, length) == u"USD" && status == 0);
    while (uenum_next_nid_postfix(enumeration, &length, &status)) {}
    Require(!uenum_next_nid_postfix(enumeration, reinterpret_cast<std::int32_t*>(1), &status) && status == 0);
    RequireThrows([&] { uenum_unext_nid_postfix(enumeration, readOnly, &status); });
    length = -99;
    Require(!uenum_unext_nid_postfix(enumeration, &length, &status) && length == 0 && status == 0);
    uenum_close_nid_postfix(enumeration);
}

void IgnoredPlural() {
    const auto* code = u"ZZZtail";
    const auto* ignored = reinterpret_cast<const char*>(1);
    const auto expected = CurrencyOracle::Name(code, "en", 1, ignored, 0, true);
    std::int32_t status = 0;
    std::int32_t length = -99;
    std::int8_t choice = 73;
    const auto* text = ucurr_getPluralName_nid_postfix(code, "en", &choice, ignored, &length, &status);
    Require(text == code && expected.alias && status == expected.status && choice == expected.choice && length == expected.length);
}

void HistoryBatch() {
    CurrencyOracle::SetDefault(uloc_getDefault());
    std::string region;
    std::int64_t date;
    int safe;
    while (std::cin >> region >> date >> safe) {
        const std::string locale = "und_" + region;
        std::int32_t status = 0;
        const auto count = ucurr_countCurrencies_nid_postfix(locale.c_str(), static_cast<double>(date), &status);
        Require(count >= 0 && status == 0);
        if (safe) {
            const auto expected = CurrencyOracle::Scalar(6, u"USD", locale.c_str(), 0, static_cast<double>(date), 0, 0);
            Require(count == expected.value && status == expected.status);
        }
        std::cout << "HISTORY " << region << ' ' << date << ' ' << count << ' ';
        for (std::int32_t index = 1; index <= count; ++index) {
            std::array<char16_t, 8> result;
            result.fill(0x5a5a);
            const auto length = ucurr_forLocaleAndDate_nid_postfix(locale.c_str(), static_cast<double>(date), index, result.data(), 4, &status);
            Require(length == 3 && status == 0 && result[3] == 0 && result[4] == 0x5a5a);
            if (safe) {
                const auto expected = CurrencyOracle::Code(locale.c_str(), static_cast<double>(date), index, 4, 0, true);
                Require(length == expected.length && status == expected.status && result == expected.text);
            }
            if (index > 1) std::cout << ',';
            for (std::int32_t unit = 0; unit < length; ++unit) {
                Require(result[unit] >= u'A' && result[unit] <= u'Z');
                std::cout << static_cast<char>(result[unit]);
            }
        }
        std::cout << '\n';
    }
    Require(std::cin.eof());
}

struct Memory {
    std::size_t calls = 0;
    std::size_t allocations = 0;
    std::size_t frees = 0;
    std::size_t failAt = 0;
    std::size_t reentries = 0;
    bool reenter = false;
    bool invalid = false;
};

void* APS5_VABI AllocateCurrency(const void* context, std::size_t size) {
    auto& memory = *static_cast<Memory*>(const_cast<void*>(context));
    if (++memory.calls == memory.failAt) return nullptr;
    if (memory.reenter) {
        std::int32_t status = 0;
        if (ucurr_unregister_nid_postfix(nullptr, &status) != 0 || status != 0) memory.invalid = true;
        ++memory.reentries;
    }
    auto* result = std::malloc(size);
    if (result) ++memory.allocations;
    return result;
}

void* APS5_VABI ReallocateCurrency(const void* context, void* pointer, std::size_t size) {
    if (!pointer) return AllocateCurrency(context, size);
    auto& memory = *static_cast<Memory*>(const_cast<void*>(context));
    if (++memory.calls == memory.failAt) return nullptr;
    return std::realloc(pointer, size);
}

void APS5_VABI FreeCurrency(const void* context, void* pointer) {
    auto& memory = *static_cast<Memory*>(const_cast<void*>(context));
    if (pointer) ++memory.frees;
    std::free(pointer);
}

void AllocationISO(std::size_t failed) {
    Memory memory;
    memory.failAt = failed;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&memory, AllocateCurrency, ReallocateCurrency, FreeCurrency, &status);
    Require(status == 0);
    Require(ucurr_openISOCurrencies_nid_postfix(AllCurrencies, &status) == nullptr && status == 7);
    Require(memory.allocations == memory.frees && memory.calls == failed);
    memory.failAt = 0;
    auto* enumeration = ucurr_openISOCurrencies_nid_postfix(AllCurrencies, &status);
    Require(enumeration && status == 7);
    status = 0;
    std::int32_t length = 0;
    Require(uenum_unext_nid_postfix(enumeration, &length, &status) && length == 3 && status == 0);
    uenum_close_nid_postfix(enumeration);
    u_cleanup_nid_postfix();
    Require(memory.allocations == memory.frees);
}

void AllocationRegistry() {
    Memory memory;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&memory, AllocateCurrency, ReallocateCurrency, FreeCurrency, &status);
    Require(status == 0);
    std::array<char16_t, 4> result{};
    Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && status == 0);
    memory.reenter = true;
    const auto key = ucurr_register_nid_postfix(u"JPY", "en_US", &status);
    Require(key && status == 0 && memory.reentries > 0 && !memory.invalid);
    memory.reenter = false;
    memory.failAt = memory.calls + 1;
    Require(!ucurr_register_nid_postfix(u"CHF", "en_US", &status) && status == 7);
    memory.failAt = 0;
    status = 0;
    Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"JPY");
    Require(ucurr_unregister_nid_postfix(key, &status) == 1);
    u_cleanup_nid_postfix();
    Require(memory.allocations == memory.frees);
}

void AllocationName() {
    Memory memory;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&memory, AllocateCurrency, ReallocateCurrency, FreeCurrency, &status);
    Require(status == 0);
    std::array<char16_t, 4> result{};
    Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && status == 0);
    memory.failAt = memory.calls + 1;
    const auto* code = u"USD";
    std::int32_t length = -99;
    std::int8_t choice = 73;
    const auto* text = ucurr_getName_nid_postfix(code, "en", 0, &choice, &length, &status);
    std::fprintf(stderr, "name allocation fallback: alias=%d length=%d status=%d choice=%d\n", text == code, length, status, choice);
    Require(text == code && length == 3 && status == -127 && choice == 0);
    memory.failAt = 0;
    u_cleanup_nid_postfix();
    Require(memory.allocations == memory.frees);
}

std::size_t KeywordAllocationAttempt(std::size_t failed) {
    Memory memory;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&memory, AllocateCurrency, ReallocateCurrency, FreeCurrency, &status);
    Require(status == 0);
    std::array<char16_t, 4> currency{};
    Require(ucurr_forLocale_nid_postfix("en_US", currency.data(), 4, &status) == 3 && status == 0);
    const auto before = memory.calls;
    memory.failAt = failed ? before + failed : 0;
    std::fprintf(stderr, "keyword allocation point %zu\n", failed);
    auto* enumeration = ucurr_getKeywordValuesForLocale_nid_postfix("currency", "en_QQ", 1, &status);
    const auto calls = memory.calls - before;
    memory.failAt = 0;
    if (enumeration) {
        Require(status <= 0 && uenum_count_nid_postfix(enumeration, &status) == 2);
        uenum_close_nid_postfix(enumeration);
    } else {
        Require(status == 7);
    }
    status = 0;
    enumeration = ucurr_getKeywordValuesForLocale_nid_postfix("currency", "en_QQ", 1, &status);
    Require(enumeration && status <= 0 && uenum_count_nid_postfix(enumeration, &status) == 2);
    std::int32_t length = -99;
    const auto* value = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(value && std::string_view(value, length) == "USD" && status <= 0);
    uenum_close_nid_postfix(enumeration);
    u_cleanup_nid_postfix();
    Require(memory.allocations == memory.frees);
    return calls;
}

void AllocationKeywords() {
    const auto calls = KeywordAllocationAttempt(0);
    Require(calls > 3 && calls < 512);
    for (std::size_t failed = 1; failed <= calls + 1; ++failed) KeywordAllocationAttempt(failed);
    std::fprintf(stderr, "%zu keyword allocation failure points qualified\n", calls);
}

void DefaultState() {
    Require(setenv_nid_postfix("LC_ALL", "sv_SE.UTF-8", 1) == 0);
    Require(std::string_view(uloc_getDefault()) == "sv_SE");
    CurrencyOracle::SetDefault(uloc_getDefault());
    const auto expected = CurrencyOracle::Name(u"USD", "zz_ZZ", 0, "other", 0, false);
    for (unsigned pass = 0; pass < 2; ++pass) {
        std::int32_t status = 0;
        std::array<char16_t, 4> result{};
        Require(ucurr_forLocale_nid_postfix(nullptr, result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"SEK" && status == 0);
        std::int32_t length = -99;
        std::int8_t choice = 73;
        const auto* text = ucurr_getName_nid_postfix(u"USD", "zz_ZZ", 0, &choice, &length, &status);
        Require(text && std::u16string_view(text, length) == expected.text && status == expected.status && choice == expected.choice);
        u_cleanup_nid_postfix();
    }
}

void EnumerationLifetime() {
    std::int32_t status = 0;
    auto* enumeration = ucurr_openISOCurrencies_nid_postfix(AllCurrencies, &status);
    auto* other = ucurr_openISOCurrencies_nid_postfix(0xffffffff, &status);
    Require(enumeration && other && status == 0);
    const auto count = enumeration->count;
    const auto reset = enumeration->reset;
    const auto close = enumeration->close;
    std::int32_t length = -99;
    const auto* first = uenum_unext_nid_postfix(enumeration, &length, &status);
    Require(first && std::u16string_view(first, length) == u"ADP");
    status = 7;
    Require(uenum_next_nid_postfix(enumeration, reinterpret_cast<std::int32_t*>(1), &status) == nullptr && status == 7);
    status = 0;
    reset(enumeration, &status);
    const auto* again = uenum_unext_nid_postfix(enumeration, &length, &status);
    Require(again && std::u16string_view(again, length) == u"ADP");
    close(enumeration);
    RequireThrows([&] { count(enumeration, &status); });
    RequireThrows([&] { reset(enumeration, &status); });
    RequireThrows([&] { close(enumeration); });
    Require(uenum_count_nid_postfix(other, &status) == 0 && status == 0);
    uenum_close_nid_postfix(other);
}

void ConcurrentRegistry() {
    std::atomic<bool> failed = false;
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) thread = std::thread([&] {
        try {
            for (unsigned round = 0; round < 100; ++round) {
                std::int32_t status = 0;
                const auto key = ucurr_register_nid_postfix(u"JPY", "en_US", &status);
                Require(key && status == 0);
                std::array<char16_t, 4> result{};
                Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"JPY");
                Require(ucurr_unregister_nid_postfix(key, &status) == 1 && status == 0);
            }
        } catch (...) { failed = true; }
    });
    for (auto& thread : threads) thread.join();
    Require(!failed.load());
    std::int32_t status = 0;
    std::array<char16_t, 4> result{};
    Require(ucurr_forLocale_nid_postfix("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"USD" && status == 0);
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2);
        const std::string_view mode = argv[1];
        if (mode.starts_with("api_")) Api(static_cast<unsigned>(std::stoi(std::string(mode.substr(4)))));
        else if (mode == "metadata") Metadata();
        else if (mode == "locale") Locale();
        else if (mode == "names") Names();
        else if (mode == "historical") Historical();
        else if (mode == "historical_missing_from") MissingHistoricalDate(true);
        else if (mode == "historical_missing_to") MissingHistoricalDate(false);
        else if (mode == "enumeration") Enumeration();
        else if (mode == "registration") Registration();
        else if (mode == "guarded") GuardedInputs();
        else if (mode == "guarded_outputs") GuardedOutputs();
        else if (mode == "scalar_oracle") ScalarOracle();
        else if (mode == "code_oracle") CodeOracle();
        else if (mode == "name_oracle") NameOracle();
        else if (mode == "enumeration_oracle") EnumerationOracle();
        else if (mode == "ignored_plural") IgnoredPlural();
        else if (mode == "null_plural") NullPlural();
        else if (mode == "keyword_oracle") KeywordOracle();
        else if (mode == "enumeration_write_bounds") EnumerationWriteBounds();
        else if (mode == "allocation_keywords") AllocationKeywords();
        else if (mode == "allocation_iso_first") AllocationISO(1);
        else if (mode == "allocation_iso_second") AllocationISO(2);
        else if (mode == "allocation_registry") AllocationRegistry();
        else if (mode == "allocation_name") AllocationName();
        else if (mode == "default_state") DefaultState();
        else if (mode == "enumeration_lifetime") EnumerationLifetime();
        else if (mode == "concurrent_registry") ConcurrentRegistry();
        else if (mode == "history_batch") HistoryBatch();
        else Require(false);
        std::fprintf(stderr, "currency %s: passed\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "currency failure: %s\n", error.what());
        return 1;
    }
}
