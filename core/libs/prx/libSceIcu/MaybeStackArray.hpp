// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "CharString.hpp"

namespace Icu::Abi {
using MaybeStackArray40 = CharString::Buffer;
static_assert(sizeof(MaybeStackArray40) == 56);
static_assert(alignof(MaybeStackArray40) == 8);
static_assert(offsetof(MaybeStackArray40, stack) == 13);
}

extern "C" {
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(Icu::Abi::MaybeStackArray40*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ev_nid_postfix(Icu::Abi::MaybeStackArray40*);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE17resetToStackArrayEv_nid_postfix(Icu::Abi::MaybeStackArray40*);
std::int32_t APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE11getCapacityEv_nid_postfix(const Icu::Abi::MaybeStackArray40*);
char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE8getAliasEv_nid_postfix(const Icu::Abi::MaybeStackArray40*);
char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE13getArrayLimitEv_nid_postfix(const Icu::Abi::MaybeStackArray40*);
char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(Icu::Abi::MaybeStackArray40*, std::int64_t);
const char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(const Icu::Abi::MaybeStackArray40*, std::int64_t);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(Icu::Abi::MaybeStackArray40* array);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EED2Ev_nid_postfix(Icu::Abi::MaybeStackArray40* array);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE12releaseArrayEv_nid_postfix(Icu::Abi::MaybeStackArray40* array);
char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(Icu::Abi::MaybeStackArray40* array, std::int32_t capacity, std::int32_t length);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE12aliasInsteadEPci_nid_postfix(Icu::Abi::MaybeStackArray40* array, char* source, std::int32_t capacity);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ei10UErrorCode_nid_postfix(Icu::Abi::MaybeStackArray40* array, std::int32_t capacity, std::int32_t status);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1EOS1__nid_postfix(Icu::Abi::MaybeStackArray40* array, Icu::Abi::MaybeStackArray40* source);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ei10UErrorCode_nid_postfix(Icu::Abi::MaybeStackArray40* array, std::int32_t capacity, std::int32_t status);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2EOS1__nid_postfix(Icu::Abi::MaybeStackArray40* array, Icu::Abi::MaybeStackArray40* source);
Icu::Abi::MaybeStackArray40* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEaSEOS1__nid_postfix(Icu::Abi::MaybeStackArray40* array, Icu::Abi::MaybeStackArray40* source);
void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE8copyFromERKS1_R10UErrorCode_nid_postfix(Icu::Abi::MaybeStackArray40* array, const Icu::Abi::MaybeStackArray40* source, std::int32_t* status);
}
