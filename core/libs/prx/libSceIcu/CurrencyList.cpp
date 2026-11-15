// Derived from icu4c/source/common/ucurr.cpp and uenum.cpp@049e0d6a420629ac7db77256987d083a563287b5 (ICU 78.1):
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ucurr.cpp
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/uenum.cpp
//
// The upstream files' notices, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (c) 2002-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.
//   Copyright (C) 2002-2012, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/CurrencyList.hpp"
#include "prx/libSceIcu/Invariant.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace {

constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t UnsupportedError = 16;
constexpr std::uint32_t AllCurrencies = 0x7fffffff;
constexpr std::uint32_t Common = 1;
constexpr std::uint32_t Uncommon = 2;
constexpr std::uint32_t Deprecated = 4;
constexpr std::uint32_t NonDeprecated = 8;
constexpr std::int32_t CodeLength = 3;
constexpr std::int32_t BufferPadding = 8;

struct Currency {
    char code[CodeLength + 1];
    std::uint32_t type;
};

struct CurrencyContext {
    std::uint32_t type;
    std::uint32_t index;
};

constexpr Currency Currencies[] = {
    {"ADP", Common | Deprecated},
    {"AED", Common | NonDeprecated},
    {"AFA", Common | Deprecated},
    {"AFN", Common | NonDeprecated},
    {"ALK", Common | Deprecated},
    {"ALL", Common | NonDeprecated},
    {"AMD", Common | NonDeprecated},
    {"ANG", Common | NonDeprecated},
    {"AOA", Common | NonDeprecated},
    {"AOK", Common | Deprecated},
    {"AON", Common | Deprecated},
    {"AOR", Common | Deprecated},
    {"ARA", Common | Deprecated},
    {"ARL", Common | Deprecated},
    {"ARM", Common | Deprecated},
    {"ARP", Common | Deprecated},
    {"ARS", Common | NonDeprecated},
    {"ATS", Common | Deprecated},
    {"AUD", Common | NonDeprecated},
    {"AWG", Common | NonDeprecated},
    {"AZM", Common | Deprecated},
    {"AZN", Common | NonDeprecated},
    {"BAD", Common | Deprecated},
    {"BAM", Common | NonDeprecated},
    {"BAN", Common | Deprecated},
    {"BBD", Common | NonDeprecated},
    {"BDT", Common | NonDeprecated},
    {"BEC", Uncommon | Deprecated},
    {"BEF", Common | Deprecated},
    {"BEL", Uncommon | Deprecated},
    {"BGL", Common | Deprecated},
    {"BGM", Common | Deprecated},
    {"BGN", Common | NonDeprecated},
    {"BGO", Common | Deprecated},
    {"BHD", Common | NonDeprecated},
    {"BIF", Common | NonDeprecated},
    {"BMD", Common | NonDeprecated},
    {"BND", Common | NonDeprecated},
    {"BOB", Common | NonDeprecated},
    {"BOL", Common | Deprecated},
    {"BOP", Common | Deprecated},
    {"BOV", Uncommon | NonDeprecated},
    {"BRB", Common | Deprecated},
    {"BRC", Common | Deprecated},
    {"BRE", Common | Deprecated},
    {"BRL", Common | NonDeprecated},
    {"BRN", Common | Deprecated},
    {"BRR", Common | Deprecated},
    {"BRZ", Common | Deprecated},
    {"BSD", Common | NonDeprecated},
    {"BTN", Common | NonDeprecated},
    {"BUK", Common | Deprecated},
    {"BWP", Common | NonDeprecated},
    {"BYB", Common | Deprecated},
    {"BYN", Common | NonDeprecated},
    {"BYR", Common | Deprecated},
    {"BZD", Common | NonDeprecated},
    {"CAD", Common | NonDeprecated},
    {"CDF", Common | NonDeprecated},
    {"CHE", Uncommon | NonDeprecated},
    {"CHF", Common | NonDeprecated},
    {"CHW", Uncommon | NonDeprecated},
    {"CLE", Common | Deprecated},
    {"CLF", Uncommon | NonDeprecated},
    {"CLP", Common | NonDeprecated},
    {"CNH", Uncommon | NonDeprecated},
    {"CNX", Uncommon | Deprecated},
    {"CNY", Common | NonDeprecated},
    {"COP", Common | NonDeprecated},
    {"COU", Uncommon | NonDeprecated},
    {"CRC", Common | NonDeprecated},
    {"CSD", Common | Deprecated},
    {"CSK", Common | Deprecated},
    {"CUC", Common | NonDeprecated},
    {"CUP", Common | NonDeprecated},
    {"CVE", Common | NonDeprecated},
    {"CYP", Common | Deprecated},
    {"CZK", Common | NonDeprecated},
    {"DDM", Common | Deprecated},
    {"DEM", Common | Deprecated},
    {"DJF", Common | NonDeprecated},
    {"DKK", Common | NonDeprecated},
    {"DOP", Common | NonDeprecated},
    {"DZD", Common | NonDeprecated},
    {"ECS", Common | Deprecated},
    {"ECV", Uncommon | Deprecated},
    {"EEK", Common | Deprecated},
    {"EGP", Common | NonDeprecated},
    {"ERN", Common | NonDeprecated},
    {"ESA", Uncommon | Deprecated},
    {"ESB", Uncommon | Deprecated},
    {"ESP", Common | Deprecated},
    {"ETB", Common | NonDeprecated},
    {"EUR", Common | NonDeprecated},
    {"FIM", Common | Deprecated},
    {"FJD", Common | NonDeprecated},
    {"FKP", Common | NonDeprecated},
    {"FRF", Common | Deprecated},
    {"GBP", Common | NonDeprecated},
    {"GEK", Common | Deprecated},
    {"GEL", Common | NonDeprecated},
    {"GHC", Common | Deprecated},
    {"GHS", Common | NonDeprecated},
    {"GIP", Common | NonDeprecated},
    {"GMD", Common | NonDeprecated},
    {"GNF", Common | NonDeprecated},
    {"GNS", Common | Deprecated},
    {"GQE", Common | Deprecated},
    {"GRD", Common | Deprecated},
    {"GTQ", Common | NonDeprecated},
    {"GWE", Common | Deprecated},
    {"GWP", Common | Deprecated},
    {"GYD", Common | NonDeprecated},
    {"HKD", Common | NonDeprecated},
    {"HNL", Common | NonDeprecated},
    {"HRD", Common | Deprecated},
    {"HRK", Common | NonDeprecated},
    {"HTG", Common | NonDeprecated},
    {"HUF", Common | NonDeprecated},
    {"IDR", Common | NonDeprecated},
    {"IEP", Common | Deprecated},
    {"ILP", Common | Deprecated},
    {"ILR", Common | Deprecated},
    {"ILS", Common | NonDeprecated},
    {"INR", Common | NonDeprecated},
    {"IQD", Common | NonDeprecated},
    {"IRR", Common | NonDeprecated},
    {"ISJ", Common | Deprecated},
    {"ISK", Common | NonDeprecated},
    {"ITL", Common | Deprecated},
    {"JMD", Common | NonDeprecated},
    {"JOD", Common | NonDeprecated},
    {"JPY", Common | NonDeprecated},
    {"KES", Common | NonDeprecated},
    {"KGS", Common | NonDeprecated},
    {"KHR", Common | NonDeprecated},
    {"KMF", Common | NonDeprecated},
    {"KPW", Common | NonDeprecated},
    {"KRH", Common | Deprecated},
    {"KRO", Common | Deprecated},
    {"KRW", Common | NonDeprecated},
    {"KWD", Common | NonDeprecated},
    {"KYD", Common | NonDeprecated},
    {"KZT", Common | NonDeprecated},
    {"LAK", Common | NonDeprecated},
    {"LBP", Common | NonDeprecated},
    {"LKR", Common | NonDeprecated},
    {"LRD", Common | NonDeprecated},
    {"LSL", Common | NonDeprecated},
    {"LSM", Common | Deprecated},
    {"LTL", Common | Deprecated},
    {"LTT", Common | Deprecated},
    {"LUC", Uncommon | Deprecated},
    {"LUF", Common | Deprecated},
    {"LUL", Uncommon | Deprecated},
    {"LVL", Common | Deprecated},
    {"LVR", Common | Deprecated},
    {"LYD", Common | NonDeprecated},
    {"MAD", Common | NonDeprecated},
    {"MAF", Common | Deprecated},
    {"MCF", Common | Deprecated},
    {"MDC", Common | Deprecated},
    {"MDL", Common | NonDeprecated},
    {"MGA", Common | NonDeprecated},
    {"MGF", Common | Deprecated},
    {"MKD", Common | NonDeprecated},
    {"MKN", Common | Deprecated},
    {"MLF", Common | Deprecated},
    {"MMK", Common | NonDeprecated},
    {"MNT", Common | NonDeprecated},
    {"MOP", Common | NonDeprecated},
    {"MRO", Common | Deprecated},
    {"MRU", Common | NonDeprecated},
    {"MTL", Common | Deprecated},
    {"MTP", Common | Deprecated},
    {"MUR", Common | NonDeprecated},
    {"MVP", Common | Deprecated},
    {"MVR", Common | NonDeprecated},
    {"MWK", Common | NonDeprecated},
    {"MXN", Common | NonDeprecated},
    {"MXP", Common | Deprecated},
    {"MXV", Uncommon | NonDeprecated},
    {"MYR", Common | NonDeprecated},
    {"MZE", Common | Deprecated},
    {"MZM", Common | Deprecated},
    {"MZN", Common | NonDeprecated},
    {"NAD", Common | NonDeprecated},
    {"NGN", Common | NonDeprecated},
    {"NIC", Common | Deprecated},
    {"NIO", Common | NonDeprecated},
    {"NLG", Common | Deprecated},
    {"NOK", Common | NonDeprecated},
    {"NPR", Common | NonDeprecated},
    {"NZD", Common | NonDeprecated},
    {"OMR", Common | NonDeprecated},
    {"PAB", Common | NonDeprecated},
    {"PEI", Common | Deprecated},
    {"PEN", Common | NonDeprecated},
    {"PES", Common | Deprecated},
    {"PGK", Common | NonDeprecated},
    {"PHP", Common | NonDeprecated},
    {"PKR", Common | NonDeprecated},
    {"PLN", Common | NonDeprecated},
    {"PLZ", Common | Deprecated},
    {"PTE", Common | Deprecated},
    {"PYG", Common | NonDeprecated},
    {"QAR", Common | NonDeprecated},
    {"RHD", Common | Deprecated},
    {"ROL", Common | Deprecated},
    {"RON", Common | NonDeprecated},
    {"RSD", Common | NonDeprecated},
    {"RUB", Common | NonDeprecated},
    {"RUR", Common | Deprecated},
    {"RWF", Common | NonDeprecated},
    {"SAR", Common | NonDeprecated},
    {"SBD", Common | NonDeprecated},
    {"SCR", Common | NonDeprecated},
    {"SDD", Common | Deprecated},
    {"SDG", Common | NonDeprecated},
    {"SDP", Common | Deprecated},
    {"SEK", Common | NonDeprecated},
    {"SGD", Common | NonDeprecated},
    {"SHP", Common | NonDeprecated},
    {"SIT", Common | Deprecated},
    {"SKK", Common | Deprecated},
    {"SLE", Common | NonDeprecated},
    {"SLL", Common | NonDeprecated},
    {"SOS", Common | NonDeprecated},
    {"SRD", Common | NonDeprecated},
    {"SRG", Common | Deprecated},
    {"SSP", Common | NonDeprecated},
    {"STD", Common | Deprecated},
    {"STN", Common | NonDeprecated},
    {"SUR", Common | Deprecated},
    {"SVC", Common | Deprecated},
    {"SYP", Common | NonDeprecated},
    {"SZL", Common | NonDeprecated},
    {"THB", Common | NonDeprecated},
    {"TJR", Common | Deprecated},
    {"TJS", Common | NonDeprecated},
    {"TMM", Common | Deprecated},
    {"TMT", Common | NonDeprecated},
    {"TND", Common | NonDeprecated},
    {"TOP", Common | NonDeprecated},
    {"TPE", Common | Deprecated},
    {"TRL", Common | Deprecated},
    {"TRY", Common | NonDeprecated},
    {"TTD", Common | NonDeprecated},
    {"TWD", Common | NonDeprecated},
    {"TZS", Common | NonDeprecated},
    {"UAH", Common | NonDeprecated},
    {"UAK", Common | Deprecated},
    {"UGS", Common | Deprecated},
    {"UGX", Common | NonDeprecated},
    {"USD", Common | NonDeprecated},
    {"USN", Uncommon | NonDeprecated},
    {"USS", Uncommon | NonDeprecated},
    {"UYI", Uncommon | NonDeprecated},
    {"UYP", Common | Deprecated},
    {"UYU", Common | NonDeprecated},
    {"UYW", Uncommon | NonDeprecated},
    {"UZS", Common | NonDeprecated},
    {"VEB", Common | Deprecated},
    {"VED", Uncommon | NonDeprecated},
    {"VEF", Common | NonDeprecated},
    {"VES", Common | NonDeprecated},
    {"VND", Common | NonDeprecated},
    {"VNN", Common | Deprecated},
    {"VUV", Common | NonDeprecated},
    {"WST", Common | NonDeprecated},
    {"XAF", Common | NonDeprecated},
    {"XAG", Uncommon | NonDeprecated},
    {"XAU", Uncommon | NonDeprecated},
    {"XBA", Uncommon | NonDeprecated},
    {"XBB", Uncommon | NonDeprecated},
    {"XBC", Uncommon | NonDeprecated},
    {"XBD", Uncommon | NonDeprecated},
    {"XCD", Common | NonDeprecated},
    {"XCG", Common | NonDeprecated},
    {"XDR", Uncommon | NonDeprecated},
    {"XEU", Uncommon | Deprecated},
    {"XFO", Uncommon | NonDeprecated},
    {"XFU", Uncommon | NonDeprecated},
    {"XOF", Common | NonDeprecated},
    {"XPD", Uncommon | NonDeprecated},
    {"XPF", Common | NonDeprecated},
    {"XPT", Uncommon | NonDeprecated},
    {"XRE", Uncommon | Deprecated},
    {"XSU", Uncommon | NonDeprecated},
    {"XTS", Uncommon | NonDeprecated},
    {"XUA", Uncommon | NonDeprecated},
    {"XXX", Uncommon | NonDeprecated},
    {"YDD", Common | Deprecated},
    {"YER", Common | NonDeprecated},
    {"YUD", Common | Deprecated},
    {"YUM", Common | Deprecated},
    {"YUN", Common | Deprecated},
    {"YUR", Common | Deprecated},
    {"ZAL", Uncommon | Deprecated},
    {"ZAR", Common | NonDeprecated},
    {"ZMK", Common | Deprecated},
    {"ZMW", Common | NonDeprecated},
    {"ZRN", Common | Deprecated},
    {"ZRZ", Common | Deprecated},
    {"ZWD", Common | Deprecated},
    {"ZWG", Common | NonDeprecated},
    {"ZWL", Common | Deprecated},
    {"ZWR", Common | Deprecated},
};

