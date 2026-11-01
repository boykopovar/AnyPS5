// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2001-2014, International Business Machines Corporation. All Rights Reserved.
// Copyright (C) 2001-2011, International Business Machines Corporation. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) { if (!value) throw std::invalid_argument("ICU string pair: null pointer"); return *value; }
void Construct(StringPair* self, const UnicodeString* displayName, const UnicodeString* id) {
    Require(self); Require(displayName); Require(id);
    _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(&self->displayName, displayName);
    _ZN6icu_6813UnicodeStringC1ERKS0__nid_postfix(&self->id, id);
}
}

extern "C" {
void APS5_VABI _ZN6icu_6810StringPairC1ERKNS_13UnicodeStringES3__nid_postfix(StringPair* self, const UnicodeString* displayName, const UnicodeString* id) { Construct(self, displayName, id); }
void APS5_VABI _ZN6icu_6810StringPairC2ERKNS_13UnicodeStringES3__nid_postfix(StringPair* self, const UnicodeString* displayName, const UnicodeString* id) { Construct(self, displayName, id); }
std::int8_t APS5_VABI _ZNK6icu_6810StringPair7isBogusEv_nid_postfix(const StringPair* self) { const auto& pair = Require(self); return (pair.displayName.Flags() & UnicodeString::IsBogus) || (pair.id.Flags() & UnicodeString::IsBogus); }
StringPair* APS5_VABI _ZN6icu_6810StringPair6createERKNS_13UnicodeStringES3_R10UErrorCode_nid_postfix(const UnicodeString* displayName, const UnicodeString* id, std::int32_t* status) {
    if (Require(status) > 0) return nullptr;
    Require(displayName); Require(id);
    auto* pair = static_cast<StringPair*>(Allocate_nid_no_patch(sizeof(StringPair)));
    if (pair) Construct(pair, displayName, id);
    if (!pair || _ZNK6icu_6810StringPair7isBogusEv_nid_postfix(pair)) {
        *status = 7;
        if (pair) { _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&pair->id); _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&pair->displayName); Free_nid_no_patch(pair); }
        return nullptr;
    }
    return pair;
}
}
