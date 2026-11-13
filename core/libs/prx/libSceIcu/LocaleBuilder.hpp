// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2009-2015, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "LocaleObject.hpp"
#include "CharString.hpp"

namespace Icu::Abi {
struct LocaleBuilder {
    const GuestFunction* vtable;
    std::int32_t status;
    char language[9];
    char script[5];
    char region[4];
    CharString* variant;
    Locale* extensions;
};
static_assert(sizeof(LocaleBuilder) == 48 && alignof(LocaleBuilder) == 8);
static_assert(offsetof(LocaleBuilder, vtable) == 0 && offsetof(LocaleBuilder, status) == 8);
static_assert(offsetof(LocaleBuilder, language) == 12 && offsetof(LocaleBuilder, script) == 21);
static_assert(offsetof(LocaleBuilder, region) == 26 && offsetof(LocaleBuilder, variant) == 32);
static_assert(offsetof(LocaleBuilder, extensions) == 40);
}

extern "C" {
void APS5_VABI _ZN6icu_6813LocaleBuilderC1Ev_nid_postfix(Icu::Abi::LocaleBuilder*);
void APS5_VABI _ZN6icu_6813LocaleBuilderC2Ev_nid_postfix(Icu::Abi::LocaleBuilder*);
void APS5_VABI _ZN6icu_6813LocaleBuilderD1Ev_nid_postfix(Icu::Abi::LocaleBuilder*);
void APS5_VABI _ZN6icu_6813LocaleBuilderD2Ev_nid_postfix(Icu::Abi::LocaleBuilder*);
void APS5_VABI _ZN6icu_6813LocaleBuilderD0Ev_nid_postfix(Icu::Abi::LocaleBuilder*);
Icu::Abi::LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder11setLanguageENS_11StringPieceE_nid_postfix(Icu::Abi::LocaleBuilder*, Icu::Abi::StringPiece);
Icu::Abi::LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder9setScriptENS_11StringPieceE_nid_postfix(Icu::Abi::LocaleBuilder*, Icu::Abi::StringPiece);
Icu::Abi::LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder9setRegionENS_11StringPieceE_nid_postfix(Icu::Abi::LocaleBuilder*, Icu::Abi::StringPiece);
Icu::Abi::LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder10setVariantENS_11StringPieceE_nid_postfix(Icu::Abi::LocaleBuilder*, Icu::Abi::StringPiece);
Icu::Abi::LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder5clearEv_nid_postfix(Icu::Abi::LocaleBuilder*);
Icu::Abi::LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder15clearExtensionsEv_nid_postfix(Icu::Abi::LocaleBuilder*);
std::int8_t APS5_VABI _ZNK6icu_6813LocaleBuilder11copyErrorToER10UErrorCode_nid_postfix(const Icu::Abi::LocaleBuilder*, std::int32_t*);
Icu::Abi::LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder9setLocaleERKNS_6LocaleE_nid_postfix(Icu::Abi::LocaleBuilder* builder, const Icu::Abi::Locale* locale);
}
