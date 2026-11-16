// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "Vector.hpp"

namespace Icu::Abi {

struct Vector32 {
    const void* vtable;
    std::int32_t count;
    std::int32_t capacity;
    std::int32_t maxCapacity;
    std::int32_t* elements;
};

static_assert(sizeof(Vector32) == 32 && alignof(Vector32) == 8);
static_assert(offsetof(Vector32, count) == 8 && offsetof(Vector32, capacity) == 12);
static_assert(offsetof(Vector32, maxCapacity) == 16 && offsetof(Vector32, elements) == 24);

}

extern "C" {
void APS5_VABI _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t capacity, std::int32_t* status);
std::int8_t APS5_VABI _ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t minimum, std::int32_t* status);
void APS5_VABI _ZN6icu_689UVector3214setMaxCapacityEi_nid_postfix(Icu::Abi::Vector32* self, std::int32_t limit);
void APS5_VABI _ZN6icu_689UVector3212setElementAtEii_nid_postfix(Icu::Abi::Vector32* self, std::int32_t value, std::int32_t index);
void APS5_VABI _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t value, std::int32_t index, std::int32_t* status);
void APS5_VABI _ZN6icu_689UVector3215removeElementAtEi_nid_postfix(Icu::Abi::Vector32* self, std::int32_t index);
void APS5_VABI _ZN6icu_689UVector3217removeAllElementsEv_nid_postfix(Icu::Abi::Vector32* self);
void APS5_VABI _ZN6icu_689UVector327setSizeEi_nid_postfix(Icu::Abi::Vector32* self, std::int32_t size);
std::int32_t APS5_VABI _ZNK6icu_689UVector327indexOfEii_nid_postfix(const Icu::Abi::Vector32* self, std::int32_t value, std::int32_t start);
std::int8_t APS5_VABI _ZNK6icu_689UVector326equalsERKS0__nid_postfix(const Icu::Abi::Vector32* self, const Icu::Abi::Vector32* other);
std::int8_t APS5_VABI _ZN6icu_689UVector32eqERKS0__nid_postfix(Icu::Abi::Vector32* self, const Icu::Abi::Vector32* other);
std::int8_t APS5_VABI _ZNK6icu_689UVector3211containsAllERKS0__nid_postfix(const Icu::Abi::Vector32* self, const Icu::Abi::Vector32* other);
std::int8_t APS5_VABI _ZNK6icu_689UVector3212containsNoneERKS0__nid_postfix(const Icu::Abi::Vector32* self, const Icu::Abi::Vector32* other);
std::int8_t APS5_VABI _ZN6icu_689UVector329removeAllERKS0__nid_postfix(Icu::Abi::Vector32* self, const Icu::Abi::Vector32* other);
std::int8_t APS5_VABI _ZN6icu_689UVector329retainAllERKS0__nid_postfix(Icu::Abi::Vector32* self, const Icu::Abi::Vector32* other);
void APS5_VABI _ZN6icu_689UVector326assignERKS0_R10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, const Icu::Abi::Vector32* other, std::int32_t* status);
void APS5_VABI _ZN6icu_689UVector3212sortedInsertEiR10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t value, std::int32_t* status);
void* APS5_VABI _ZN6icu_689UVector3216getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_689UVector3217getDynamicClassIDEv_nid_postfix(const Icu::Abi::Vector32*);
}
