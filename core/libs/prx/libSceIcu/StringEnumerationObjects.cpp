// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2002-2012, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (c) 2002-2014, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
using namespace Icu::Abi;
using NextChars = const char* (APS5_VABI *)(StringEnumeration*, std::int32_t*, std::int32_t*);
using NextUnits = const char16_t* (APS5_VABI *)(StringEnumeration*, std::int32_t*, std::int32_t*);
using NextString = const UnicodeString* (APS5_VABI *)(StringEnumeration*, std::int32_t*);
using Equals = std::int8_t (APS5_VABI *)(const StringEnumeration*, const StringEnumeration*);
template<typename TValue> TValue& Require(TValue* pointer) {
    if (!pointer) throw std::invalid_argument("ICU string enumeration: null pointer");
    return *pointer;
}
std::int32_t CheckedLength(const char* text) {
    const auto length = std::strlen(text);
    if (length > INT32_MAX) throw std::invalid_argument("ICU string enumeration: length overflow");
    return static_cast<std::int32_t>(length);
}
void Close(Icu::UEnumeration* enumeration) {
    if (!enumeration) return;
    if (enumeration->close) {
        Free_nid_no_patch(enumeration->baseContext);
        enumeration->close(enumeration);
    } else Free_nid_no_patch(enumeration);
}
void Destroy(StringEnumeration& object) {
    if (object.chars && object.chars != object.charsBuffer) Free_nid_no_patch(object.chars);
    _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&object.unistr);
    Initialize_nid_no_patch(&object, Class::StringEnumeration);
}
void Delete(StringEnumeration* object) {
    if (object) Virtual_nid_no_patch<void (APS5_VABI *)(StringEnumeration*)>(object, 1)(object);
}
StringEnumeration* Context(Icu::UEnumeration* enumeration) {
    return static_cast<StringEnumeration*>(Require(enumeration).context);
}
void APS5_VABI WrapperClose(Icu::UEnumeration* enumeration) {
    Delete(Context(enumeration));
    Free_nid_no_patch(enumeration);
}
std::int32_t APS5_VABI WrapperCount(Icu::UEnumeration* enumeration, std::int32_t* status) {
    Require(status);
    auto* object = Context(enumeration);
    return Virtual_nid_no_patch<std::int32_t (APS5_VABI *)(const StringEnumeration*, std::int32_t*)>(object, 4)(object, status);
}
const char16_t* APS5_VABI WrapperUNext(Icu::UEnumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    Require(status);
    auto* object = Context(enumeration);
    return Virtual_nid_no_patch<NextUnits>(object, 6)(object, length, status);
}
const char* APS5_VABI WrapperNext(Icu::UEnumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    Require(status);
    auto* object = Context(enumeration);
    return Virtual_nid_no_patch<NextChars>(object, 5)(object, length, status);
}
void APS5_VABI WrapperReset(Icu::UEnumeration* enumeration, std::int32_t* status) {
    Require(status);
    auto* object = Context(enumeration);
    Virtual_nid_no_patch<void (APS5_VABI *)(StringEnumeration*, std::int32_t*)>(object, 8)(object, status);
}
}

