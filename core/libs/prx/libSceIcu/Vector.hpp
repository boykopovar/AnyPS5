// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace Icu::Abi {

struct Storage {
    static void* Resize(void* storage, std::size_t bytes) { return std::realloc(storage, bytes); }
    static void Release(void* storage) { std::free(storage); }
};

union Element {
    void* pointer;
    std::int32_t integer;
};

using ElementDeleter = void (APS5_VABI *)(void*);
using ElementsEqual = std::int8_t (APS5_VABI *)(Element, Element);
using ElementComparator = std::int8_t (APS5_VABI *)(Element, Element);
using ElementAssigner = void (APS5_VABI *)(Element*, Element*);
using ArrayComparator = std::int32_t (APS5_VABI *)(const void*, const void*, const void*);

struct Vector {
    const void* vtable;
    std::int32_t count;
    std::int32_t capacity;
    Element* elements;
    ElementDeleter deleter;
    ElementsEqual comparer;
};

struct Stack : Vector {};

static_assert(sizeof(Element) == 8 && alignof(Element) == 8);
static_assert(offsetof(Element, pointer) == 0 && offsetof(Element, integer) == 0);
static_assert(sizeof(Vector) == 40 && alignof(Vector) == 8);
static_assert(offsetof(Vector, count) == 8 && offsetof(Vector, capacity) == 12);
static_assert(offsetof(Vector, elements) == 16 && offsetof(Vector, deleter) == 24);
static_assert(offsetof(Vector, comparer) == 32);
static_assert(sizeof(Stack) == 40 && alignof(Stack) == 8);

}

extern "C" {
void APS5_VABI _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t capacity, std::int32_t* status);
std::int8_t APS5_VABI _ZN6icu_687UVector14ensureCapacityEiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t minimum, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, void* value, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t value, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector12setElementAtEPvi_nid_postfix(Icu::Abi::Vector* self, void* value, std::int32_t index);
void APS5_VABI _ZN6icu_687UVector12setElementAtEii_nid_postfix(Icu::Abi::Vector* self, std::int32_t value, std::int32_t index);
void APS5_VABI _ZN6icu_687UVector15insertElementAtEPviR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, void* value, std::int32_t index, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector15insertElementAtEiiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t value, std::int32_t index, std::int32_t* status);
void* APS5_VABI _ZNK6icu_687UVector9elementAtEi_nid_postfix(const Icu::Abi::Vector* self, std::int32_t index);
std::int32_t APS5_VABI _ZNK6icu_687UVector10elementAtiEi_nid_postfix(const Icu::Abi::Vector* self, std::int32_t index);
std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfE8UElementia_nid_postfix(const Icu::Abi::Vector* self, Icu::Abi::Element key, std::int32_t start, std::int8_t hint);
std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfEPvi_nid_postfix(const Icu::Abi::Vector* self, void* key, std::int32_t start);
std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfEii_nid_postfix(const Icu::Abi::Vector* self, std::int32_t key, std::int32_t start);
std::int8_t APS5_VABI _ZN6icu_687UVectoreqERKS0__nid_postfix(Icu::Abi::Vector* self, const Icu::Abi::Vector* other);
std::int8_t APS5_VABI _ZNK6icu_687UVector6equalsERKS0__nid_postfix(const Icu::Abi::Vector* self, const Icu::Abi::Vector* other);
std::int8_t APS5_VABI _ZNK6icu_687UVector11containsAllERKS0__nid_postfix(const Icu::Abi::Vector* self, const Icu::Abi::Vector* other);
std::int8_t APS5_VABI _ZNK6icu_687UVector12containsNoneERKS0__nid_postfix(const Icu::Abi::Vector* self, const Icu::Abi::Vector* other);
void* APS5_VABI _ZN6icu_687UVector15orphanElementAtEi_nid_postfix(Icu::Abi::Vector* self, std::int32_t index);
void APS5_VABI _ZN6icu_687UVector15removeElementAtEi_nid_postfix(Icu::Abi::Vector* self, std::int32_t index);
std::int8_t APS5_VABI _ZN6icu_687UVector13removeElementEPv_nid_postfix(Icu::Abi::Vector* self, void* value);
void APS5_VABI _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(Icu::Abi::Vector* self);
std::int8_t APS5_VABI _ZN6icu_687UVector9removeAllERKS0__nid_postfix(Icu::Abi::Vector* self, const Icu::Abi::Vector* other);
std::int8_t APS5_VABI _ZN6icu_687UVector9retainAllERKS0__nid_postfix(Icu::Abi::Vector* self, const Icu::Abi::Vector* other);
void APS5_VABI _ZN6icu_687UVector7setSizeEiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t size, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector6assignERKS0_PFvP8UElementS4_ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, const Icu::Abi::Vector* other, Icu::Abi::ElementAssigner assign, std::int32_t* status);
void** APS5_VABI _ZNK6icu_687UVector7toArrayEPPv_nid_postfix(const Icu::Abi::Vector* self, void** result);
Icu::Abi::ElementDeleter APS5_VABI _ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementDeleter deleter);
Icu::Abi::ElementsEqual APS5_VABI _ZN6icu_687UVector11setComparerEPFa8UElementS1_E_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementsEqual comparer);
void APS5_VABI _ZN6icu_687UVector12sortedInsertE8UElementPFaS1_S1_ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::Element value, Icu::Abi::ElementComparator compare, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector12sortedInsertEPvPFa8UElementS2_ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, void* value, Icu::Abi::ElementComparator compare, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector12sortedInsertEiPFa8UElementS1_ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t value, Icu::Abi::ElementComparator compare, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector5sortiER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector4sortEPFa8UElementS1_ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementComparator compare, std::int32_t* status);
void APS5_VABI _ZN6icu_687UVector19sortWithUComparatorEPFiPKvS2_S2_ES2_R10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ArrayComparator compare, const void* context, std::int32_t* status);
void* APS5_VABI _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix(const Icu::Abi::Vector*);
void* APS5_VABI _ZN6icu_686UStack16getStaticClassIDEv_nid_postfix();
void* APS5_VABI _ZNK6icu_686UStack17getDynamicClassIDEv_nid_postfix(const Icu::Abi::Stack*);
void* APS5_VABI _ZN6icu_686UStack3popEv_nid_postfix(Icu::Abi::Stack* self);
std::int32_t APS5_VABI _ZN6icu_686UStack4popiEv_nid_postfix(Icu::Abi::Stack* self);
std::int32_t APS5_VABI _ZNK6icu_686UStack6searchEPv_nid_postfix(const Icu::Abi::Stack* self, void* value);
}
