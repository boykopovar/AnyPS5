#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
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
        else if (mode == "enumeration") Enumeration();
        else if (mode == "registration") Registration();
        else if (mode == "guarded") GuardedInputs();
        else Require(false);
        std::fprintf(stderr, "currency %s: passed\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "currency failure: %s\n", error.what());
        return 1;
    }
}