extern "C" {
void APS5_VABI _ZN6icu_6817StringEnumerationC2Ev_nid_postfix(Icu::Abi::StringEnumeration* self) {
    auto& object = Require(self);
    object.unistr.Init();
    object.chars = object.charsBuffer;
    object.charsCapacity = sizeof(object.charsBuffer);
    Initialize_nid_no_patch(self, Class::StringEnumeration);
}

void APS5_VABI _ZN6icu_6817StringEnumerationD1Ev_nid_postfix(Icu::Abi::StringEnumeration* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_6817StringEnumerationD2Ev_nid_postfix(Icu::Abi::StringEnumeration* self) {
    Destroy(Require(self));
}

void APS5_VABI _ZN6icu_6817StringEnumerationD0Ev_nid_postfix(Icu::Abi::StringEnumeration* self) {
    Destroy(Require(self));
    Free_nid_no_patch(self);
}

void APS5_VABI _ZN6icu_6818UStringEnumerationD1Ev_nid_postfix(Icu::Abi::UStringEnumeration* self) {
    Close(Require(self).enumeration);
    Destroy(Require(self).base);
}

void APS5_VABI _ZN6icu_6818UStringEnumerationD2Ev_nid_postfix(Icu::Abi::UStringEnumeration* self) {
    Close(Require(self).enumeration);
    Destroy(Require(self).base);
}

void APS5_VABI _ZN6icu_6818UStringEnumerationD0Ev_nid_postfix(Icu::Abi::UStringEnumeration* self) {
    Close(Require(self).enumeration);
    Destroy(Require(self).base);
    Free_nid_no_patch(self);
}

Icu::Abi::StringEnumeration* APS5_VABI _ZNK6icu_6817StringEnumeration5cloneEv_nid_postfix(const Icu::Abi::StringEnumeration*) {
    return nullptr;
}

void APS5_VABI _ZN6icu_6817StringEnumeration19ensureCharsCapacityEiR10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, std::int32_t capacity, std::int32_t* status) {
    auto& error = Require(status);
    if (error > 0) return;
    auto& object = Require(self);
    if (capacity <= object.charsCapacity) return;
    capacity = std::max(capacity, CheckedInt_nid_no_patch(static_cast<std::int64_t>(object.charsCapacity) + object.charsCapacity / 2));
    if (object.chars != object.charsBuffer) Free_nid_no_patch(object.chars);
    object.chars = static_cast<char*>(Allocate_nid_no_patch(capacity));
    if (!object.chars) {
        object.chars = object.charsBuffer;
        object.charsCapacity = sizeof(object.charsBuffer);
        error = 7;
    } else object.charsCapacity = capacity;
}

Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6817StringEnumeration8setCharsEPKciR10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, const char* text, std::int32_t length, std::int32_t* status) {
    auto& error = Require(status);
    if (error > 0 || !text) return nullptr;
    auto& object = Require(self);
    if (length < 0) length = CheckedLength(text);
    auto* buffer = _ZN6icu_6813UnicodeString9getBufferEi_nid_postfix(&object.unistr, CheckedInt_nid_no_patch(static_cast<std::int64_t>(length) + 1));
    if (!buffer) { error = 7; return nullptr; }
    u_charsToUChars_nid_postfix(text, buffer, length);
    buffer[length] = 0;
    _ZN6icu_6813UnicodeString13releaseBufferEi_nid_postfix(&object.unistr, length);
    return &object.unistr;
}

const Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6817StringEnumeration5snextER10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, std::int32_t* status) {
    Require(status);
    std::int32_t length = 0;
    auto next = Virtual_nid_no_patch<NextChars>(self, 5);
    const auto* text = next(self, &length, status);
    return _ZN6icu_6817StringEnumeration8setCharsEPKciR10UErrorCode_nid_postfix(self, text, length, status);
}

const char16_t* APS5_VABI _ZN6icu_6817StringEnumeration5unextEPiR10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, std::int32_t* length, std::int32_t* status) {
    Require(status);
    auto next = Virtual_nid_no_patch<NextString>(self, 7);
    const auto* text = next(self, status);
    if (*status > 0 || !text) return nullptr;
    auto& object = Require(self);
    _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(&object.unistr, text);
    if (length) *length = object.unistr.Length();
    return _ZN6icu_6813UnicodeString19getTerminatedBufferEv_nid_postfix(&object.unistr);
}

const char* APS5_VABI _ZN6icu_6817StringEnumeration4nextEPiR10UErrorCode_nid_postfix(Icu::Abi::StringEnumeration* self, std::int32_t* length, std::int32_t* status) {
    Require(status);
    auto next = Virtual_nid_no_patch<NextString>(self, 7);
    const auto* text = next(self, status);
    if (*status > 0 || !text) return nullptr;
    auto& object = Require(self);
    _ZN6icu_6813UnicodeStringaSERKS0__nid_postfix(&object.unistr, text);
    _ZN6icu_6817StringEnumeration19ensureCharsCapacityEiR10UErrorCode_nid_postfix(self, CheckedInt_nid_no_patch(static_cast<std::int64_t>(object.unistr.Length()) + 1), status);
    if (*status > 0) return nullptr;
    if (length) *length = object.unistr.Length();
    _ZNK6icu_6813UnicodeString7extractEiiPciNS0_10EInvariantE_nid_postfix(&object.unistr, 0, INT32_MAX, object.chars, object.charsCapacity, 0);
    return object.chars;
}

std::int8_t APS5_VABI _ZNK6icu_6817StringEnumerationeqERKS0__nid_postfix(const Icu::Abi::StringEnumeration* self, const Icu::Abi::StringEnumeration* other) {
    Require(self);
    Require(other);
    Require(self->vtable);
    Require(other->vtable);
    const auto* first = reinterpret_cast<const TypeInfo* const*>(self->vtable)[-1];
    const auto* second = reinterpret_cast<const TypeInfo* const*>(other->vtable)[-1];
    Require(first);
    Require(second);
    Require(first->name);
    Require(second->name);
    return first == second || std::strcmp(first->name, second->name) == 0;
}

std::int8_t APS5_VABI _ZNK6icu_6817StringEnumerationneERKS0__nid_postfix(const Icu::Abi::StringEnumeration* self, const Icu::Abi::StringEnumeration* other) {
    auto equals = Virtual_nid_no_patch<Equals>(self, 9);
    return !equals(self, other);
}

