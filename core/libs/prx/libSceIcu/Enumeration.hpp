#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UEnumeration;

using EnumerationClose = void (APS5_VABI*)(UEnumeration*);
using EnumerationCount = std::int32_t (APS5_VABI*)(UEnumeration*, std::int32_t*);
using EnumerationUnicodeNext = const char16_t* (APS5_VABI*)(UEnumeration*, std::int32_t*, std::int32_t*);
using EnumerationNext = const char* (APS5_VABI*)(UEnumeration*, std::int32_t*, std::int32_t*);
using EnumerationReset = void (APS5_VABI*)(UEnumeration*, std::int32_t*);

struct UEnumeration {
    void* baseContext;
    void* context;
    EnumerationClose close;
    EnumerationCount count;
    EnumerationUnicodeNext uNext;
    EnumerationNext next;
    EnumerationReset reset;
};

static_assert(sizeof(UEnumeration) == 56);

extern "C" {
void APS5_VABI uenum_close_nid_postfix(UEnumeration*);
std::int32_t APS5_VABI uenum_count_nid_postfix(UEnumeration*, std::int32_t*);
const char16_t* APS5_VABI uenum_unext_nid_postfix(UEnumeration*, std::int32_t*, std::int32_t*);
const char* APS5_VABI uenum_next_nid_postfix(UEnumeration*, std::int32_t*, std::int32_t*);
}
