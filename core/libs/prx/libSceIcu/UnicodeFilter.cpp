// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (c) 2001-2012, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (c) 2002-2004, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <stdexcept>

namespace {

using namespace Icu::Abi;

template<typename T> T* Require(T* pointer) {
    if (!pointer) throw std::invalid_argument("null pointer");
    return pointer;
}

}

extern "C" {

void APS5_VABI _ZN6icu_6814UnicodeMatcherD2Ev_nid_postfix(UnicodeMatcher* self) {
    Require(self);
}

void APS5_VABI _ZN6icu_6814UnicodeMatcherD1Ev_nid_postfix(UnicodeMatcher* self) {
    _ZN6icu_6814UnicodeMatcherD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6814UnicodeMatcherD0Ev_nid_postfix(UnicodeMatcher* self) {
    auto* matcher = Require(self);
    _ZN6icu_6814UnicodeMatcherD1Ev_nid_postfix(matcher);
    Free_nid_no_patch(matcher);
}

void APS5_VABI _ZN6icu_6814UnicodeFunctorD2Ev_nid_postfix(UnicodeFunctor* self) {
    Require(self);
}

void APS5_VABI _ZN6icu_6814UnicodeFunctorD1Ev_nid_postfix(UnicodeFunctor* self) {
    _ZN6icu_6814UnicodeFunctorD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6814UnicodeFunctorD0Ev_nid_postfix(UnicodeFunctor* self) {
    auto* functor = Require(self);
    _ZN6icu_6814UnicodeFunctorD1Ev_nid_postfix(functor);
    Free_nid_no_patch(functor);
}

void* APS5_VABI _ZN6icu_6814UnicodeFunctor16getStaticClassIDEv_nid_postfix() {
    static char fgClassID = 0;
    return &fgClassID;
}

void* APS5_VABI _ZNK6icu_6814UnicodeFunctor9toMatcherEv_nid_postfix(const UnicodeFunctor* self) {
    Require(self);
    return nullptr;
}

void* APS5_VABI _ZNK6icu_6814UnicodeFunctor10toReplacerEv_nid_postfix(const UnicodeFunctor* self) {
    Require(self);
    return nullptr;
}

void APS5_VABI _ZN6icu_6813UnicodeFilterD2Ev_nid_postfix(UnicodeFilter* self) {
    Require(self);
}

void APS5_VABI _ZN6icu_6813UnicodeFilterD1Ev_nid_postfix(UnicodeFilter* self) {
    _ZN6icu_6813UnicodeFilterD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6813UnicodeFilterD0Ev_nid_postfix(UnicodeFilter* self) {
    auto* filter = Require(self);
    _ZN6icu_6813UnicodeFilterD1Ev_nid_postfix(filter);
    Free_nid_no_patch(filter);
}

void APS5_VABI _ZThn8_N6icu_6813UnicodeFilterD1Ev_nid_postfix(void* self) {
    auto* base = reinterpret_cast<UnicodeFilter*>(static_cast<char*>(Require(self)) - 8);
    _ZN6icu_6813UnicodeFilterD1Ev_nid_postfix(base);
}

void APS5_VABI _ZThn8_N6icu_6813UnicodeFilterD0Ev_nid_postfix(void* self) {
    auto* base = reinterpret_cast<UnicodeFilter*>(static_cast<char*>(Require(self)) - 8);
    _ZN6icu_6813UnicodeFilterD0Ev_nid_postfix(base);
}

void* APS5_VABI _ZN6icu_6813UnicodeFilter16getStaticClassIDEv_nid_postfix() {
    static char fgClassID = 0;
    return &fgClassID;
}

void APS5_VABI _ZN6icu_6813UnicodeFilter7setDataEPKNS_23TransliterationRuleDataE_nid_postfix(UnicodeFilter* self, const void*) {
    Require(self);
}

void* APS5_VABI _ZNK6icu_6813UnicodeFilter9toMatcherEv_nid_postfix(const UnicodeFilter* self) {
    return const_cast<void*>(static_cast<const void*>(reinterpret_cast<const char*>(Require(self)) + 8));
}

std::int32_t APS5_VABI _ZN6icu_6813UnicodeFilter7matchesERKNS_11ReplaceableERiia_nid_postfix(const UnicodeFilter* self, const void* text, std::int32_t* offset, std::int32_t limit, std::int8_t incremental) {
    const auto* filter = Require(self);
    const auto* rep = Require(text);
    auto* off = Require(offset);

    auto char32At = Virtual_nid_no_patch<std::int32_t (APS5_VABI*)(const void*, std::int32_t)>(rep, 10);
    auto contains = Virtual_nid_no_patch<std::int8_t (APS5_VABI*)(const void*, std::int32_t)>(filter, 7);

    int32_t pos = *off;
    if (pos < limit) {
        std::int32_t c = char32At(rep, pos);
        if (contains(filter, c)) {
            *off = pos + (c <= 0xffff ? 1 : 2);
            return 2;
        }
    }
    if (pos > limit) {
        std::int32_t c = char32At(rep, pos);
        if (contains(filter, c)) {
            --pos;
            if (pos >= 0) {
                std::int32_t prev = char32At(rep, pos);
                pos -= (prev <= 0xffff ? 1 : 2) - 1;
            }
            *off = pos;
            return 2;
        }
    }
    if (incremental && pos == limit) {
        return 1;
    }
    return 0;
}

std::int32_t APS5_VABI _ZThn8_N6icu_6813UnicodeFilter7matchesERKNS_11ReplaceableERiia_nid_postfix(const void* self, const void* text, std::int32_t* offset, std::int32_t limit, std::int8_t incremental) {
    const auto* base = reinterpret_cast<const UnicodeFilter*>(static_cast<const char*>(Require(self)) - 8);
    return _ZN6icu_6813UnicodeFilter7matchesERKNS_11ReplaceableERiia_nid_postfix(base, text, offset, limit, incremental);
}

void APS5_VABI _ZN6icu_6811SymbolTableD2Ev_nid_postfix(SymbolTable* self) {
    Require(self);
}

void APS5_VABI _ZN6icu_6811SymbolTableD1Ev_nid_postfix(SymbolTable* self) {
    _ZN6icu_6811SymbolTableD2Ev_nid_postfix(self);
}

void APS5_VABI _ZN6icu_6811SymbolTableD0Ev_nid_postfix(SymbolTable* self) {
    auto* table = Require(self);
    _ZN6icu_6811SymbolTableD1Ev_nid_postfix(table);
    Free_nid_no_patch(table);
}

}