void APS5_VABI _ZN6icu_6818UStringEnumerationC1EP12UEnumeration_nid_postfix(Icu::Abi::UStringEnumeration* self, Icu::UEnumeration* enumeration) {
    auto& object = Require(self);
    _ZN6icu_6817StringEnumerationC2Ev_nid_postfix(&object.base);
    object.enumeration = enumeration;
    Initialize_nid_no_patch(self, Class::UStringEnumeration);
}

void APS5_VABI _ZN6icu_6818UStringEnumerationC2EP12UEnumeration_nid_postfix(Icu::Abi::UStringEnumeration* self, Icu::UEnumeration* enumeration) {
    auto& object = Require(self);
    _ZN6icu_6817StringEnumerationC2Ev_nid_postfix(&object.base);
    object.enumeration = enumeration;
    Initialize_nid_no_patch(self, Class::UStringEnumeration);
}

Icu::Abi::UStringEnumeration* APS5_VABI _ZN6icu_6818UStringEnumeration16fromUEnumerationEP12UEnumerationR10UErrorCode_nid_postfix(Icu::UEnumeration* enumeration, std::int32_t* status) {
    auto& error = Require(status);
    if (error > 0) { Close(enumeration); return nullptr; }
    auto* result = static_cast<UStringEnumeration*>(Allocate_nid_no_patch(sizeof(UStringEnumeration)));
    if (!result) { error = 7; Close(enumeration); return nullptr; }
    _ZN6icu_6818UStringEnumerationC1EP12UEnumeration_nid_postfix(result, enumeration);
    return result;
}

std::int32_t APS5_VABI _ZNK6icu_6818UStringEnumeration5countER10UErrorCode_nid_postfix(const Icu::Abi::UStringEnumeration* self, std::int32_t* status) {
    auto* enumeration = Require(self).enumeration;
    if (!enumeration || Require(status) > 0) return -1;
    if (enumeration->count) return enumeration->count(enumeration, status);
    *status = 16;
    return -1;
}

const char* APS5_VABI _ZN6icu_6818UStringEnumeration4nextEPiR10UErrorCode_nid_postfix(Icu::Abi::UStringEnumeration* self, std::int32_t* length, std::int32_t* status) {
    auto* enumeration = Require(self).enumeration;
    if (!enumeration || Require(status) > 0) return nullptr;
    if (!enumeration->next) { *status = 16; return nullptr; }
    std::int32_t dummyLength = 0;
    return enumeration->next(enumeration, length ? length : &dummyLength, status);
}

const Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6818UStringEnumeration5snextER10UErrorCode_nid_postfix(Icu::Abi::UStringEnumeration* self, std::int32_t* status) {
    auto& object = Require(self);
    auto* enumeration = object.enumeration;
    if (!enumeration || Require(status) > 0) return nullptr;
    if (!enumeration->uNext) { *status = 16; return nullptr; }
    std::int32_t length = 0;
    const auto* text = enumeration->uNext(enumeration, &length, status);
    if (!text || *status > 0) return nullptr;
    _ZN6icu_6813UnicodeString7unBogusEv_nid_postfix(&object.base.unistr);
    return object.base.unistr.Replace(0, object.base.unistr.Length(), text, 0, length);
}

void APS5_VABI _ZN6icu_6818UStringEnumeration5resetER10UErrorCode_nid_postfix(Icu::Abi::UStringEnumeration* self, std::int32_t* status) {
    uenum_reset_nid_postfix(Require(self).enumeration, status);
}

void* APS5_VABI _ZN6icu_6818UStringEnumeration16getStaticClassIDEv_nid_postfix() {
    return ClassId_nid_no_patch(Class::UStringEnumeration);
}

void* APS5_VABI _ZNK6icu_6818UStringEnumeration17getDynamicClassIDEv_nid_postfix(const Icu::Abi::UStringEnumeration*) {
    return ClassId_nid_no_patch(Class::UStringEnumeration);
}

Icu::UEnumeration* APS5_VABI uenum_openFromStringEnumeration_nid_postfix(Icu::Abi::StringEnumeration* adopted, std::int32_t* status) {
    auto& error = Require(status);
    if (error <= 0 && adopted) {
        auto* result = static_cast<Icu::UEnumeration*>(Allocate_nid_no_patch(sizeof(Icu::UEnumeration)));
        if (result) {
            *result = {nullptr, adopted, WrapperClose, WrapperCount, WrapperUNext, WrapperNext, WrapperReset};
            return result;
        }
        error = 7;
    }
    Delete(adopted);
    return nullptr;
}

}
