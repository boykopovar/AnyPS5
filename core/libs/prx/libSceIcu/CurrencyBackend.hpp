#pragma once
#include <array>
#include <cstdint>

namespace IcuCurrency {

struct CodeResult {
    std::array<char16_t, 4> text{};
    std::int32_t length = 0;
    std::int32_t written = 0;
};

struct NameResult {
    const char16_t* text = nullptr;
    std::int32_t length = INT32_MIN;
    std::int8_t choice = -1;
    bool inputAlias = false;
};

struct NextResult {
    const void* text = nullptr;
    std::int32_t length = INT32_MIN;
};

using CurrencyReader = const char16_t* (*)(void*, bool);
using PluralReader = const char* (*)(void*);

void SetDefault_nid_no_patch(const char*, std::int32_t&);
CodeResult ForLocale_nid_no_patch(const char*, std::int32_t, std::int32_t&);
CodeResult ForLocaleAndDate_nid_no_patch(const char*, double, std::int32_t, std::int32_t, std::int32_t&);
NameResult Name_nid_no_patch(const char*, std::int32_t, bool, bool, CurrencyReader, PluralReader, void*, std::int32_t&);
std::int32_t FractionDigits_nid_no_patch(const char16_t*, std::int32_t, std::int32_t&);
double RoundingIncrement_nid_no_patch(const char16_t*, std::int32_t, std::int32_t&);
std::int32_t NumericCode_nid_no_patch(const char16_t*);
std::int8_t IsAvailable_nid_no_patch(const char16_t*, double, double, std::int32_t&);
std::int32_t CountCurrencies_nid_no_patch(const char*, double, std::int32_t&);
void* OpenISO_nid_no_patch(std::uint32_t, std::int32_t&);
void* OpenKeywords_nid_no_patch(const char*, std::int8_t, std::int32_t&);
const void* Register_nid_no_patch(const char16_t*, const char*, std::int32_t&);
std::int8_t Unregister_nid_no_patch(const void*, std::int32_t&);
void CloseEnumeration_nid_no_patch(void*);
std::int32_t CountEnumeration_nid_no_patch(void*, std::int32_t&);
NextResult NextEnumeration_nid_no_patch(void*, bool, std::int32_t&);
void ResetEnumeration_nid_no_patch(void*, std::int32_t&);

}