bool Matches(std::uint32_t type, std::uint32_t wanted) {
    return wanted == AllCurrencies || (type & wanted) == wanted;
}

void Report(std::int32_t* status, std::int32_t error) {
    if (!status) APS5_INVALID_ARG_EX;
    *status = error;
}

CurrencyContext& Context(UEnumeration* enumeration) {
    return *static_cast<CurrencyContext*>(enumeration->context);
}

char16_t* UnicodeBuffer(UEnumeration* enumeration, std::int32_t capacity) {
    auto* length = static_cast<std::int32_t*>(enumeration->baseContext);
    if (!length || *length < capacity) {
        capacity += BufferPadding;
        const std::size_t bytes = sizeof(std::int32_t) + static_cast<std::size_t>(capacity);
        length = static_cast<std::int32_t*>(length ? realloc_nid_postfix(length, bytes) : malloc_nid_postfix(bytes));
        enumeration->baseContext = length;
        if (!length) return nullptr;
        *length = capacity;
    }
    return reinterpret_cast<char16_t*>(length + 1);
}

void APS5_VABI Close_nid_no_patch(UEnumeration* enumeration) {
    free_nid_postfix(enumeration->context);
    free_nid_postfix(enumeration);
}

std::int32_t APS5_VABI Count_nid_no_patch(UEnumeration* enumeration, std::int32_t*) {
    const std::uint32_t wanted = Context(enumeration).type;
    std::int32_t count = 0;
    for (const Currency& currency : Currencies) {
        if (Matches(currency.type, wanted)) ++count;
    }
    return count;
}

