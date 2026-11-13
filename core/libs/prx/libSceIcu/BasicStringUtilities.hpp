// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2010-2015, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 2009-2013, International Business Machines Corporation and others. All Rights Reserved.
// Copyright 2004 and onwards Google Inc.
// Copyright (c) 2001-2011, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "UnicodeString.hpp"
#include "CharString.hpp"
#include "MaybeStackArray.hpp"

extern "C" {
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6811ICU_Utility12appendNumberERNS_13UnicodeStringEiii_nid_postfix(Icu::Abi::UnicodeString* result, std::int32_t number, std::int32_t radix, std::int32_t minimum);
std::int8_t APS5_VABI _ZN6icu_6811ICU_Utility17escapeUnprintableERNS_13UnicodeStringEi_nid_postfix(Icu::Abi::UnicodeString* result, std::int32_t point);
std::int32_t APS5_VABI _ZN6icu_6811ICU_Utility14skipWhitespaceERKNS_13UnicodeStringERia_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t* position, std::int8_t advance);
std::int8_t APS5_VABI _ZN6icu_6811ICU_Utility9parseCharERKNS_13UnicodeStringERiDs_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t* position, char16_t unit);
std::int32_t APS5_VABI _ZN6icu_6811ICU_Utility12parsePatternERKNS_13UnicodeStringERKNS_11ReplaceableEii_nid_postfix(const Icu::Abi::UnicodeString* pattern, const void* text, std::int32_t index, std::int32_t limit);
char* APS5_VABI _ZNK6icu_6810CharString9cloneDataER10UErrorCode_nid_postfix(const Icu::Abi::CharString* self, std::int32_t* status);
char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(Icu::Abi::MaybeStackArray40* self, std::int32_t length, std::int32_t* capacity);
std::int32_t APS5_VABI _ZN6icu_6811StringPiece4findES0_i_nid_postfix(const Icu::Abi::StringPiece* self, Icu::Abi::StringPiece needle, std::int32_t offset);
void APS5_VABI _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringEiaaS2__nid_postfix(Icu::Abi::UnicodeString* rule, std::int32_t c, std::int8_t isLiteral, std::int8_t escapeUnprintable, Icu::Abi::UnicodeString* quoteBuf);
void APS5_VABI _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringERKS1_aaS2__nid_postfix(Icu::Abi::UnicodeString* rule, const Icu::Abi::UnicodeString* text, std::int8_t isLiteral, std::int8_t escapeUnprintable, Icu::Abi::UnicodeString* quoteBuf);
void APS5_VABI _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringEPKNS_14UnicodeMatcherEaS2__nid_postfix(Icu::Abi::UnicodeString* rule, const void* matcher, std::int8_t escapeUnprintable, Icu::Abi::UnicodeString* quoteBuf);
}
