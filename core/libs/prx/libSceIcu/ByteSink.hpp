// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstddef>

namespace Icu::Abi {
struct ByteSink { const void* vtable; };
struct CheckedArrayByteSink {
    const void* vtable;
    char* buffer;
    std::int32_t capacity;
    std::int32_t size;
    std::int32_t appended;
    std::int8_t overflowed;
};
static_assert(sizeof(ByteSink) == 8 && alignof(ByteSink) == 8 && offsetof(ByteSink, vtable) == 0);
static_assert(sizeof(CheckedArrayByteSink) == 32 && alignof(CheckedArrayByteSink) == 8);
static_assert(offsetof(CheckedArrayByteSink, vtable) == 0 && offsetof(CheckedArrayByteSink, buffer) == 8);
static_assert(offsetof(CheckedArrayByteSink, capacity) == 16 && offsetof(CheckedArrayByteSink, size) == 20);
static_assert(offsetof(CheckedArrayByteSink, appended) == 24 && offsetof(CheckedArrayByteSink, overflowed) == 28);
}
extern "C" {
char* APS5_VABI _ZN6icu_688ByteSink15GetAppendBufferEiiPciPi_nid_postfix(void*, std::int32_t, std::int32_t, char*, std::int32_t, std::int32_t*);
void APS5_VABI _ZN6icu_688ByteSink5FlushEv_nid_postfix(void*);
void APS5_VABI _ZN6icu_688ByteSinkD1Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_688ByteSinkD2Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_688ByteSinkD0Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD1Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD2Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD0Ev_nid_postfix(void* object);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkC1EPci_nid_postfix(Icu::Abi::CheckedArrayByteSink* object, char* buffer, std::int32_t capacity);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkC2EPci_nid_postfix(Icu::Abi::CheckedArrayByteSink* object, char* buffer, std::int32_t capacity);
Icu::Abi::CheckedArrayByteSink* APS5_VABI _ZN6icu_6820CheckedArrayByteSink5ResetEv_nid_postfix(Icu::Abi::CheckedArrayByteSink* object);
void APS5_VABI _ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix(Icu::Abi::CheckedArrayByteSink* object, const char* source, std::int32_t count);
char* APS5_VABI _ZN6icu_6820CheckedArrayByteSink15GetAppendBufferEiiPciPi_nid_postfix(Icu::Abi::CheckedArrayByteSink* object, std::int32_t minimum, std::int32_t, char* scratch, std::int32_t capacity, std::int32_t* result);
}
