#include "prx/libSceIcu/Currency.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include <nid/NidCompute.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("currency NID assertion at line " + std::to_string(location.line()));
}

template<auto Function>
auto Resolve(void* module, const char* name) {
    const auto symbol = Nid::ComputeNid(name, "");
#ifdef _WIN32
    const auto function = reinterpret_cast<decltype(Function)>(reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), symbol.c_str())));
#else
    const auto function = reinterpret_cast<decltype(Function)>(dlsym(module, symbol.c_str()));
#endif
    Require(function != nullptr);
    return function;
}

void Check(void* module) {
    const auto locale = Resolve<&ucurr_forLocale_nid_postfix>(module, "ucurr_forLocale");
    const auto name = Resolve<&ucurr_getName_nid_postfix>(module, "ucurr_getName");
    const auto plural = Resolve<&ucurr_getPluralName_nid_postfix>(module, "ucurr_getPluralName");
    const auto digits = Resolve<&ucurr_getDefaultFractionDigits_nid_postfix>(module, "ucurr_getDefaultFractionDigits");
    const auto cashDigits = Resolve<&ucurr_getDefaultFractionDigitsForUsage_nid_postfix>(module, "ucurr_getDefaultFractionDigitsForUsage");
    const auto rounding = Resolve<&ucurr_getRoundingIncrement_nid_postfix>(module, "ucurr_getRoundingIncrement");
    const auto cashRounding = Resolve<&ucurr_getRoundingIncrementForUsage_nid_postfix>(module, "ucurr_getRoundingIncrementForUsage");
    const auto numeric = Resolve<&ucurr_getNumericCode_nid_postfix>(module, "ucurr_getNumericCode");
    const auto available = Resolve<&ucurr_isAvailable_nid_postfix>(module, "ucurr_isAvailable");
    const auto count = Resolve<&ucurr_countCurrencies_nid_postfix>(module, "ucurr_countCurrencies");
    const auto historical = Resolve<&ucurr_forLocaleAndDate_nid_postfix>(module, "ucurr_forLocaleAndDate");
    const auto iso = Resolve<&ucurr_openISOCurrencies_nid_postfix>(module, "ucurr_openISOCurrencies");
    const auto keywords = Resolve<&ucurr_getKeywordValuesForLocale_nid_postfix>(module, "ucurr_getKeywordValuesForLocale");
    const auto registration = Resolve<&ucurr_register_nid_postfix>(module, "ucurr_register");
    const auto unregistration = Resolve<&ucurr_unregister_nid_postfix>(module, "ucurr_unregister");
    const auto enumCount = Resolve<&uenum_count_nid_postfix>(module, "uenum_count");
    const auto next = Resolve<&uenum_next_nid_postfix>(module, "uenum_next");
    const auto unext = Resolve<&uenum_unext_nid_postfix>(module, "uenum_unext");
    const auto close = Resolve<&uenum_close_nid_postfix>(module, "uenum_close");
    std::int32_t status = 0;
    std::array<char16_t, 4> result{};
    Require(locale("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"USD");
    std::int32_t length = -99;
    std::int8_t choice = 73;
    const auto* text = name(u"USD", "en", 0, &choice, &length, &status);
    Require(text && std::u16string_view(text, length) == u"$" && choice == 0);
    choice = 73;
    text = plural(u"USD", "en", &choice, "other", &length, &status);
    Require(text && std::u16string_view(text, length) == u"US dollars" && choice == 73);
    Require(digits(u"JPY", &status) == 0 && cashDigits(u"USD", 1, &status) == 2);
    Require(rounding(u"CHF", &status) == 0 && cashRounding(u"CHF", 1, &status) == 0.05);
    Require(numeric(u"USD") == 840 && available(u"USD", 0, 1700000000000.0, &status));
    Require(count("en_US", 1700000000000.0, &status) == 2);
    Require(historical("en_US", 1700000000000.0, 2, result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"USN");
    for (const std::int32_t incoming : {0, -127}) {
        std::int32_t openStatus = incoming;
        auto* enumeration = iso(0x7fffffff, &openStatus);
        Require(enumeration && openStatus == incoming);
        Require(enumCount(enumeration, &status) > 200 && next(enumeration, &length, &status));
        enumeration->reset(enumeration, &status);
        Require(unext(enumeration, &length, &status) && length == 3);
        close(enumeration);
    }
    auto* enumeration = keywords("currency", "en_US", 1, &status);
    Require(enumeration && enumCount(enumeration, &status) >= 1);
    Require(next(enumeration, &length, &status) != nullptr);
    close(enumeration);
    const auto key = registration(u"JPY", "en_US", &status);
    Require(key && locale("en_US", result.data(), 4, &status) == 3 && std::u16string_view(result.data()) == u"JPY");
    Require(unregistration(key, &status) == 1 && unregistration(nullptr, &status) == 0 && status == 0);
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2);
        const auto path = std::filesystem::absolute(argv[1]) / "libSceIcu.prx";
#ifdef _WIN32
        auto* module = reinterpret_cast<void*>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
#else
        auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
        Require(module != nullptr);
        Check(module);
        std::puts("all fifteen currency services executed through patched NIDs");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
