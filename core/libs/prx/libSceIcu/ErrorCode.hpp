// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2009-2011, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1997-2015, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"

namespace Icu::Abi {
struct ErrorCode {
    const GuestFunction* vtable;
    std::int32_t errorCode;
};
static_assert(sizeof(ErrorCode) == 16 && alignof(ErrorCode) == 8);
static_assert(offsetof(ErrorCode, vtable) == 0 && offsetof(ErrorCode, errorCode) == 8);
}

extern "C" {
void APS5_VABI _ZN6icu_689ErrorCodeD1Ev_nid_postfix(Icu::Abi::ErrorCode* self);
void APS5_VABI _ZN6icu_689ErrorCodeD2Ev_nid_postfix(Icu::Abi::ErrorCode* self);
void APS5_VABI _ZN6icu_689ErrorCodeD0Ev_nid_postfix(Icu::Abi::ErrorCode* self);
std::int32_t APS5_VABI _ZN6icu_689ErrorCode5resetEv_nid_postfix(Icu::Abi::ErrorCode* self);
void APS5_VABI _ZNK6icu_689ErrorCode13handleFailureEv_nid_postfix(const Icu::Abi::ErrorCode*);
void APS5_VABI _ZNK6icu_689ErrorCode13assertSuccessEv_nid_postfix(const Icu::Abi::ErrorCode* self);
const char* APS5_VABI _ZNK6icu_689ErrorCode9errorNameEv_nid_postfix(const Icu::Abi::ErrorCode* self);
}
