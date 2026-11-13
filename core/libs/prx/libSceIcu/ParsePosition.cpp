// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2003-2003, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>

namespace {

using namespace Icu::Abi;

template<typename T> T& Require(T* pointer) {
    if (!pointer) throw std::invalid_argument("ParsePosition: null pointer");
    return *pointer;
}

template<std::size_t N> struct Table {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    std::array<GuestFunction, N> slots;
};

const TypeInfo* ParsePositionType() {
    static const DerivedTypeInfo info{{SingleTypeTable_nid_no_patch(), "N6icu_6813ParsePositionE"}, Rtti_nid_no_patch(Class::Object)};
    return &info.type;
}

const GuestFunction* ParsePositionVtable() {
    static const Table<3> table{0, ParsePositionType(), {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6813ParsePositionD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6813ParsePositionD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6813ParsePosition17getDynamicClassIDEv_nid_postfix),
    }}};
    return table.slots.data();
}

}

extern "C" {

void APS5_VABI _ZN6icu_6813ParsePositionD2Ev_nid_postfix(ParsePosition* self) {
    Require(self);
}

void APS5_VABI _ZN6icu_6813ParsePositionD1Ev_nid_postfix(ParsePosition* self) {
    _ZN6icu_6813ParsePositionD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6813ParsePositionD0Ev_nid_postfix(ParsePosition* self) {
    auto& pos = Require(self);
    _ZN6icu_6813ParsePositionD1Ev_nid_postfix(&pos);
    Free_nid_no_patch(&pos);
}

ParsePosition* APS5_VABI _ZNK6icu_6813ParsePosition5cloneEv_nid_postfix(const ParsePosition* self) {
    const auto& pos = Require(self);
    auto* copy = static_cast<ParsePosition*>(Allocate_nid_no_patch(sizeof(ParsePosition)));
    if (!copy) return nullptr;
    copy->vtable = ParsePositionVtable();
    copy->index = pos.index;
    copy->errorIndex = pos.errorIndex;
    return copy;
}

void* APS5_VABI _ZN6icu_6813ParsePosition16getStaticClassIDEv_nid_postfix() {
    static char fgClassID = 0;
    return &fgClassID;
}

void* APS5_VABI _ZNK6icu_6813ParsePosition17getDynamicClassIDEv_nid_postfix(const ParsePosition* self) {
    Require(self);
    return _ZN6icu_6813ParsePosition16getStaticClassIDEv_nid_postfix();
}

}
