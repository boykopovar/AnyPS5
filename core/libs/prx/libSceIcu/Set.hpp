#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct USet;

extern "C" {
USet* APS5_VABI uset_openEmpty_nid_postfix();
void APS5_VABI uset_close_nid_postfix(USet*);
void APS5_VABI uset_add_nid_postfix(USet*, std::int32_t);
void APS5_VABI uset_addRange_nid_postfix(USet*, std::int32_t, std::int32_t);
void APS5_VABI uset_addAll_nid_postfix(USet*, const USet*);
std::int8_t APS5_VABI uset_contains_nid_postfix(const USet*, std::int32_t);
std::int32_t APS5_VABI uset_getItemCount_nid_postfix(const USet*);
std::int32_t APS5_VABI uset_getItem_nid_postfix(const USet*, std::int32_t, std::int32_t*, std::int32_t*, char16_t*, std::int32_t, std::int32_t*);
}
