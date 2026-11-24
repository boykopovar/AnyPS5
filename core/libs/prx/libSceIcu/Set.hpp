#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct USet;

namespace Icu {
const USet* NativeSet(const USet* set);
}

extern "C" {
const USet* APS5_VABI u_getBinaryPropertySet_nid_postfix(std::int32_t, std::int32_t*);
USet* APS5_VABI uset_openEmpty_nid_postfix();
void APS5_VABI uset_close_nid_postfix(USet*);
void APS5_VABI uset_add_nid_postfix(USet*, std::int32_t);
void APS5_VABI uset_addRange_nid_postfix(USet*, std::int32_t, std::int32_t);
void APS5_VABI uset_addAll_nid_postfix(USet*, const USet*);
void APS5_VABI uset_clear_nid_postfix(USet*);
void APS5_VABI uset_compact_nid_postfix(USet*);
void APS5_VABI uset_complement_nid_postfix(USet*);
void APS5_VABI uset_complementRange_nid_postfix(USet*, std::int32_t, std::int32_t);
std::int8_t APS5_VABI uset_contains_nid_postfix(const USet*, std::int32_t);
std::int8_t APS5_VABI uset_containsRange_nid_postfix(const USet*, std::int32_t, std::int32_t);
std::int8_t APS5_VABI uset_equals_nid_postfix(const USet*, const USet*);
std::int32_t APS5_VABI uset_indexOf_nid_postfix(const USet*, std::int32_t);
std::int32_t APS5_VABI uset_charAt_nid_postfix(const USet*, std::int32_t);
std::int32_t APS5_VABI uset_getStringCount_nid_postfix(const USet*);
std::int8_t APS5_VABI uset_contains_nid_postfix(const USet*, std::int32_t);
std::int8_t APS5_VABI uset_containsAll_nid_postfix(const USet*, const USet*);
std::int8_t APS5_VABI uset_containsNone_nid_postfix(const USet*, const USet*);
std::int8_t APS5_VABI uset_containsSome_nid_postfix(const USet*, const USet*);
std::int8_t APS5_VABI uset_hasStrings_nid_postfix(const USet*);
std::int8_t APS5_VABI uset_isFrozen_nid_postfix(const USet*);
std::int32_t APS5_VABI uset_span_nid_postfix(const USet*, const char16_t*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI uset_spanBack_nid_postfix(const USet*, const char16_t*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI uset_spanUTF8_nid_postfix(const USet*, const char*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI uset_getItemCount_nid_postfix(const USet*);
std::int32_t APS5_VABI uset_getItem_nid_postfix(const USet*, std::int32_t, std::int32_t*, std::int32_t*, char16_t*, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI uset_isEmpty_nid_postfix(const USet*);
void APS5_VABI uset_remove_nid_postfix(USet*, std::int32_t);
void APS5_VABI uset_removeRange_nid_postfix(USet*, std::int32_t, std::int32_t);
void APS5_VABI uset_retain_nid_postfix(USet*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI uset_size_nid_postfix(const USet*);
}
