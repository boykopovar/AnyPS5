// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2009-2015, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
using namespace Icu::Abi;

template<typename TValue>
TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU locale builder: null pointer");
    return *value;
}

const DerivedTypeInfo BuilderType{{SingleTypeTable_nid_no_patch(), "N6icu_6813LocaleBuilderE"}, Rtti_nid_no_patch(Class::Object)};
struct BuilderTable {
    std::ptrdiff_t offset;
    const TypeInfo* type;
    GuestFunction slots[3];
};
static_assert(sizeof(BuilderTable) == 40 && alignof(BuilderTable) == 8);
static_assert(offsetof(BuilderTable, offset) == 0 && offsetof(BuilderTable, type) == 8 && offsetof(BuilderTable, slots) == 16);
const BuilderTable Table{0, &BuilderType.type, {
    reinterpret_cast<GuestFunction>(_ZN6icu_6813LocaleBuilderD1Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZN6icu_6813LocaleBuilderD0Ev_nid_postfix),
    reinterpret_cast<GuestFunction>(_ZNK6icu_687UObject17getDynamicClassIDEv_nid_postfix)
}};

void DeleteVariant(CharString* variant) {
    if (!variant) return;
    _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(&variant->buffer);
    Free_nid_no_patch(variant);
}

bool Alpha(char value) { return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z'); }
bool Digit(char value) { return value >= '0' && value <= '9'; }

void SetField(StringPiece input, char* destination, std::int32_t& status, int kind) {
    if (status > 0) return;
    if (!input.length) { destination[0] = 0; return; }
    auto length = input.length;
    if (length < 0) length = CheckedInt_nid_no_patch(std::strlen(&Require(input.pointer)));
    bool valid = kind == 0 ? length >= 2 && length <= 8 : kind == 1 ? length == 4 : length == 2 || length == 3;
    if (valid) {
        Require(input.pointer);
        for (std::int32_t i = 0; i < length; ++i) {
            if (!(kind == 2 && length == 3 ? Digit(input.pointer[i]) : Alpha(input.pointer[i]))) { valid = false; break; }
        }
    }
    if (!valid) { status = 1; return; }
    if (input.length < 0) throw std::invalid_argument("ICU locale builder: negative copy size");
    const auto from = reinterpret_cast<std::uintptr_t>(input.pointer);
    const auto to = reinterpret_cast<std::uintptr_t>(destination);
    const auto size = static_cast<std::uintptr_t>(input.length);
    if (from <= to ? to - from < size : from - to < size) throw std::invalid_argument("ICU locale builder: overlapping copy");
    std::memcpy(destination, input.pointer, input.length);
    destination[input.length] = 0;
}

bool ValidVariant(const CharString& text) {
    std::int32_t start = 0;
    for (std::int32_t i = 0; i <= text.length; ++i) {
        if (i != text.length && text.buffer.pointer[i] != '-') continue;
        const auto length = i - start;
        if (!((length >= 5 && length <= 8) || (length == 4 && Digit(text.buffer.pointer[start])))) return false;
        for (auto j = start; j < i; ++j) if (!Alpha(text.buffer.pointer[j]) && !Digit(text.buffer.pointer[j])) return false;
        start = i + 1;
    }
    return true;
}

}

extern "C" {
void APS5_VABI _ZN6icu_6813LocaleBuilderC1Ev_nid_postfix(LocaleBuilder* builder) {
    auto& value = Require(builder);
    value.vtable = Table.slots;
    value.status = 0;
    std::fill_n(value.language, 9, 0);
    std::fill_n(value.script, 5, 0);
    std::fill_n(value.region, 4, 0);
    value.variant = nullptr;
    value.extensions = nullptr;
}
void APS5_VABI _ZN6icu_6813LocaleBuilderC2Ev_nid_postfix(LocaleBuilder* builder) { _ZN6icu_6813LocaleBuilderC1Ev_nid_postfix(builder); }
void APS5_VABI _ZN6icu_6813LocaleBuilderD1Ev_nid_postfix(LocaleBuilder* builder) {
    DeleteVariant(Require(builder).variant);
    if (builder->extensions) _ZN6icu_686LocaleD0Ev_nid_postfix(builder->extensions);
    _ZN6icu_687UObjectD1Ev_nid_postfix(builder);
}
void APS5_VABI _ZN6icu_6813LocaleBuilderD2Ev_nid_postfix(LocaleBuilder* builder) { _ZN6icu_6813LocaleBuilderD1Ev_nid_postfix(builder); }
void APS5_VABI _ZN6icu_6813LocaleBuilderD0Ev_nid_postfix(LocaleBuilder* builder) {
    _ZN6icu_6813LocaleBuilderD1Ev_nid_postfix(builder);
    Free_nid_no_patch(builder);
}
LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder11setLanguageENS_11StringPieceE_nid_postfix(LocaleBuilder* builder, StringPiece input) {
    auto& value = Require(builder);
    SetField(input, value.language, value.status, 0);
    return builder;
}
LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder9setScriptENS_11StringPieceE_nid_postfix(LocaleBuilder* builder, StringPiece input) {
    auto& value = Require(builder);
    SetField(input, value.script, value.status, 1);
    return builder;
}
LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder9setRegionENS_11StringPieceE_nid_postfix(LocaleBuilder* builder, StringPiece input) {
    auto& value = Require(builder);
    SetField(input, value.region, value.status, 2);
    return builder;
}
LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder10setVariantENS_11StringPieceE_nid_postfix(LocaleBuilder* builder, StringPiece input) {
    auto& value = Require(builder);
    if (value.status > 0) return builder;
    if (!input.length) {
        DeleteVariant(value.variant);
        value.variant = nullptr;
        return builder;
    }
    auto* variant = static_cast<CharString*>(Allocate_nid_no_patch(sizeof(CharString)));
    if (!variant) { value.status = 7; return builder; }
    _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(&variant->buffer);
    variant->length = 0;
    variant->buffer.pointer[0] = 0;
    _ZN6icu_6810CharString6appendEPKciR10UErrorCode_nid_postfix(variant, input.pointer, input.length, &value.status);
    if (value.status > 0) { DeleteVariant(variant); return builder; }
    for (std::int32_t i = 0; i < variant->length; ++i) {
        auto& unit = variant->buffer.pointer[i];
        if (unit == '_') unit = '-';
        else if (unit >= 'A' && unit <= 'Z') unit += 'a' - 'A';
    }
    if (!ValidVariant(*variant)) {
        DeleteVariant(variant);
        value.status = 1;
        return builder;
    }
    DeleteVariant(value.variant);
    value.variant = variant;
    return builder;
}
LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder15clearExtensionsEv_nid_postfix(LocaleBuilder* builder) {
    auto& value = Require(builder);
    if (value.extensions) _ZN6icu_686LocaleD0Ev_nid_postfix(value.extensions);
    value.extensions = nullptr;
    return builder;
}
LocaleBuilder* APS5_VABI _ZN6icu_6813LocaleBuilder5clearEv_nid_postfix(LocaleBuilder* builder) {
    auto& value = Require(builder);
    value.status = 0;
    value.language[0] = value.script[0] = value.region[0] = 0;
    DeleteVariant(value.variant);
    value.variant = nullptr;
    return _ZN6icu_6813LocaleBuilder15clearExtensionsEv_nid_postfix(builder);
}
std::int8_t APS5_VABI _ZNK6icu_6813LocaleBuilder11copyErrorToER10UErrorCode_nid_postfix(const LocaleBuilder* builder, std::int32_t* status) {
    if (Require(status) > 0) return 1;
    *status = Require(builder).status;
    return *status > 0;
}
}
