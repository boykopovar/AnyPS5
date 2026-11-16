// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1999-2011, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1998-2012, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU iterator object: null pointer");
    return *value;
}
void APS5_VABI Abstract() { throw std::invalid_argument("ICU iterator object: abstract method"); }
struct IteratorTable {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    std::array<GuestFunction, 27> slots;
};
static_assert(sizeof(IteratorTable) == 232 && alignof(IteratorTable) == 8);
static_assert(offsetof(IteratorTable, slots) == 16);
const DerivedTypeInfo& ForwardType() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_6824ForwardCharacterIteratorE"}, Rtti_nid_no_patch(Class::Object)};
    return type;
}
const DerivedTypeInfo& CharacterType() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_6817CharacterIteratorE"}, &ForwardType().type};
    return type;
}
const DerivedTypeInfo& UCharType() {
    static const DerivedTypeInfo type{{SingleTypeTable_nid_no_patch(), "N6icu_6822UCharCharacterIteratorE"}, &CharacterType().type};
    return type;
}
const CharacterIteratorFunctions* ForwardTable() {
    static const IteratorTable table = [] {
        IteratorTable result{0, &ForwardType().type, {}};
        result.slots.fill(Abstract);
        result.slots[0] = reinterpret_cast<GuestFunction>(_ZN6icu_6824ForwardCharacterIteratorD1Ev_nid_postfix);
        result.slots[1] = reinterpret_cast<GuestFunction>(_ZN6icu_6824ForwardCharacterIteratorD0Ev_nid_postfix);
        result.slots[2] = reinterpret_cast<GuestFunction>(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix);
        return result;
    }();
    return reinterpret_cast<const CharacterIteratorFunctions*>(table.slots.data());
}
const CharacterIteratorFunctions* CharacterTable() {
    static const IteratorTable table = [] {
        IteratorTable result{0, &CharacterType().type, {}};
        result.slots.fill(Abstract);
        result.slots[0] = reinterpret_cast<GuestFunction>(_ZN6icu_6817CharacterIteratorD1Ev_nid_postfix);
        result.slots[1] = reinterpret_cast<GuestFunction>(_ZN6icu_6817CharacterIteratorD0Ev_nid_postfix);
        result.slots[2] = reinterpret_cast<GuestFunction>(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix);
        result.slots[10] = reinterpret_cast<GuestFunction>(_ZN6icu_6817CharacterIterator12firstPostIncEv_nid_postfix);
        result.slots[12] = reinterpret_cast<GuestFunction>(_ZN6icu_6817CharacterIterator14first32PostIncEv_nid_postfix);
        return result;
    }();
    return reinterpret_cast<const CharacterIteratorFunctions*>(table.slots.data());
}
const CharacterIteratorFunctions* UCharTable() {
    static const IteratorTable table{0, &UCharType().type, {{
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIteratorD1Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIteratorD0Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6822UCharCharacterIterator17getDynamicClassIDEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6822UCharCharacterIteratoreqERKNS_24ForwardCharacterIteratorE_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6822UCharCharacterIterator8hashCodeEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator11nextPostIncEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator13next32PostIncEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator7hasNextEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6822UCharCharacterIterator5cloneEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator5firstEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator12firstPostIncEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator7first32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator14first32PostIncEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator4lastEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator6last32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator8setIndexEi_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator10setIndex32Ei_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6822UCharCharacterIterator7currentEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZNK6icu_6822UCharCharacterIterator9current32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator4nextEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator6next32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator8previousEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator10previous32Ev_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator11hasPreviousEv_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator4moveEiNS_17CharacterIterator7EOriginE_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator6move32EiNS_17CharacterIterator7EOriginE_nid_postfix),
        reinterpret_cast<GuestFunction>(_ZN6icu_6822UCharCharacterIterator7getTextERNS_13UnicodeStringE_nid_postfix),
    }}};
    return reinterpret_cast<const CharacterIteratorFunctions*>(table.slots.data());
}
void Initialize(CharacterIterator& self, std::int32_t length, std::int32_t begin, std::int32_t end, std::int32_t position) {
    self.vtable = CharacterTable();
    self.textLength = std::max(length, 0);
    self.begin = std::clamp(begin, 0, self.textLength);
    self.end = std::clamp(end, self.begin, self.textLength);
    self.position = std::clamp(position, self.begin, self.end);
}
std::int32_t TextLength(const ConstChar16Ptr* pointer, std::int32_t length) {
    const auto* text = Require(pointer).pointer;
    return !text ? 0 : length < 0 ? Utf16Length_nid_no_patch(text) : length;
}
void Construct(UCharCharacterIterator& self, const char16_t* text, std::int32_t length, std::int32_t begin, std::int32_t end, std::int32_t position) {
    Initialize(self.base, length, begin, end, position);
    self.base.vtable = UCharTable();
    self.text = text;
}
bool SameType(const CharacterIteratorFunctions* left, const CharacterIteratorFunctions* right) {
    const auto* a = reinterpret_cast<const TypeInfo* const*>(&Require(left));
    const auto* b = reinterpret_cast<const TypeInfo* const*>(&Require(right));
    const auto& first = Require(a[-1]);
    const auto& second = Require(b[-1]);
    Require(first.name);
    Require(second.name);
    return &first == &second || std::strcmp(first.name, second.name) == 0;
}
}

