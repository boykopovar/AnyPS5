// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2003-2011, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1999-2013, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1999-2015, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"
#include "Vector.hpp"
#include "Vector32.hpp"

extern "C" {
void APS5_VABI _ZN6icu_687UVectorC1ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVectorC1EiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVectorC1EPFvPvEPFa8UElementS4_ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVectorC1EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVectorC2ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVectorC2EiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVectorC2EPFvPvEPFa8UElementS4_ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVectorC2EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVectorD1Ev_nid_postfix(Icu::Abi::Vector* self);
void APS5_VABI _ZN6icu_687UVectorD2Ev_nid_postfix(Icu::Abi::Vector* self);
void APS5_VABI _ZN6icu_687UVectorD0Ev_nid_postfix(Icu::Abi::Vector* self);
void APS5_VABI _ZN6icu_686UStackC1ER10UErrorCode_nid_postfix(Icu::Abi::Stack* self, std::int32_t* status);
void APS5_VABI _ZN6icu_686UStackC1EiR10UErrorCode_nid_postfix(Icu::Abi::Stack* self, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_686UStackC1EPFvPvEPFa8UElementS4_ER10UErrorCode_nid_postfix(Icu::Abi::Stack* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t* status);
void APS5_VABI _ZN6icu_686UStackC1EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(Icu::Abi::Stack* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_686UStackC2ER10UErrorCode_nid_postfix(Icu::Abi::Stack* self, std::int32_t* status);
void APS5_VABI _ZN6icu_686UStackC2EiR10UErrorCode_nid_postfix(Icu::Abi::Stack* self, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_686UStackC2EPFvPvEPFa8UElementS4_ER10UErrorCode_nid_postfix(Icu::Abi::Stack* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t* status);
void APS5_VABI _ZN6icu_686UStackC2EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(Icu::Abi::Stack* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_686UStackD1Ev_nid_postfix(Icu::Abi::Stack* self);
void APS5_VABI _ZN6icu_686UStackD2Ev_nid_postfix(Icu::Abi::Stack* self);
void APS5_VABI _ZN6icu_686UStackD0Ev_nid_postfix(Icu::Abi::Stack* self);
void APS5_VABI _ZN6icu_689UVector32C1ER10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t* status);
void APS5_VABI _ZN6icu_689UVector32C1EiR10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_689UVector32C2ER10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t* status);
void APS5_VABI _ZN6icu_689UVector32C2EiR10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t capacity, std::int32_t* status);
void APS5_VABI _ZN6icu_689UVector32D1Ev_nid_postfix(Icu::Abi::Vector32* self);
void APS5_VABI _ZN6icu_689UVector32D2Ev_nid_postfix(Icu::Abi::Vector32* self);
void APS5_VABI _ZN6icu_689UVector32D0Ev_nid_postfix(Icu::Abi::Vector32* self);
}