const char* APS5_VABI Next_nid_no_patch(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t*) {
    CurrencyContext& context = Context(enumeration);
    while (context.index < std::size(Currencies)) {
        const Currency& currency = Currencies[context.index++];
        if (Matches(currency.type, context.type)) {
            if (resultLength) *resultLength = CodeLength;
            return currency.code;
        }
    }
    if (resultLength) *resultLength = 0;
    return nullptr;
}

const char16_t* APS5_VABI UnicodeNext_nid_no_patch(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    char16_t* units = nullptr;
    std::int32_t length = 0;
    if (enumeration->next) {
        const char* code = enumeration->next(enumeration, &length, status);
        if (code) {
            units = UnicodeBuffer(enumeration, (length + 1) * static_cast<std::int32_t>(sizeof(char16_t)));
            if (units) u_charsToUChars_nid_postfix(code, units, length + 1);
            else Report(status, MemoryAllocationError);
        }
    } else {
        Report(status, UnsupportedError);
    }
    if (resultLength) *resultLength = length;
    return units;
}

void APS5_VABI Reset_nid_no_patch(UEnumeration* enumeration, std::int32_t*) {
    Context(enumeration).index = 0;
}

}

extern "C" {

UEnumeration* APS5_VABI ucurr_openISOCurrencies(std::uint32_t type, std::int32_t* status) {
    auto* enumeration = static_cast<UEnumeration*>(malloc_nid_postfix(sizeof(UEnumeration)));
    if (!enumeration) {
        Report(status, MemoryAllocationError);
        return nullptr;
    }
    auto* context = static_cast<CurrencyContext*>(malloc_nid_postfix(sizeof(CurrencyContext)));
    if (!context) {
        free_nid_postfix(enumeration);
        Report(status, MemoryAllocationError);
        return nullptr;
    }
    *context = CurrencyContext{type, 0};
    *enumeration = UEnumeration{nullptr, context, Close_nid_no_patch, Count_nid_no_patch, UnicodeNext_nid_no_patch,
                                Next_nid_no_patch, Reset_nid_no_patch};
    return enumeration;
}

}
