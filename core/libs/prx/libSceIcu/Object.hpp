// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include "UnicodeString.hpp"
#include <stdexcept>

namespace Icu::Abi {
using GuestFunction = void (APS5_VABI*)();
enum class Class { Object, Replaceable, UnicodeString, Appendable, UnicodeStringAppendable, ByteSink, CheckedArrayByteSink, StringCharacterIterator, StringEnumeration, UStringEnumeration };
struct Object { const GuestFunction* vtable; };
struct TypeInfo { const GuestFunction* vtable; const char* name; };
struct DerivedTypeInfo { TypeInfo type; const TypeInfo* base; };
static_assert(sizeof(Object) == 8 && alignof(Object) == 8);
static_assert(offsetof(Object, vtable) == 0);
static_assert(sizeof(TypeInfo) == 16 && alignof(TypeInfo) == 8);
static_assert(offsetof(TypeInfo, name) == 8);
static_assert(sizeof(DerivedTypeInfo) == 24 && alignof(DerivedTypeInfo) == 8);
static_assert(offsetof(DerivedTypeInfo, base) == 16);
void* Allocate_nid_no_patch(std::size_t size);
void Free_nid_no_patch(void* pointer);
const GuestFunction* Vtable_nid_no_patch(Class type);
const TypeInfo* Rtti_nid_no_patch(Class type);
const GuestFunction* ClassTypeTable_nid_no_patch();
const GuestFunction* SingleTypeTable_nid_no_patch();
const GuestFunction* MultipleTypeTable_nid_no_patch();
void* ClassId_nid_no_patch(Class type);
void Initialize_nid_no_patch(void* object, Class type);
template<typename TFunction> TFunction Virtual_nid_no_patch(const void* object, std::size_t slot) {
    if (!object) throw std::invalid_argument("ICU: null virtual object");
    const auto* table = static_cast<const Object*>(object)->vtable;
    if (!table || !table[slot]) throw std::invalid_argument("ICU: null virtual callback");
    return reinterpret_cast<TFunction>(table[slot]);
}
}

extern "C" {
void* APS5_VABI _ZN6icu_687UMemorynwEm_nid_postfix(std::size_t size);
void* APS5_VABI _ZN6icu_687UMemorynaEm_nid_postfix(std::size_t size);
void APS5_VABI _ZN6icu_687UMemorydlEPv_nid_postfix(void* pointer);
void APS5_VABI _ZN6icu_687UMemorydaEPv_nid_postfix(void* pointer);
void APS5_VABI _ZN6icu_687UObjectD1Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_687UObjectD2Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_687UObjectD0Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6811ReplaceableD1Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6811ReplaceableD2Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6811ReplaceableD0Ev_nid_postfix(void* object);
void* APS5_VABI _ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix(const void*);
std::int8_t APS5_VABI _ZNK6icu_6811Replaceable11hasMetaDataEv_nid_postfix(const void*);
void* APS5_VABI _ZNK6icu_6811Replaceable5cloneEv_nid_postfix(const void*);
}
