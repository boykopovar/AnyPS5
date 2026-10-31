// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

struct UEnumeration {
    void* baseContext;
    void* context;
    void (APS5_VABI* close)(UEnumeration*);
    std::int32_t (APS5_VABI* count)(UEnumeration*, std::int32_t*);
    const char16_t* (APS5_VABI* uNext)(UEnumeration*, std::int32_t*, std::int32_t*);
    const char* (APS5_VABI* next)(UEnumeration*, std::int32_t*, std::int32_t*);
    void (APS5_VABI* reset)(UEnumeration*, std::int32_t*);
};

static_assert(sizeof(UEnumeration) == 56 && alignof(UEnumeration) == 8);
static_assert(offsetof(UEnumeration, baseContext) == 0 && offsetof(UEnumeration, context) == 8);
static_assert(offsetof(UEnumeration, close) == 16 && offsetof(UEnumeration, count) == 24);
static_assert(offsetof(UEnumeration, uNext) == 32 && offsetof(UEnumeration, next) == 40);
static_assert(offsetof(UEnumeration, reset) == 48);

}

extern "C" {
const char16_t* APS5_VABI uenum_unextDefault_nid_postfix(Icu::UEnumeration*, std::int32_t*, std::int32_t*);
const char* APS5_VABI uenum_nextDefault_nid_postfix(Icu::UEnumeration*, std::int32_t*, std::int32_t*);
Icu::UEnumeration* APS5_VABI uenum_openCharStringsEnumeration_nid_postfix(const char* const*, std::int32_t, std::int32_t*);
Icu::UEnumeration* APS5_VABI uenum_openUCharStringsEnumeration_nid_postfix(const char16_t* const*, std::int32_t, std::int32_t*);
void APS5_VABI uenum_reset_nid_postfix(Icu::UEnumeration*, std::int32_t*);
}
