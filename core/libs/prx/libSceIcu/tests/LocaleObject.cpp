// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {
using Icu::Abi::Locale;
void Check(bool condition) { if (!condition) std::abort(); }
}

int main() {
    std::atomic<int> ready{0};
    std::vector<std::thread> threads;
    std::vector<const Icu::Abi::Locale*> seen(16);
    for (int t = 0; t < 16; ++t) threads.emplace_back([&, t] {
        ++ready;
        while (ready.load() < 16) {}
        seen[t] = _ZN6icu_686Locale5getUSEv_nid_postfix();
        Check(std::strcmp(seen[t]->language, "en") == 0 && std::strcmp(seen[t]->country, "US") == 0);
        Check(std::strcmp(seen[t]->fullName, "en_US") == 0);
        Check(std::strcmp(_ZN6icu_686Locale9getLocaleEi_nid_postfix(t)->language, _ZN6icu_686Locale14getLocaleCacheEv_nid_postfix()[t].language) == 0);
    });
    for (auto& thread : threads) thread.join();
    for (auto* locale : seen) Check(locale == seen[0]);
    Locale locale{};
    _ZN6icu_686LocaleC1ENS0_11ELocaleTypeE_nid_postfix(&locale, 0);
    Check(locale.isBogus && locale.baseName == nullptr && locale.fullName == locale.fullNameBuffer);
    std::strcpy(locale.fullNameBuffer, "en_US@calendar=gregorian");
    std::strcpy(locale.language, "en"); std::strcpy(locale.country, "US");
    locale.variantBegin = 23; locale.isBogus = 0;
    std::int32_t status = -1;
    _ZN6icu_686Locale12initBaseNameER10UErrorCode_nid_postfix(&locale, &status);
    Check(status == -1 && locale.variantBegin == 5);
    Check(std::strcmp(_ZNK6icu_686Locale11getBaseNameEv_nid_postfix(&locale), "en_US") == 0);
    Check(std::strcmp(_ZNK6icu_686Locale15getISO3LanguageEv_nid_postfix(&locale), "eng") == 0);
    Check(std::strcmp(_ZNK6icu_686Locale14getISO3CountryEv_nid_postfix(&locale), "USA") == 0);
    Locale copy{};
    _ZN6icu_686LocaleC2ERKS0__nid_postfix(&copy, &locale);
    Check(_ZNK6icu_686LocaleeqERKS0__nid_postfix(&locale, &copy));
    Check(copy.baseName != locale.baseName && std::strcmp(copy.baseName, locale.baseName) == 0);
    Check(_ZNK6icu_686Locale8hashCodeEv_nid_postfix(&copy) == _ZNK6icu_686Locale8hashCodeEv_nid_postfix(&locale));
    Locale moved{};
    auto base = copy.baseName;
    _ZN6icu_686LocaleC2EOS0__nid_postfix(&moved, &copy);
    Check(moved.baseName == base && copy.baseName == copy.fullNameBuffer);
    Check(_ZN6icu_686LocaleaSERKS0__nid_postfix(&copy, &moved) == &copy);
    auto clone = _ZNK6icu_686Locale5cloneEv_nid_postfix(&moved);
    Check(clone && _ZNK6icu_686LocaleeqERKS0__nid_postfix(clone, &moved));
    _ZN6icu_686LocaleD0Ev_nid_postfix(clone);
    Check(_ZN6icu_686LocaleaSEOS0__nid_postfix(&locale, &moved) == &locale);
    Check(moved.baseName == moved.fullNameBuffer);
    Check(_ZNK6icu_686Locale17getDynamicClassIDEv_nid_postfix(&locale) == _ZN6icu_686Locale16getStaticClassIDEv_nid_postfix());
    Check(_ZN6icu_686Locale15getISOLanguagesEv_nid_postfix() == uloc_getISOLanguages_nid_postfix());
    Check(_ZN6icu_686Locale15getISOCountriesEv_nid_postfix() == uloc_getISOCountries_nid_postfix());
    char valid[157]{}, actualId[157]{};
    Icu::Abi::LocaleBased based{valid, actualId};
    _ZN6icu_6811LocaleBased12setLocaleIDsEPKcS2__nid_postfix(&based, "en_US", "en");
    status = 0;
    Check(_ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(&based, 1, &status) == valid);
    Check(_ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(&based, 0, &status) == actualId);
    _ZN6icu_6811LocaleBased12setLocaleIDsERKNS_6LocaleES3__nid_postfix(&based, &locale, &copy);
    Check(std::strcmp(valid, locale.fullName) == 0 && std::strcmp(actualId, copy.fullName) == 0);
    Check(_ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(nullptr, 9, &status) == nullptr && status == 1);
    _ZN6icu_686Locale10setToBogusEv_nid_postfix(&copy);
    Check(copy.isBogus && !copy.language[0] && !copy.fullName[0] && copy.baseName == nullptr);
    status = 7;
    _ZN6icu_686Locale12initBaseNameER10UErrorCode_nid_postfix(nullptr, &status);
    Check(status == 7);
    bool invalid = false;
    try { _ZN6icu_686LocaleaSEOS0__nid_postfix(&copy, &copy); } catch (const std::invalid_argument&) { invalid = true; }
    Check(invalid);
    _ZN6icu_686LocaleD2Ev_nid_postfix(&copy);
    _ZN6icu_686LocaleD1Ev_nid_postfix(&moved);
    _ZN6icu_686LocaleD1Ev_nid_postfix(&locale);
    Check(locale.baseName == nullptr && locale.vtable == Icu::Abi::Vtable_nid_no_patch(Icu::Abi::Class::Object));
    Icu::Abi::LocaleIterator iterator{locale.vtable};
    _ZN6icu_686Locale8IteratorD1Ev_nid_postfix(&iterator);
    Check(iterator.vtable != nullptr);
    auto iteratorType = reinterpret_cast<const Icu::Abi::TypeInfo* const*>(iterator.vtable)[-1];
    Check(std::strcmp(iteratorType->name, "N6icu_687UObjectE") == 0);
    auto table = iterator.vtable;
    _ZN6icu_686Locale8IteratorD2Ev_nid_postfix(&iterator);
    Check(iterator.vtable == table);
    auto heapIterator = static_cast<Icu::Abi::LocaleIterator*>(_ZN6icu_687UMemorynwEm_nid_postfix(sizeof(Icu::Abi::LocaleIterator)));
    heapIterator->vtable = table;
    _ZN6icu_686Locale8IteratorD0Ev_nid_postfix(heapIterator);
    Check(std::strcmp(_ZN6icu_686Locale7getRootEv_nid_postfix()->fullName, "") == 0);
    Check(std::strcmp(_ZN6icu_686Locale10getEnglishEv_nid_postfix()->fullName, "en") == 0);
    Check(std::strcmp(_ZN6icu_686Locale9getFrenchEv_nid_postfix()->fullName, "fr") == 0);
    Check(std::strcmp(_ZN6icu_686Locale9getGermanEv_nid_postfix()->fullName, "de") == 0);
    Check(std::strcmp(_ZN6icu_686Locale10getItalianEv_nid_postfix()->fullName, "it") == 0);
    Check(std::strcmp(_ZN6icu_686Locale11getJapaneseEv_nid_postfix()->fullName, "ja") == 0);
    Check(std::strcmp(_ZN6icu_686Locale8getKoreaEv_nid_postfix()->fullName, "ko_KR") == 0);
    Check(std::strcmp(_ZN6icu_686Locale9getKoreanEv_nid_postfix()->fullName, "ko") == 0);
    Check(std::strcmp(_ZN6icu_686Locale10getChineseEv_nid_postfix()->fullName, "zh") == 0);
    Check(std::strcmp(_ZN6icu_686Locale20getSimplifiedChineseEv_nid_postfix()->fullName, "zh_CN") == 0);
    Check(std::strcmp(_ZN6icu_686Locale21getTraditionalChineseEv_nid_postfix()->fullName, "zh_TW") == 0);
    Check(std::strcmp(_ZN6icu_686Locale9getFranceEv_nid_postfix()->fullName, "fr_FR") == 0);
    Check(std::strcmp(_ZN6icu_686Locale10getGermanyEv_nid_postfix()->fullName, "de_DE") == 0);
    Check(std::strcmp(_ZN6icu_686Locale8getItalyEv_nid_postfix()->fullName, "it_IT") == 0);
    Check(std::strcmp(_ZN6icu_686Locale8getJapanEv_nid_postfix()->fullName, "ja_JP") == 0);
    Check(std::strcmp(_ZN6icu_686Locale8getChinaEv_nid_postfix()->fullName, "zh_CN") == 0);
    Check(std::strcmp(_ZN6icu_686Locale6getPRCEv_nid_postfix()->fullName, "zh_CN") == 0);
    Check(std::strcmp(_ZN6icu_686Locale9getTaiwanEv_nid_postfix()->fullName, "zh_TW") == 0);
    Check(std::strcmp(_ZN6icu_686Locale5getUKEv_nid_postfix()->fullName, "en_GB") == 0);
    Check(std::strcmp(_ZN6icu_686Locale5getUSEv_nid_postfix()->fullName, "en_US") == 0);
    Check(std::strcmp(_ZN6icu_686Locale9getCanadaEv_nid_postfix()->fullName, "en_CA") == 0);
    Check(std::strcmp(_ZN6icu_686Locale15getCanadaFrenchEv_nid_postfix()->fullName, "fr_CA") == 0);
    Check(_ZN6icu_686Locale14getLocaleCacheEv_nid_postfix() != nullptr);
    Check(std::strcmp(_ZN6icu_686Locale9getLocaleEi_nid_postfix(0)->fullName, "en") == 0);
    std::int32_t length = -1;
    const auto* known = ulocimp_getKnownCanonicalizedLocaleForTest_nid_postfix(&length);
    Check(length == 177 && std::strcmp(known[0], "c") == 0 && std::strcmp(known[length - 1], "zu_ZA") == 0);
    std::puts("ICU locale object tests passed");
}
