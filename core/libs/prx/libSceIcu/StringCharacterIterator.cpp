// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1998-2012, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstring>
#include <stdexcept>

namespace {
using namespace Icu::Abi;
template<typename TValue> TValue& Require(TValue* pointer) {
    if (!pointer) throw std::invalid_argument("ICU string iterator: null pointer");
    return *pointer;
}
void Construct(StringCharacterIterator& self, const UnicodeString* source, std::int32_t begin, std::int32_t end, std::int32_t position) {
    ConstChar16Ptr pointer{source->Buffer()};
    _ZN6icu_6822UCharCharacterIteratorC2ENS_14ConstChar16PtrEiiii_nid_postfix(&self.base, &pointer, source->Length(), begin, end, position);
    _ZN6icu_6813UnicodeStringC2ERKS0__nid_postfix(&self.text, source);
    self.base.text = self.text.Buffer();
    Initialize_nid_no_patch(&self, Class::StringCharacterIterator);
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
void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self) {
    auto& object = Require(self);
    _ZN6icu_6822UCharCharacterIteratorC2Ev_nid_postfix(&object.base);
    object.text.Init();
    Initialize_nid_no_patch(self, Class::StringCharacterIterator);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1ERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source) {
    Require(source);
    Construct(Require(self), source, 0, source->Length(), 0);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1ERKNS_13UnicodeStringEi_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source, std::int32_t position) {
    Require(source);
    Construct(Require(self), source, 0, source->Length(), position);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1ERKNS_13UnicodeStringEiii_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source, std::int32_t begin, std::int32_t end, std::int32_t position) {
    Require(source);
    Construct(Require(self), source, begin, end, position);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorC1ERKS0__nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::StringCharacterIterator* source) {
    auto& object = Require(self);
    const auto& other = Require(source);
    _ZN6icu_6822UCharCharacterIteratorC2ERKS0__nid_postfix(&object.base, &other.base);
    _ZN6icu_6813UnicodeStringC2ERKS0__nid_postfix(&object.text, &other.text);
    object.base.text = object.text.Buffer();
    Initialize_nid_no_patch(self, Class::StringCharacterIterator);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self) {
    auto& object = Require(self);
    _ZN6icu_6822UCharCharacterIteratorC2Ev_nid_postfix(&object.base);
    object.text.Init();
    Initialize_nid_no_patch(self, Class::StringCharacterIterator);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2ERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source) {
    Require(source);
    Construct(Require(self), source, 0, source->Length(), 0);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2ERKNS_13UnicodeStringEi_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source, std::int32_t position) {
    Require(source);
    Construct(Require(self), source, 0, source->Length(), position);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2ERKNS_13UnicodeStringEiii_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source, std::int32_t begin, std::int32_t end, std::int32_t position) {
    Require(source);
    Construct(Require(self), source, begin, end, position);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorC2ERKS0__nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::StringCharacterIterator* source) {
    auto& object = Require(self);
    const auto& other = Require(source);
    _ZN6icu_6822UCharCharacterIteratorC2ERKS0__nid_postfix(&object.base, &other.base);
    _ZN6icu_6813UnicodeStringC2ERKS0__nid_postfix(&object.text, &other.text);
    object.base.text = object.text.Buffer();
    Initialize_nid_no_patch(self, Class::StringCharacterIterator);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorD1Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self) {
    auto& object = Require(self);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&object.text);
    _ZN6icu_6822UCharCharacterIteratorD2Ev_nid_postfix(&object.base);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorD2Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self) {
    auto& object = Require(self);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&object.text);
    _ZN6icu_6822UCharCharacterIteratorD2Ev_nid_postfix(&object.base);
}

void APS5_VABI _ZN6icu_6823StringCharacterIteratorD0Ev_nid_postfix(Icu::Abi::StringCharacterIterator* self) {
    auto& object = Require(self);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&object.text);
    _ZN6icu_6822UCharCharacterIteratorD2Ev_nid_postfix(&object.base);
    Free_nid_no_patch(self);
}

Icu::Abi::StringCharacterIterator* APS5_VABI _ZN6icu_6823StringCharacterIteratoraSERKS0__nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::StringCharacterIterator* source) {
    auto& object = Require(self);
    const auto& other = Require(source);
    _ZN6icu_6822UCharCharacterIteratoraSERKS0__nid_postfix(&object.base, &other.base);
    _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(&object.text, &other.text);
    object.base.text = object.text.Buffer();
    return self;
}

std::int8_t APS5_VABI _ZNK6icu_6823StringCharacterIteratoreqERKNS_24ForwardCharacterIteratorE_nid_postfix(const Icu::Abi::StringCharacterIterator* self, const Icu::Abi::ForwardCharacterIterator* other) {
    const auto& object = Require(self);
    Require(other);
    if (reinterpret_cast<const void*>(self) == other) return 1;
    if (!SameType(object.base.base.vtable, other->vtable)) return 0;
    const auto& source = *reinterpret_cast<const StringCharacterIterator*>(other);
    return _ZNK6icu_6813UnicodeStringeqERKS0__nid_postfix(&object.text, &source.text) && object.base.base.position == source.base.base.position && object.base.base.begin == source.base.base.begin && object.base.base.end == source.base.base.end;
}

Icu::Abi::StringCharacterIterator* APS5_VABI _ZNK6icu_6823StringCharacterIterator5cloneEv_nid_postfix(const Icu::Abi::StringCharacterIterator* self) {
    Require(self);
    auto* result = static_cast<StringCharacterIterator*>(Allocate_nid_no_patch(sizeof(StringCharacterIterator)));
    if (result) _ZN6icu_6823StringCharacterIteratorC1ERKS0__nid_postfix(result, self);
    return result;
}

void APS5_VABI _ZN6icu_6823StringCharacterIterator7setTextERKNS_13UnicodeStringE_nid_postfix(Icu::Abi::StringCharacterIterator* self, const Icu::Abi::UnicodeString* source) {
    auto& object = Require(self);
    _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(&object.text, source);
    ConstChar16Ptr pointer{object.text.Buffer()};
    _ZN6icu_6822UCharCharacterIterator7setTextENS_14ConstChar16PtrEi_nid_postfix(&object.base, &pointer, object.text.Length());
}

void APS5_VABI _ZN6icu_6823StringCharacterIterator7getTextERNS_13UnicodeStringE_nid_postfix(Icu::Abi::StringCharacterIterator* self, Icu::Abi::UnicodeString* result) {
    _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(result, &Require(self).text);
}

void* APS5_VABI _ZN6icu_6823StringCharacterIterator16getStaticClassIDEv_nid_postfix() {
    return ClassId_nid_no_patch(Class::StringCharacterIterator);
}

void* APS5_VABI _ZNK6icu_6823StringCharacterIterator17getDynamicClassIDEv_nid_postfix(const Icu::Abi::StringCharacterIterator*) {
    return ClassId_nid_no_patch(Class::StringCharacterIterator);
}

}
