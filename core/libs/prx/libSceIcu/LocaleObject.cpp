// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (c) 2004-2014, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <mutex>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
using namespace Icu::Abi;

template<typename TValue>
TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU locale: null pointer");
    return *value;
}

char* Duplicate(const char* source) {
    Require(source);
    const auto size = std::strlen(source) + 1;
    auto* result = static_cast<char*>(Allocate_nid_no_patch(size));
    if (result) std::memcpy(result, source, size);
    return result;
}

void Release(Locale& locale) {
    if (locale.baseName != locale.fullName) Free_nid_no_patch(locale.baseName);
    if (locale.fullName != locale.fullNameBuffer) Free_nid_no_patch(locale.fullName);
}

const DerivedTypeInfo LocaleType{{SingleTypeTable_nid_no_patch(), "N6icu_686LocaleE"}, Rtti_nid_no_patch(Class::Object)};
struct LocaleTable {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    GuestFunction slots[3];
};
static_assert(sizeof(LocaleTable) == 40 && alignof(LocaleTable) == 8);
static_assert(offsetof(LocaleTable, offset) == 0 && offsetof(LocaleTable, type) == 8 && offsetof(LocaleTable, slots) == 16);
const LocaleTable Table{0, &LocaleType.type, {
    reinterpret_cast<GuestFunction>(_ZN6icu_686LocaleD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_686LocaleD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_686Locale17getDynamicClassIDEv_nid_postfix)
}};
std::int8_t APS5_VABI IteratorHasNext_nid_no_patch(const LocaleIterator*) { throw std::invalid_argument("ICU locale iterator: pure virtual call"); }
const Locale* APS5_VABI IteratorNext_nid_no_patch(LocaleIterator*) { throw std::invalid_argument("ICU locale iterator: pure virtual call"); }
const TypeInfo IteratorType{ClassTypeTable_nid_no_patch(), "N6icu_686Locale8IteratorE"};
struct IteratorTable {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    GuestFunction slots[4];
};
static_assert(sizeof(IteratorTable) == 48 && alignof(IteratorTable) == 8);
static_assert(offsetof(IteratorTable, offset) == 0 && offsetof(IteratorTable, type) == 8 && offsetof(IteratorTable, slots) == 16);
const IteratorTable IteratorFunctions{0, &IteratorType, {
    nullptr,
    nullptr,
    reinterpret_cast<GuestFunction>(IteratorHasNext_nid_no_patch),
    reinterpret_cast<GuestFunction>(IteratorNext_nid_no_patch)
}};
char ClassId;

void Initialize(Locale* locale) {
    Require(locale).vtable = Table.slots;
    locale->fullName = locale->fullNameBuffer;
    locale->baseName = nullptr;
}

struct StaticLocaleData {
    const char* lang;
    const char* country;
};
const StaticLocaleData CacheData[19]{
    {"en", ""},
    {"fr", ""},
    {"de", ""},
    {"it", ""},
    {"ja", ""},
    {"ko", ""},
    {"zh", ""},
    {"fr", "FR"},
    {"de", "DE"},
    {"it", "IT"},
    {"ja", "JP"},
    {"ko", "KR"},
    {"zh", "CN"},
    {"zh", "TW"},
    {"en", "GB"},
    {"en", "US"},
    {"en", "CA"},
    {"fr", "CA"},
    {"", ""},
};

Locale Cache[19];
std::once_flag CacheOnce;

Locale* EnsureLocaleCache() {
    std::call_once(CacheOnce, [] {
        for (int i = 0; i < 19; ++i) {
            auto& loc = Cache[i];
            loc.vtable = Table.slots;
            std::strcpy(loc.language, CacheData[i].lang);
            loc.script[0] = 0;
            std::strcpy(loc.country, CacheData[i].country);
            loc.fullName = loc.fullNameBuffer;
            if (CacheData[i].country[0]) {
                std::strcpy(loc.fullNameBuffer, CacheData[i].lang);
                std::strcat(loc.fullNameBuffer, "_");
                std::strcat(loc.fullNameBuffer, CacheData[i].country);
            } else {
                std::strcpy(loc.fullNameBuffer, CacheData[i].lang);
            }
            loc.baseName = loc.fullName;
            loc.variantBegin = CheckedInt_nid_no_patch(std::strlen(loc.fullNameBuffer));
            loc.isBogus = 0;
        }
    });
    return Cache;
}

}

