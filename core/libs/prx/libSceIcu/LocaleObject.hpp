// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (c) 2004-2014, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu::Abi {
struct LocaleIterator { const GuestFunction* vtable; };
static_assert(sizeof(LocaleIterator) == 8 && alignof(LocaleIterator) == 8 && offsetof(LocaleIterator, vtable) == 0);
const GuestFunction* LocaleIteratorTable_nid_no_patch();

struct LocaleBased { char* valid; char* actual; };
static_assert(sizeof(LocaleBased) == 16 && alignof(LocaleBased) == 8);
static_assert(offsetof(LocaleBased, valid) == 0 && offsetof(LocaleBased, actual) == 8);

struct Locale {
    const GuestFunction* vtable;
    char language[12];
    char script[6];
    char country[4];
    std::int32_t variantBegin;
    char* fullName;
    char fullNameBuffer[157];
    char* baseName;
    std::int8_t isBogus;
};
static_assert(sizeof(Locale) == 224 && alignof(Locale) == 8);
static_assert(offsetof(Locale, vtable) == 0);
static_assert(offsetof(Locale, language) == 8);
static_assert(offsetof(Locale, script) == 20);
static_assert(offsetof(Locale, country) == 26);
static_assert(offsetof(Locale, variantBegin) == 32);
static_assert(offsetof(Locale, fullName) == 40);
static_assert(offsetof(Locale, fullNameBuffer) == 48);
static_assert(offsetof(Locale, baseName) == 208);
static_assert(offsetof(Locale, isBogus) == 216);
}

extern "C" {
void APS5_VABI _ZN6icu_686LocaleC1ENS0_11ELocaleTypeE_nid_postfix(Icu::Abi::Locale*, std::int32_t);
void APS5_VABI _ZN6icu_686LocaleC2ENS0_11ELocaleTypeE_nid_postfix(Icu::Abi::Locale*, std::int32_t);
void APS5_VABI _ZN6icu_686LocaleD1Ev_nid_postfix(Icu::Abi::Locale*);
void APS5_VABI _ZN6icu_686LocaleD2Ev_nid_postfix(Icu::Abi::Locale*);
void APS5_VABI _ZN6icu_686LocaleD0Ev_nid_postfix(Icu::Abi::Locale*);
void APS5_VABI _ZN6icu_686LocaleC1ERKS0__nid_postfix(Icu::Abi::Locale*, const Icu::Abi::Locale*);
void APS5_VABI _ZN6icu_686LocaleC2ERKS0__nid_postfix(Icu::Abi::Locale*, const Icu::Abi::Locale*);
void APS5_VABI _ZN6icu_686LocaleC1EOS0__nid_postfix(Icu::Abi::Locale*, Icu::Abi::Locale*);
void APS5_VABI _ZN6icu_686LocaleC2EOS0__nid_postfix(Icu::Abi::Locale*, Icu::Abi::Locale*);
Icu::Abi::Locale* APS5_VABI _ZN6icu_686LocaleaSERKS0__nid_postfix(Icu::Abi::Locale*, const Icu::Abi::Locale*);
Icu::Abi::Locale* APS5_VABI _ZN6icu_686LocaleaSEOS0__nid_postfix(Icu::Abi::Locale*, Icu::Abi::Locale*);
Icu::Abi::Locale* APS5_VABI _ZNK6icu_686Locale5cloneEv_nid_postfix(const Icu::Abi::Locale*);
std::int8_t APS5_VABI _ZNK6icu_686LocaleeqERKS0__nid_postfix(const Icu::Abi::Locale*, const Icu::Abi::Locale*);
std::int32_t APS5_VABI _ZNK6icu_686Locale8hashCodeEv_nid_postfix(const Icu::Abi::Locale*);
void APS5_VABI _ZN6icu_686Locale10setToBogusEv_nid_postfix(Icu::Abi::Locale*);
void APS5_VABI _ZN6icu_686Locale12initBaseNameER10UErrorCode_nid_postfix(Icu::Abi::Locale*, std::int32_t*);
const char* APS5_VABI _ZNK6icu_686Locale11getBaseNameEv_nid_postfix(const Icu::Abi::Locale*);
void* APS5_VABI _ZN6icu_686Locale16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_686Locale17getDynamicClassIDEv_nid_postfix(const Icu::Abi::Locale*);
const char* const* APS5_VABI _ZN6icu_686Locale15getISOLanguagesEv_nid_postfix();
const char* const* APS5_VABI _ZN6icu_686Locale15getISOCountriesEv_nid_postfix();
const char* APS5_VABI _ZNK6icu_686Locale15getISO3LanguageEv_nid_postfix(const Icu::Abi::Locale*);
const char* APS5_VABI _ZNK6icu_686Locale14getISO3CountryEv_nid_postfix(const Icu::Abi::Locale*);
const char* APS5_VABI _ZNK6icu_6811LocaleBased11getLocaleIDE18ULocDataLocaleTypeR10UErrorCode_nid_postfix(const Icu::Abi::LocaleBased*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_6811LocaleBased12setLocaleIDsEPKcS2__nid_postfix(Icu::Abi::LocaleBased*, const char*, const char*);
void APS5_VABI _ZN6icu_6811LocaleBased12setLocaleIDsERKNS_6LocaleES3__nid_postfix(Icu::Abi::LocaleBased*, const Icu::Abi::Locale*, const Icu::Abi::Locale*);
void APS5_VABI _ZN6icu_686Locale8IteratorD1Ev_nid_postfix(Icu::Abi::LocaleIterator*);
void APS5_VABI _ZN6icu_686Locale8IteratorD2Ev_nid_postfix(Icu::Abi::LocaleIterator*);
void APS5_VABI _ZN6icu_686Locale8IteratorD0Ev_nid_postfix(Icu::Abi::LocaleIterator*);
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale7getRootEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale10getEnglishEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale9getFrenchEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale9getGermanEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale10getItalianEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale11getJapaneseEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale8getKoreaEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale9getKoreanEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale10getChineseEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale20getSimplifiedChineseEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale21getTraditionalChineseEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale9getFranceEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale10getGermanyEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale8getItalyEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale8getJapanEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale8getChinaEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale6getPRCEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale9getTaiwanEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale5getUKEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale5getUSEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale9getCanadaEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale15getCanadaFrenchEv_nid_postfix();
Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale14getLocaleCacheEv_nid_postfix();
const Icu::Abi::Locale* APS5_VABI _ZN6icu_686Locale9getLocaleEi_nid_postfix(std::int32_t);
const char* const* APS5_VABI ulocimp_getKnownCanonicalizedLocaleForTest_nid_postfix(std::int32_t* length);
}
