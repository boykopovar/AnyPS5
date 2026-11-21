#pragma once
#include "prx/libSceIcu/Enumeration.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

using UEnumeration = ::UEnumeration;

}

extern "C" {
const char16_t* APS5_VABI uenum_unextDefault_nid_postfix(Icu::UEnumeration*, std::int32_t*, std::int32_t*);
const char* APS5_VABI uenum_nextDefault_nid_postfix(Icu::UEnumeration*, std::int32_t*, std::int32_t*);
Icu::UEnumeration* APS5_VABI uenum_openCharStringsEnumeration_nid_postfix(const char* const*, std::int32_t, std::int32_t*);
Icu::UEnumeration* APS5_VABI uenum_openUCharStringsEnumeration_nid_postfix(const char16_t* const*, std::int32_t, std::int32_t*);
void APS5_VABI uenum_reset_nid_postfix(Icu::UEnumeration*, std::int32_t*);
}
