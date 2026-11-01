// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2003-2011, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1999-2013, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1999-2015, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <stdexcept>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU vector object: null pointer");
    return *value;
}
struct VectorTable {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    std::array<GuestFunction, 3> slots;
};
static_assert(sizeof(VectorTable) == 40 && alignof(VectorTable) == 8);
static_assert(offsetof(VectorTable, slots) == 16);
const DerivedTypeInfo& VectorType() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_687UVectorE"}, Rtti_nid_no_patch(Class::Object)};
    return type;
}
const GuestFunction* VectorVtable() {
    static const VectorTable table{0, &VectorType().type, {{
        reinterpret_cast<GuestFunction>(_ZN6icu_687UVectorD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_687UVectorD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix),
    }}};
    return table.slots.data();
}
const GuestFunction* StackTable() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_686UStackE"}, &VectorType().type};
    static const VectorTable table{0, &type.type, {{
        reinterpret_cast<GuestFunction>(_ZN6icu_686UStackD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_686UStackD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_686UStack17getDynamicClassIDEv_nid_postfix),
    }}};
    return table.slots.data();
}
const GuestFunction* Vector32Table() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_689UVector32E"}, Rtti_nid_no_patch(Class::Object)};
    static const VectorTable table{0, &type.type, {{
        reinterpret_cast<GuestFunction>(_ZN6icu_689UVector32D1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_689UVector32D0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_689UVector3217getDynamicClassIDEv_nid_postfix),
    }}};
    return table.slots.data();
}
void Construct(Vector& self, std::int32_t capacity, ElementDeleter deleter, ElementsEqual comparer, std::int32_t* status) {
    Require(status);
    self = {VectorVtable(), 0, 0, nullptr, deleter, comparer};
    _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(&self, capacity, status);
}
void Construct(Vector32& self, std::int32_t capacity, std::int32_t* status) {
    Require(status);
    self = {Vector32Table(), 0, 0, 0, nullptr};
    _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(&self, capacity, status);
}
void Destroy(Vector& self) {
    self.vtable = VectorVtable();
    _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(&self);
    Free_nid_no_patch(self.elements);
    self.elements = nullptr;
}
void Destroy(Vector32& self) {
    self.vtable = Vector32Table();
    Free_nid_no_patch(self.elements);
    self.elements = nullptr;
}
}

extern "C" {
void APS5_VABI _ZN6icu_687UVectorC1ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t* status) {
    Construct(Require(self), 8, nullptr, nullptr, status);
}

void APS5_VABI _ZN6icu_687UVectorC1EiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, nullptr, nullptr, status);
}

void APS5_VABI _ZN6icu_687UVectorC1EPFvPvEPFa8UElementS4_ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t* status) {
    Construct(Require(self), 8, deleter, comparer, status);
}

void APS5_VABI _ZN6icu_687UVectorC1EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, deleter, comparer, status);
}

void APS5_VABI _ZN6icu_687UVectorC2ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t* status) {
    Construct(Require(self), 8, nullptr, nullptr, status);
}

void APS5_VABI _ZN6icu_687UVectorC2EiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, nullptr, nullptr, status);
}

void APS5_VABI _ZN6icu_687UVectorC2EPFvPvEPFa8UElementS4_ER10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t* status) {
    Construct(Require(self), 8, deleter, comparer, status);
}

void APS5_VABI _ZN6icu_687UVectorC2EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, deleter, comparer, status);
}

void APS5_VABI _ZN6icu_687UVectorD1Ev_nid_postfix(Icu::Abi::Vector* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_687UVectorD2Ev_nid_postfix(Icu::Abi::Vector* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_687UVectorD0Ev_nid_postfix(Icu::Abi::Vector* self) {
    Destroy(Require(self));
    Free_nid_no_patch(self);
}

void APS5_VABI _ZN6icu_686UStackC1ER10UErrorCode_nid_postfix(Icu::Abi::Stack* self, std::int32_t* status) {
    Construct(Require(self), 8, nullptr, nullptr, status);
    self->vtable = StackTable();
}

void APS5_VABI _ZN6icu_686UStackC1EiR10UErrorCode_nid_postfix(Icu::Abi::Stack* self, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, nullptr, nullptr, status);
    self->vtable = StackTable();
}

void APS5_VABI _ZN6icu_686UStackC1EPFvPvEPFa8UElementS4_ER10UErrorCode_nid_postfix(Icu::Abi::Stack* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t* status) {
    Construct(Require(self), 8, deleter, comparer, status);
    self->vtable = StackTable();
}

void APS5_VABI _ZN6icu_686UStackC1EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(Icu::Abi::Stack* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, deleter, comparer, status);
    self->vtable = StackTable();
}

void APS5_VABI _ZN6icu_686UStackC2ER10UErrorCode_nid_postfix(Icu::Abi::Stack* self, std::int32_t* status) {
    Construct(Require(self), 8, nullptr, nullptr, status);
    self->vtable = StackTable();
}

void APS5_VABI _ZN6icu_686UStackC2EiR10UErrorCode_nid_postfix(Icu::Abi::Stack* self, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, nullptr, nullptr, status);
    self->vtable = StackTable();
}

void APS5_VABI _ZN6icu_686UStackC2EPFvPvEPFa8UElementS4_ER10UErrorCode_nid_postfix(Icu::Abi::Stack* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t* status) {
    Construct(Require(self), 8, deleter, comparer, status);
    self->vtable = StackTable();
}

void APS5_VABI _ZN6icu_686UStackC2EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(Icu::Abi::Stack* self, Icu::Abi::ElementDeleter deleter, Icu::Abi::ElementsEqual comparer, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, deleter, comparer, status);
    self->vtable = StackTable();
}

void APS5_VABI _ZN6icu_686UStackD1Ev_nid_postfix(Icu::Abi::Stack* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_686UStackD2Ev_nid_postfix(Icu::Abi::Stack* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_686UStackD0Ev_nid_postfix(Icu::Abi::Stack* self) {
    Destroy(Require(self));
    Free_nid_no_patch(self);
}

void APS5_VABI _ZN6icu_689UVector32C1ER10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t* status) {
    Construct(Require(self), 8, status);
}

void APS5_VABI _ZN6icu_689UVector32C1EiR10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, status);
}

void APS5_VABI _ZN6icu_689UVector32C2ER10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t* status) {
    Construct(Require(self), 8, status);
}

void APS5_VABI _ZN6icu_689UVector32C2EiR10UErrorCode_nid_postfix(Icu::Abi::Vector32* self, std::int32_t capacity, std::int32_t* status) {
    Construct(Require(self), capacity, status);
}

void APS5_VABI _ZN6icu_689UVector32D1Ev_nid_postfix(Icu::Abi::Vector32* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_689UVector32D2Ev_nid_postfix(Icu::Abi::Vector32* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_689UVector32D0Ev_nid_postfix(Icu::Abi::Vector32* self) {
    Destroy(Require(self));
    Free_nid_no_patch(self);
}

}
