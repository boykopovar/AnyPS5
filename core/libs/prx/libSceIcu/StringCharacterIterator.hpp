// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1998-2012, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "CharacterIteratorObjects.hpp"
#include "UnicodeString.hpp"

namespace Icu::Abi {
struct StringCharacterIterator {
    UCharCharacterIterator base;
    UnicodeString text;
};
static_assert(sizeof(StringCharacterIterator) == 96 && alignof(StringCharacterIterator) == 8);
static_assert(offsetof(StringCharacterIterator, base) == 0 && offsetof(StringCharacterIterator, text) == 32);
}

extern "C" {
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1ERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1ERKNS_13UnicodeStringEi_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source, std::int32_t position);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1ERKNS_13UnicodeStringEiii_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source, std::int32_t begin, std::int32_t end, std::int32_t position);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1ERKS0__nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::StringCharacterIterator* source);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2ERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2ERKNS_13UnicodeStringEi_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source, std::int32_t position);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2ERKNS_13UnicodeStringEiii_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source, std::int32_t begin, std::int32_t end, std::int32_t position);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2ERKS0__nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::StringCharacterIterator* source);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorD1Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorD2Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self);
void APS5_VABI _ZN6icu_6823StringCharacterIteratorD0Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self);
Icu::Abi::StringCharacterIterator* APS5_VABI _ZN6icu_6823StringCharacterIteratoraSERKS0__nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::StringCharacterIterator* source);
std::int8_t APS5_VABI _ZNK6icu_6823StringCharacterIteratoreqERKNS_24ForwardCharacterIteratorE_nid_postfix(const Icu::Abi::StringCharacterIterator* self, const Icu::Abi::ForwardCharacterIterator* other);
Icu::Abi::StringCharacterIterator* APS5_VABI _ZNK6icu_6823StringCharacterIterator5cloneEv_nid_postfix(const Icu::Abi::StringCharacterIterator* self);
void APS5_VABI _ZN6icu_6823StringCharacterIterator7setTextERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source);
void APS5_VABI _ZN6icu_6823StringCharacterIterator7getTextERNS_13UnicodeStringE_nid_postfix(Icu::Abi::StringCharacterIterator* self, Icu::Abi::UnicodeString* result);
void* APS5_VABI _ZN6icu_6823StringCharacterIterator16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6823StringCharacterIterator17getDynamicClassIDEv_nid_postfix(const Icu::Abi::StringCharacterIterator*);
}