namespace Icu::Abi {
const GuestFunction* LocaleIteratorTable_nid_no_patch() { return IteratorFunctions.slots; }
}

extern "C" {

void APS5_VABI _ZN6icu_686LocaleC1ENS0_11ELocaleTypeE_nid_postfix(Locale* locale, std::int32_t) {
    Initialize(locale);
    _ZN6icu_686Locale10setToBogusEv_nid_postfix(locale);
}
void APS5_VABI _ZN6icu_686LocaleC2ENS0_11ELocaleTypeE_nid_postfix(Locale* locale, std::int32_t type) {
    _ZN6icu_686LocaleC1ENS0_11ELocaleTypeE_nid_postfix(locale, type);
}

void APS5_VABI _ZN6icu_686Locale10setToBogusEv_nid_postfix(Locale* locale) {
    Release(Require(locale));
    locale->baseName = nullptr;
    locale->fullName = locale->fullNameBuffer;
    locale->fullNameBuffer[0] = 0;
    locale->language[0] = locale->script[0] = locale->country[0] = 0;
    locale->isBogus = 1;
    locale->variantBegin = 0;
}

void APS5_VABI _ZN6icu_686LocaleD1Ev_nid_postfix(Locale* locale) {
    auto& value = Require(locale);
    if (value.baseName != value.fullName) Free_nid_no_patch(value.baseName);
    value.baseName = nullptr;
    if (value.fullName != value.fullNameBuffer) {
        Free_nid_no_patch(value.fullName);
        value.fullName = nullptr;
    }
    _ZN6icu_687UObjectD1Ev_nid_postfix(locale);
}
void APS5_VABI _ZN6icu_686LocaleD2Ev_nid_postfix(Locale* locale) { _ZN6icu_686LocaleD1Ev_nid_postfix(locale); }
void APS5_VABI _ZN6icu_686LocaleD0Ev_nid_postfix(Locale* locale) {
    _ZN6icu_686LocaleD1Ev_nid_postfix(locale);
    Free_nid_no_patch(locale);
}

Locale* APS5_VABI _ZN6icu_686LocaleaSERKS0__nid_postfix(Locale* locale, const Locale* source) {
    Require(locale); Require(source);
    if (locale == source) return locale;
    _ZN6icu_686Locale10setToBogusEv_nid_postfix(locale);
    if (source->fullName == source->fullNameBuffer) std::strcpy(locale->fullNameBuffer, source->fullNameBuffer);
    else if (!source->fullName) locale->fullName = nullptr;
    else {
        locale->fullName = Duplicate(source->fullName);
        if (!locale->fullName) return locale;
    }
    if (source->baseName == source->fullName) locale->baseName = locale->fullName;
    else if (source->baseName) {
        locale->baseName = Duplicate(source->baseName);
        if (!locale->baseName) return locale;
    }
    std::strcpy(locale->language, source->language);
    std::strcpy(locale->script, source->script);
    std::strcpy(locale->country, source->country);
    locale->variantBegin = source->variantBegin;
    locale->isBogus = source->isBogus;
    return locale;
}

Locale* APS5_VABI _ZN6icu_686LocaleaSEOS0__nid_postfix(Locale* locale, Locale* source) {
    Require(locale); Require(source);
    if (locale == source) throw std::invalid_argument("ICU locale: self move");
    Release(*locale);
    if (source->fullName == source->fullNameBuffer) {
        std::strcpy(locale->fullNameBuffer, source->fullNameBuffer);
        locale->fullName = locale->fullNameBuffer;
    } else locale->fullName = source->fullName;
    locale->baseName = source->baseName == source->fullName ? locale->fullName : source->baseName;
    std::strcpy(locale->language, source->language);
    std::strcpy(locale->script, source->script);
    std::strcpy(locale->country, source->country);
    locale->variantBegin = source->variantBegin;
    locale->isBogus = source->isBogus;
    source->baseName = source->fullName = source->fullNameBuffer;
    return locale;
}

void APS5_VABI _ZN6icu_686LocaleC1ERKS0__nid_postfix(Locale* locale, const Locale* source) {
    Initialize(locale);
    _ZN6icu_686LocaleaSERKS0__nid_postfix(locale, source);
}
void APS5_VABI _ZN6icu_686LocaleC2ERKS0__nid_postfix(Locale* locale, const Locale* source) { _ZN6icu_686LocaleC1ERKS0__nid_postfix(locale, source); }
void APS5_VABI _ZN6icu_686LocaleC1EOS0__nid_postfix(Locale* locale, Locale* source) {
    Initialize(locale);
    locale->baseName = locale->fullName;
    _ZN6icu_686LocaleaSEOS0__nid_postfix(locale, source);
}
void APS5_VABI _ZN6icu_686LocaleC2EOS0__nid_postfix(Locale* locale, Locale* source) { _ZN6icu_686LocaleC1EOS0__nid_postfix(locale, source); }

Locale* APS5_VABI _ZNK6icu_686Locale5cloneEv_nid_postfix(const Locale* locale) {
    Require(locale);
    auto* result = static_cast<Locale*>(Allocate_nid_no_patch(sizeof(Locale)));
    if (result) _ZN6icu_686LocaleC1ERKS0__nid_postfix(result, locale);
    return result;
}
std::int8_t APS5_VABI _ZNK6icu_686LocaleeqERKS0__nid_postfix(const Locale* locale, const Locale* other) {
    return std::strcmp(&Require(Require(other).fullName), &Require(Require(locale).fullName)) == 0;
}
std::int32_t APS5_VABI _ZNK6icu_686Locale8hashCodeEv_nid_postfix(const Locale* locale) {
    auto* name = &Require(Require(locale).fullName);
    return ustr_hashCharsN_nid_postfix(name, CheckedInt_nid_no_patch(std::strlen(name)));
}
const char* APS5_VABI _ZNK6icu_686Locale11getBaseNameEv_nid_postfix(const Locale* locale) { return Require(locale).baseName; }
void* APS5_VABI _ZN6icu_686Locale16getStaticClassIDEv_nid_postfix() { return &ClassId; }
void* APS5_VABI _ZNK6icu_686Locale17getDynamicClassIDEv_nid_postfix(const Locale*) { return &ClassId; }
const char* const* APS5_VABI _ZN6icu_686Locale15getISOLanguagesEv_nid_postfix() { return uloc_getISOLanguages_nid_postfix(); }
const char* const* APS5_VABI _ZN6icu_686Locale15getISOCountriesEv_nid_postfix() { return uloc_getISOCountries_nid_postfix(); }
const char* APS5_VABI _ZNK6icu_686Locale15getISO3LanguageEv_nid_postfix(const Locale* locale) { return uloc_getISO3Language_nid_postfix(Require(locale).fullName); }
const char* APS5_VABI _ZNK6icu_686Locale14getISO3CountryEv_nid_postfix(const Locale* locale) { return uloc_getISO3Country_nid_postfix(Require(locale).fullName); }

void APS5_VABI _ZN6icu_686Locale12initBaseNameER10UErrorCode_nid_postfix(Locale* locale, std::int32_t* status) {
    if (Require(status) > 0) return;
    const auto* name = &Require(Require(locale).fullName);
    const auto* at = std::strchr(name, '@');
    const auto* equal = std::strchr(name, '=');
    if (at && equal && at < equal) {
        const auto length = CheckedInt_nid_no_patch(at - name);
        locale->baseName = static_cast<char*>(Allocate_nid_no_patch(static_cast<std::size_t>(length) + 1));
        if (!locale->baseName) { *status = 7; return; }
        std::memcpy(locale->baseName, name, length);
        locale->baseName[length] = 0;
        locale->variantBegin = std::min(locale->variantBegin, length);
    } else locale->baseName = locale->fullName;
}

const char* APS5_VABI _ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(const LocaleBased* based, std::int32_t type, std::int32_t* status) {
    if (Require(status) > 0) return nullptr;
    if (type == 0) return Require(based).actual;
    if (type == 1) return Require(based).valid;
    *status = 1;
    return nullptr;
}
void APS5_VABI _ZN6icu_6811LocaleBased12setLocaleIDsEPKcS2__nid_postfix(LocaleBased* based, const char* valid, const char* actual) {
    if (valid) {
        std::strncpy(&Require(Require(based).valid), valid, 157);
        based->valid[156] = 0;
    }
    if (actual) {
        std::strncpy(&Require(Require(based).actual), actual, 157);
        based->actual[156] = 0;
    }
}
void APS5_VABI _ZN6icu_6811LocaleBased12setLocaleIDsERKNS_6LocaleES3__nid_postfix(LocaleBased* based, const Locale* valid, const Locale* actual) {
    std::strcpy(&Require(Require(based).valid), &Require(Require(valid).fullName));
    std::strcpy(&Require(Require(based).actual), &Require(Require(actual).fullName));
}

void APS5_VABI _ZN6icu_686Locale8IteratorD1Ev_nid_postfix(LocaleIterator* iterator) {
    Require(iterator);
}
void APS5_VABI _ZN6icu_686Locale8IteratorD2Ev_nid_postfix(LocaleIterator* iterator) { _ZN6icu_686Locale8IteratorD1Ev_nid_postfix(iterator); }
void APS5_VABI _ZN6icu_686Locale8IteratorD0Ev_nid_postfix(LocaleIterator* iterator) {
    _ZN6icu_686Locale8IteratorD1Ev_nid_postfix(iterator);
    Free_nid_no_patch(iterator);
}

Locale* APS5_VABI _ZN6icu_686Locale14getLocaleCacheEv_nid_postfix() {
    return EnsureLocaleCache();
}
const Locale* APS5_VABI _ZN6icu_686Locale9getLocaleEi_nid_postfix(std::int32_t locid) {
    if (locid < 0 || locid >= 19) throw std::invalid_argument("ICU locale: invalid index");
    return &EnsureLocaleCache()[locid];
}
const Locale* APS5_VABI _ZN6icu_686Locale7getRootEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(18); }
const Locale* APS5_VABI _ZN6icu_686Locale10getEnglishEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(0); }
const Locale* APS5_VABI _ZN6icu_686Locale9getFrenchEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(1); }
const Locale* APS5_VABI _ZN6icu_686Locale9getGermanEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(2); }
const Locale* APS5_VABI _ZN6icu_686Locale10getItalianEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(3); }
const Locale* APS5_VABI _ZN6icu_686Locale11getJapaneseEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(4); }
const Locale* APS5_VABI _ZN6icu_686Locale8getKoreaEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(11); }
const Locale* APS5_VABI _ZN6icu_686Locale9getKoreanEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(5); }
const Locale* APS5_VABI _ZN6icu_686Locale10getChineseEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(6); }
const Locale* APS5_VABI _ZN6icu_686Locale20getSimplifiedChineseEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(12); }
const Locale* APS5_VABI _ZN6icu_686Locale21getTraditionalChineseEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(13); }
const Locale* APS5_VABI _ZN6icu_686Locale9getFranceEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(7); }
const Locale* APS5_VABI _ZN6icu_686Locale10getGermanyEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(8); }
const Locale* APS5_VABI _ZN6icu_686Locale8getItalyEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(9); }
const Locale* APS5_VABI _ZN6icu_686Locale8getJapanEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(10); }
const Locale* APS5_VABI _ZN6icu_686Locale8getChinaEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(12); }
const Locale* APS5_VABI _ZN6icu_686Locale6getPRCEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(12); }
const Locale* APS5_VABI _ZN6icu_686Locale9getTaiwanEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(13); }
const Locale* APS5_VABI _ZN6icu_686Locale5getUKEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(14); }
const Locale* APS5_VABI _ZN6icu_686Locale5getUSEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(15); }
const Locale* APS5_VABI _ZN6icu_686Locale9getCanadaEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(16); }
const Locale* APS5_VABI _ZN6icu_686Locale15getCanadaFrenchEv_nid_postfix() { return _ZN6icu_686Locale9getLocaleEi_nid_postfix(17); }

}
