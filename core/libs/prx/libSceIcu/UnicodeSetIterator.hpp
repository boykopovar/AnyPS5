// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (c) 2002-2006, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "UnicodeSet.hpp"

namespace Icu::Abi {
struct UnicodeSetIterator {
    const GuestFunction* vtable;
    std::int32_t codepoint;
    std::int32_t codepointEnd;
    const UnicodeString* string;
    const UnicodeSet* set;
    std::int32_t endRange;
    std::int32_t range;
    std::int32_t endElement;
    std::int32_t nextElement;
    std::int32_t nextString;
    std::int32_t stringCount;
    UnicodeString* cpString;
};
static_assert(sizeof(UnicodeSetIterator) == 64 && alignof(UnicodeSetIterator) == 8);
static_assert(offsetof(UnicodeSetIterator, codepoint) == 8 && offsetof(UnicodeSetIterator, codepointEnd) == 12);
static_assert(offsetof(UnicodeSetIterator, string) == 16 && offsetof(UnicodeSetIterator, set) == 24);
static_assert(offsetof(UnicodeSetIterator, endRange) == 32 && offsetof(UnicodeSetIterator, range) == 36);
static_assert(offsetof(UnicodeSetIterator, endElement) == 40 && offsetof(UnicodeSetIterator, nextElement) == 44);
static_assert(offsetof(UnicodeSetIterator, nextString) == 48 && offsetof(UnicodeSetIterator, stringCount) == 52);
static_assert(offsetof(UnicodeSetIterator, cpString) == 56);
}

extern "C" {
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorC1Ev_nid_postfix(Icu::Abi::UnicodeSetIterator*);
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorC2Ev_nid_postfix(Icu::Abi::UnicodeSetIterator*);
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorC1ERKNS_10UnicodeSetE_nid_postfix(Icu::Abi::UnicodeSetIterator*, const Icu::Abi::UnicodeSet*);
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorC2ERKNS_10UnicodeSetE_nid_postfix(Icu::Abi::UnicodeSetIterator*, const Icu::Abi::UnicodeSet*);
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorD1Ev_nid_postfix(Icu::Abi::UnicodeSetIterator*);
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorD2Ev_nid_postfix(Icu::Abi::UnicodeSetIterator*);
void APS5_VABI _ZN6icu_6818UnicodeSetIteratorD0Ev_nid_postfix(Icu::Abi::UnicodeSetIterator*);
void APS5_VABI _ZN6icu_6818UnicodeSetIterator5resetEv_nid_postfix(Icu::Abi::UnicodeSetIterator*);
void APS5_VABI _ZN6icu_6818UnicodeSetIterator5resetERKNS_10UnicodeSetE_nid_postfix(Icu::Abi::UnicodeSetIterator*, const Icu::Abi::UnicodeSet*);
void APS5_VABI _ZN6icu_6818UnicodeSetIterator9loadRangeEi_nid_postfix(Icu::Abi::UnicodeSetIterator*, std::int32_t);
std::int8_t APS5_VABI _ZN6icu_6818UnicodeSetIterator4nextEv_nid_postfix(Icu::Abi::UnicodeSetIterator*);
std::int8_t APS5_VABI _ZN6icu_6818UnicodeSetIterator9nextRangeEv_nid_postfix(Icu::Abi::UnicodeSetIterator*);
const Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6818UnicodeSetIterator9getStringEv_nid_postfix(Icu::Abi::UnicodeSetIterator*);
void* APS5_VABI _ZN6icu_6818UnicodeSetIterator16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6818UnicodeSetIterator17getDynamicClassIDEv_nid_postfix(const Icu::Abi::UnicodeSetIterator*);
}
