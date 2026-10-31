// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <stdexcept>

namespace {
Icu::Abi::UnicodeString& StringOf(void* object) {
    if (!object || !static_cast<Icu::Abi::UnicodeStringAppendable*>(object)->string) throw std::invalid_argument("UnicodeStringAppendable: null object or string");
    return *static_cast<Icu::Abi::UnicodeStringAppendable*>(object)->string;
}
}
extern "C" {

std::int8_t APS5_VABI _ZN6icu_6810Appendable21reserveAppendCapacityEi_nid_postfix(void*, std::int32_t) {
    return 1;
}

char16_t* APS5_VABI _ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix(void*, std::int32_t minimum, std::int32_t, char16_t* scratch, std::int32_t capacity, std::int32_t* resultCapacity) {
    if (!resultCapacity) throw std::invalid_argument("Appendable: null result capacity");
    if (minimum < 1 || capacity < minimum) {
        *resultCapacity = 0;
        return nullptr;
    }
    *resultCapacity = capacity;
    return scratch;
}

void APS5_VABI _ZN6icu_6810AppendableD1Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::Appendable);
    _ZN6icu_687UObjectD2Ev_nid_postfix(object);
}

void APS5_VABI _ZN6icu_6810AppendableD2Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::Appendable);
    _ZN6icu_687UObjectD2Ev_nid_postfix(object);
}

void APS5_VABI _ZN6icu_6810AppendableD0Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::Appendable);
    _ZN6icu_687UObjectD2Ev_nid_postfix(object);
    Icu::Abi::Free_nid_no_patch(object);
}

void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD1Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::UnicodeStringAppendable);
    _ZN6icu_6810AppendableD2Ev_nid_postfix(object);
}

void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD2Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::UnicodeStringAppendable);
    _ZN6icu_6810AppendableD2Ev_nid_postfix(object);
}

void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD0Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::UnicodeStringAppendable);
    _ZN6icu_6810AppendableD2Ev_nid_postfix(object);
    Icu::Abi::Free_nid_no_patch(object);
}

std::int8_t APS5_VABI _ZN6icu_6810Appendable15appendCodePointEi_nid_postfix(void* object, std::int32_t point) {
    using AppendUnit = std::int8_t (APS5_VABI*)(void*, char16_t);
    const auto append = Icu::Abi::Virtual_nid_no_patch<AppendUnit>(object, 3);
    if (point <= 0xffff) return append(object, static_cast<char16_t>(point));
    const auto lead = static_cast<char16_t>((point >> 10) + 0xd7c0);
    const auto trail = static_cast<char16_t>((point & 1023) | 0xdc00);
    return append(object, lead) && Icu::Abi::Virtual_nid_no_patch<AppendUnit>(object, 3)(object, trail);
}

std::int8_t APS5_VABI _ZN6icu_6810Appendable12appendStringEPKDsi_nid_postfix(void* object, const char16_t* source, std::int32_t length) {
    using AppendUnit = std::int8_t (APS5_VABI*)(void*, char16_t);
    if (length == 0) return 1;
    if (length > 0) {
        if (!source) throw std::invalid_argument("Appendable: null string");
        for (std::int32_t index = 0; index < length; ++index) if (!Icu::Abi::Virtual_nid_no_patch<AppendUnit>(object, 3)(object, source[index])) return 0;
    } else {
        if (!source) throw std::invalid_argument("Appendable: null string");
        while (*source) if (!Icu::Abi::Virtual_nid_no_patch<AppendUnit>(object, 3)(object, *source++)) return 0;
    }
    return 1;
}

std::int8_t APS5_VABI _ZN6icu_6823UnicodeStringAppendable14appendCodeUnitEDs_nid_postfix(void* object, char16_t unit) {
    auto& text = StringOf(object);
    text.Append(&unit, 0, 1);
    return text.Writable();
}

std::int8_t APS5_VABI _ZN6icu_6823UnicodeStringAppendable15appendCodePointEi_nid_postfix(void* object, std::int32_t point) {
    if (static_cast<std::uint32_t>(point) > 0x10ffff) return 0;
    auto& text = StringOf(object);
    _ZN6icu_6813UnicodeString6appendEi_nid_postfix(&text, point);
    return text.Writable();
}

std::int8_t APS5_VABI _ZN6icu_6823UnicodeStringAppendable12appendStringEPKDsi_nid_postfix(void* object, const char16_t* source, std::int32_t length) {
    auto& text = StringOf(object);
    text.Append(source, 0, length);
    return text.Writable();
}

std::int8_t APS5_VABI _ZN6icu_6823UnicodeStringAppendable21reserveAppendCapacityEi_nid_postfix(void* object, std::int32_t count) {
    auto& text = StringOf(object);
    return text.CloneBuffer(Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(text.Length()) + count));
}

char16_t* APS5_VABI _ZN6icu_6823UnicodeStringAppendable15getAppendBufferEiiPDsiPi_nid_postfix(void* object, std::int32_t minimum, std::int32_t desired, char16_t* scratch, std::int32_t capacity, std::int32_t* result) {
    if (!result) throw std::invalid_argument("UnicodeStringAppendable: null capacity");
    if (minimum < 1 || capacity < minimum) { *result = 0; return nullptr; }
    auto& text = StringOf(object);
    const auto length = text.Length();
    if (minimum <= 0x7ffffff5 - length && desired <= 0x7ffffff5 - length && text.CloneBuffer(length + minimum, Icu::Abi::CheckedInt_nid_no_patch(static_cast<std::int64_t>(length) + desired))) {
        *result = text.Capacity() - length;
        return text.Array() + length;
    }
    *result = capacity;
    return scratch;
}

}
