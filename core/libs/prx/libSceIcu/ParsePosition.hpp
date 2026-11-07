// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2003-2003, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"

namespace Icu::Abi {

struct ParsePosition {
    const GuestFunction* vtable;
    std::int32_t index;
    std::int32_t errorIndex;
};

static_assert(sizeof(ParsePosition) == 16 && alignof(ParsePosition) == 8);
static_assert(offsetof(ParsePosition, vtable) == 0);
static_assert(offsetof(ParsePosition, index) == 8);
static_assert(offsetof(ParsePosition, errorIndex) == 12);

}

extern "C" {
void APS5_VABI _ZN6icu_6813ParsePositionD1Ev_nid_postfix(Icu::Abi::ParsePosition* self);
void APS5_VABI _ZN6icu_6813ParsePositionD2Ev_nid_postfix(Icu::Abi::ParsePosition* self);
void APS5_VABI _ZN6icu_6813ParsePositionD0Ev_nid_postfix(Icu::Abi::ParsePosition* self);
Icu::Abi::ParsePosition* APS5_VABI _ZNK6icu_6813ParsePosition5cloneEv_nid_postfix(const Icu::Abi::ParsePosition* self);
void* APS5_VABI _ZN6icu_6813ParsePosition16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_6813ParsePosition17getDynamicClassIDEv_nid_postfix(const Icu::Abi::ParsePosition* self);
}
