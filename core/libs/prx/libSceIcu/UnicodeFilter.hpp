// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (c) 2001-2012, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (c) 2002-2004, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"

namespace Icu::Abi {

struct UnicodeMatcher {
    const GuestFunction* vtable;
};
static_assert(sizeof(UnicodeMatcher) == 8 && alignof(UnicodeMatcher) == 8);

struct UnicodeFunctor {
    const GuestFunction* vtable;
};
static_assert(sizeof(UnicodeFunctor) == 8 && alignof(UnicodeFunctor) == 8);

struct UnicodeFilter {
    const GuestFunction* vtable;
    const GuestFunction* matcherVtable;
};
static_assert(sizeof(UnicodeFilter) == 16 && alignof(UnicodeFilter) == 8);
static_assert(offsetof(UnicodeFilter, vtable) == 0);
static_assert(offsetof(UnicodeFilter, matcherVtable) == 8);

struct SymbolTable {
    const GuestFunction* vtable;
};
static_assert(sizeof(SymbolTable) == 8 && alignof(SymbolTable) == 8);

}

extern "C" {
void APS5_VABI _ZN6icu_6814UnicodeMatcherD0Ev_nid_postfix(Icu::Abi::UnicodeMatcher* self);
void APS5_VABI _ZN6icu_6814UnicodeMatcherD1Ev_nid_postfix(Icu::Abi::UnicodeMatcher* self);
void APS5_VABI _ZN6icu_6814UnicodeMatcherD2Ev_nid_postfix(Icu::Abi::UnicodeMatcher* self);

void APS5_VABI _ZN6icu_6814UnicodeFunctorD0Ev_nid_postfix(Icu::Abi::UnicodeFunctor* self);
void APS5_VABI _ZN6icu_6814UnicodeFunctorD1Ev_nid_postfix(Icu::Abi::UnicodeFunctor* self);
void APS5_VABI _ZN6icu_6814UnicodeFunctorD2Ev_nid_postfix(Icu::Abi::UnicodeFunctor* self);
void* APS5_VABI _ZN6icu_6814UnicodeFunctor16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6814UnicodeFunctor9toMatcherEv_nid_postfix(const Icu::Abi::UnicodeFunctor* self);
void* APS5_VABI _ZNK6icu_6814UnicodeFunctor10toReplacerEv_nid_postfix(const Icu::Abi::UnicodeFunctor* self);

void APS5_VABI _ZN6icu_6813UnicodeFilterD0Ev_nid_postfix(Icu::Abi::UnicodeFilter* self);
void APS5_VABI _ZN6icu_6813UnicodeFilterD1Ev_nid_postfix(Icu::Abi::UnicodeFilter* self);
void APS5_VABI _ZN6icu_6813UnicodeFilterD2Ev_nid_postfix(Icu::Abi::UnicodeFilter* self);
void APS5_VABI _ZThn8_N6icu_6813UnicodeFilterD0Ev_nid_postfix(void* self);
void APS5_VABI _ZThn8_N6icu_6813UnicodeFilterD1Ev_nid_postfix(void* self);
void* APS5_VABI _ZN6icu_6813UnicodeFilter16getStaticClassIDEv_nid_postfix();
void APS5_VABI _ZN6icu_6813UnicodeFilter7setDataEPKNS_23TransliterationRuleDataE_nid_postfix(Icu::Abi::UnicodeFilter* self, const void* data);
void* APS5_VABI _ZNK6icu_6813UnicodeFilter9toMatcherEv_nid_postfix(const Icu::Abi::UnicodeFilter* self);
std::int32_t APS5_VABI _ZN6icu_6813UnicodeFilter7matchesERKNS_11ReplaceableERiia_nid_postfix(const Icu::Abi::UnicodeFilter* self, const void* text, std::int32_t* offset, std::int32_t limit, std::int8_t incremental);
std::int32_t APS5_VABI _ZThn8_N6icu_6813UnicodeFilter7matchesERKNS_11ReplaceableERiia_nid_postfix(const void* self, const void* text, std::int32_t* offset, std::int32_t limit, std::int8_t incremental);

void APS5_VABI _ZN6icu_6811SymbolTableD0Ev_nid_postfix(Icu::Abi::SymbolTable* self);
void APS5_VABI _ZN6icu_6811SymbolTableD1Ev_nid_postfix(Icu::Abi::SymbolTable* self);
void APS5_VABI _ZN6icu_6811SymbolTableD2Ev_nid_postfix(Icu::Abi::SymbolTable* self);
}
