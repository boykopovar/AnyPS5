// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1999-2016, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"

extern "C" {
std::int32_t APS5_VABI uhash_hashUnicodeString_nid_postfix(Icu::Abi::Element key) {
    return key.pointer ? _ZNK6icu_6813UnicodeString10doHashCodeEv_nid_postfix(static_cast<const Icu::Abi::UnicodeString*>(key.pointer)) : 0;
}

std::int8_t APS5_VABI uhash_compareUnicodeString_nid_postfix(Icu::Abi::Element first, Icu::Abi::Element second) {
    if (first.pointer == second.pointer) return 1;
    if (!first.pointer || !second.pointer) return 0;
    return _ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(static_cast<const Icu::Abi::UnicodeString*>(first.pointer), static_cast<const Icu::Abi::UnicodeString*>(second.pointer));
}
}