extern "C" {
void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorC2Ev_nid_postfix(Icu::Abi::ForwardCharacterIterator* self) {
    Require(self).vtable = ForwardTable();
}

void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorC2ERKS0__nid_postfix(Icu::Abi::ForwardCharacterIterator* self, const Icu::Abi::ForwardCharacterIterator* source) {
    Require(source);
    Require(self).vtable = ForwardTable();
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2Ev_nid_postfix(Icu::Abi::CharacterIterator* self) {
    Initialize(Require(self), 0, 0, 0, 0);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2Ei_nid_postfix(Icu::Abi::CharacterIterator* self, std::int32_t length) {
    Initialize(Require(self), length, 0, length, 0);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2Eii_nid_postfix(Icu::Abi::CharacterIterator* self, std::int32_t length, std::int32_t position) {
    Initialize(Require(self), length, 0, length, position);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2Eiiii_nid_postfix(Icu::Abi::CharacterIterator* self, std::int32_t length, std::int32_t begin, std::int32_t end, std::int32_t position) {
    Initialize(Require(self), length, begin, end, position);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorC2ERKS0__nid_postfix(Icu::Abi::CharacterIterator* self, const Icu::Abi::CharacterIterator* source) {
    Require(self) = Require(source);
    self->vtable = CharacterTable();
}

void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorD1Ev_nid_postfix(Icu::Abi::ForwardCharacterIterator* self) {
    Require(self);
    *reinterpret_cast<const Icu::Abi::CharacterIteratorFunctions**>(self) = ForwardTable();
}

void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorD2Ev_nid_postfix(Icu::Abi::ForwardCharacterIterator* self) {
    Require(self);
    *reinterpret_cast<const Icu::Abi::CharacterIteratorFunctions**>(self) = ForwardTable();
}

void APS5_VABI _ZN6icu_6824ForwardCharacterIteratorD0Ev_nid_postfix(Icu::Abi::ForwardCharacterIterator* self) {
    Require(self);
    *reinterpret_cast<const Icu::Abi::CharacterIteratorFunctions**>(self) = ForwardTable();
    Icu::Abi::Free_nid_no_patch(self);
}

void APS5_VABI _ZN6icu_6817CharacterIteratorD1Ev_nid_postfix(Icu::Abi::CharacterIterator* self) {
    Require(self);
    *reinterpret_cast<const Icu::Abi::CharacterIteratorFunctions**>(self) = ForwardTable();
}

void APS5_VABI _ZN6icu_6817CharacterIteratorD2Ev_nid_postfix(Icu::Abi::CharacterIterator* self) {
    Require(self);
    *reinterpret_cast<const Icu::Abi::CharacterIteratorFunctions**>(self) = ForwardTable();
}

void APS5_VABI _ZN6icu_6817CharacterIteratorD0Ev_nid_postfix(Icu::Abi::CharacterIterator* self) {
    Require(self);
    *reinterpret_cast<const Icu::Abi::CharacterIteratorFunctions**>(self) = ForwardTable();
    Icu::Abi::Free_nid_no_patch(self);
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorD1Ev_nid_postfix(Icu::Abi::UCharCharacterIterator* self) {
    Require(self);
    *reinterpret_cast<const Icu::Abi::CharacterIteratorFunctions**>(self) = ForwardTable();
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorD2Ev_nid_postfix(Icu::Abi::UCharCharacterIterator* self) {
    Require(self);
    *reinterpret_cast<const Icu::Abi::CharacterIteratorFunctions**>(self) = ForwardTable();
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorD0Ev_nid_postfix(Icu::Abi::UCharCharacterIterator* self) {
    Require(self);
    *reinterpret_cast<const Icu::Abi::CharacterIteratorFunctions**>(self) = ForwardTable();
    Icu::Abi::Free_nid_no_patch(self);
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC1Ev_nid_postfix(Icu::Abi::UCharCharacterIterator* self) {
    Construct(Require(self), nullptr, 0, 0, 0, 0);
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC1ERKS0__nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::UCharCharacterIterator* source) {
    Require(self) = Require(source);
    self->base.vtable = UCharTable();
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC1ENS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::ConstChar16Ptr* pointer, std::int32_t length) {
    Construct(Require(self), Require(pointer).pointer, TextLength(pointer, length), 0, TextLength(pointer, length), 0);
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC1ENS_14ConstChar16PtrEii_nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::ConstChar16Ptr* pointer, std::int32_t length, std::int32_t position) {
    Construct(Require(self), Require(pointer).pointer, TextLength(pointer, length), 0, TextLength(pointer, length), position);
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC1ENS_14ConstChar16PtrEiiii_nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::ConstChar16Ptr* pointer, std::int32_t length, std::int32_t begin, std::int32_t end, std::int32_t position) {
    Construct(Require(self), Require(pointer).pointer, TextLength(pointer, length), begin, end, position);
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC2Ev_nid_postfix(Icu::Abi::UCharCharacterIterator* self) {
    Construct(Require(self), nullptr, 0, 0, 0, 0);
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC2ERKS0__nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::UCharCharacterIterator* source) {
    Require(self) = Require(source);
    self->base.vtable = UCharTable();
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC2ENS_14ConstChar16PtrEi_nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::ConstChar16Ptr* pointer, std::int32_t length) {
    Construct(Require(self), Require(pointer).pointer, TextLength(pointer, length), 0, TextLength(pointer, length), 0);
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC2ENS_14ConstChar16PtrEii_nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::ConstChar16Ptr* pointer, std::int32_t length, std::int32_t position) {
    Construct(Require(self), Require(pointer).pointer, TextLength(pointer, length), 0, TextLength(pointer, length), position);
}

void APS5_VABI _ZN6icu_6822UCharCharacterIteratorC2ENS_14ConstChar16PtrEiiii_nid_postfix(Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::ConstChar16Ptr* pointer, std::int32_t length, std::int32_t begin, std::int32_t end, std::int32_t position) {
    Construct(Require(self), Require(pointer).pointer, TextLength(pointer, length), begin, end, position);
}

Icu::Abi::UCharCharacterIterator* APS5_VABI _ZNK6icu_6822UCharCharacterIterator5cloneEv_nid_postfix(const Icu::Abi::UCharCharacterIterator* self) {
    const auto& source = Require(self);
    auto* result = static_cast<Icu::Abi::UCharCharacterIterator*>(Icu::Abi::Allocate_nid_no_patch(sizeof(source)));
    if (result) {
        *result = source;
        result->base.vtable = UCharTable();
    }
    return result;
}

std::int8_t APS5_VABI _ZNK6icu_6822UCharCharacterIteratoreqERKNS_24ForwardCharacterIteratorE_nid_postfix(const Icu::Abi::UCharCharacterIterator* self, const Icu::Abi::ForwardCharacterIterator* source) {
    const auto& left = Require(self);
    Require(source);
    if (reinterpret_cast<const void*>(self) == source) return 1;
    if (!SameType(left.base.vtable, source->vtable)) return 0;
    const auto& right = *reinterpret_cast<const Icu::Abi::UCharCharacterIterator*>(source);
    return left.text == right.text && left.base.textLength == right.base.textLength && left.base.position == right.base.position && left.base.begin == right.base.begin && left.base.end == right.base.end;
}

void APS5_VABI _ZN6icu_6822UCharCharacterIterator7getTextERNS_13UnicodeStringE_nid_postfix(Icu::Abi::UCharCharacterIterator* self, Icu::Abi::UnicodeString* result) {
    const auto& source = Require(self);
    Require(result);
    Icu::Abi::UnicodeString temporary{};
    _ZN6icu_6813UnicodeStringC1EPKDsi_nid_postfix(&temporary, source.text, source.base.textLength);
    _ZN6icu_6813UnicodeStringaSEOS0__nid_postfix(result, &temporary);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&temporary);
}

}
