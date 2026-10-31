// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include "UnicodeString.hpp"

namespace Icu::Abi {
struct Appendable { const void* vtable; };
struct UnicodeStringAppendable { const void* vtable; UnicodeString* string; };
static_assert(sizeof(Appendable) == 8 && alignof(Appendable) == 8);
static_assert(sizeof(UnicodeStringAppendable) == 16 && alignof(UnicodeStringAppendable) == 8);
static_assert(offsetof(Appendable, vtable) == 0);
static_assert(offsetof(UnicodeStringAppendable, vtable) == 0 && offsetof(UnicodeStringAppendable, string) == 8);
}
extern "C" {
std::int8_t APS5_VABI _ZN6icu_6810Appendable21reserveAppendCapacityEi_nid_postfix(void*, std::int32_t);
char16_t* APS5_VABI _ZN6icu_6810Appendable15getAppendBufferEiiPDsiPi_nid_postfix(void*, std::int32_t, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_6810AppendableD1Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6810AppendableD2Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6810AppendableD0Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD1Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD2Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6823UnicodeStringAppendableD0Ev_nid_postfix(void* object);
std::int8_t APS5_VABI _ZN6icu_6810Appendable15appendCodePointEi_nid_postfix(void* object, std::int32_t point);
std::int8_t APS5_VABI _ZN6icu_6810Appendable12appendStringEPKDsi_nid_postfix(void* object, const char16_t* source, std::int32_t length);
std::int8_t APS5_VABI _ZN6icu_6823UnicodeStringAppendable14appendCodeUnitEDs_nid_postfix(void* object, char16_t unit);
std::int8_t APS5_VABI _ZN6icu_6823UnicodeStringAppendable15appendCodePointEi_nid_postfix(void* object, std::int32_t point);
std::int8_t APS5_VABI _ZN6icu_6823UnicodeStringAppendable12appendStringEPKDsi_nid_postfix(void* object, const char16_t* source, std::int32_t length);
std::int8_t APS5_VABI _ZN6icu_6823UnicodeStringAppendable21reserveAppendCapacityEi_nid_postfix(void* object, std::int32_t count);
char16_t* APS5_VABI _ZN6icu_6823UnicodeStringAppendable15getAppendBufferEiiPDsiPi_nid_postfix(void* object, std::int32_t minimum, std::int32_t desired, char16_t* scratch, std::int32_t capacity, std::int32_t* result);
}
