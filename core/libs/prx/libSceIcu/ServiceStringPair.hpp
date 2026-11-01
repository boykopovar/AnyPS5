// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2014, International Business Machines Corporation. All Rights Reserved.
// Copyright (C) 2001-2011, International Business Machines Corporation. All Rights Reserved.
#pragma once
#include "Object.hpp"

namespace Icu::Abi {
struct StringPair { alignas(8) char padding[8]; UnicodeString displayName; UnicodeString id; };
static_assert(sizeof(StringPair) == 136 && alignof(StringPair) == 8);
static_assert(offsetof(StringPair, displayName) == 8 && offsetof(StringPair, id) == 72);
}

extern "C" {
void APS5_VABI _ZN6icu_6810StringPairC1ERKNS_13UnicodeStringES3__nid_postfix(Icu::Abi::StringPair* self, const Icu::Abi::UnicodeString* displayName, const Icu::Abi::UnicodeString* id);
void APS5_VABI _ZN6icu_6810StringPairC2ERKNS_13UnicodeStringES3__nid_postfix(Icu::Abi::StringPair* self, const Icu::Abi::UnicodeString* displayName, const Icu::Abi::UnicodeString* id);
std::int8_t APS5_VABI _ZNK6icu_6810StringPair7isBogusEv_nid_postfix(const Icu::Abi::StringPair* self);
Icu::Abi::StringPair* APS5_VABI _ZN6icu_6810StringPair6createERKNS_13UnicodeStringES3_R10UErrorCode_nid_postfix(const Icu::Abi::UnicodeString* displayName, const Icu::Abi::UnicodeString* id, std::int32_t* status);
}
