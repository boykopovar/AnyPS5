#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct USet;
struct USerializedSet;

namespace Icu {
const USet* NativeSet(const USet* set);
}

extern "C" {
const USet* APS5_VABI u_getBinaryPropertySet_nid_postfix(std::int32_t, std::int32_t*);
USet* APS5_VABI uset_openEmpty_nid_postfix();
USet* APS5_VABI uset_open_nid_postfix(std::int32_t, std::int32_t);
USet* APS5_VABI uset_clone_nid_postfix(const USet*);
USet* APS5_VABI uset_cloneAsThawed_nid_postfix(const USet*);
void APS5_VABI uset_close_nid_postfix(USet*);
void APS5_VABI uset_freeze_nid_postfix(USet*);
void APS5_VABI uset_add_nid_postfix(USet*, std::int32_t);
void APS5_VABI uset_addRange_nid_postfix(USet*, std::int32_t, std::int32_t);
void APS5_VABI uset_addAll_nid_postfix(USet*, const USet*);
void APS5_VABI uset_addString_nid_postfix(USet*, const char16_t*, std::int32_t);
void APS5_VABI uset_addAllCodePoints_nid_postfix(USet*, const char16_t*, std::int32_t);
void APS5_VABI uset_applyIntPropertyValue_nid_postfix(USet*, std::int32_t, std::int32_t, std::int32_t*);
void APS5_VABI uset_applyPropertyAlias_nid_postfix(USet*, const char16_t*, std::int32_t, const char16_t*, std::int32_t, std::int32_t*);
void APS5_VABI uset_clear_nid_postfix(USet*);
void APS5_VABI uset_compact_nid_postfix(USet*);
void APS5_VABI uset_complement_nid_postfix(USet*);
void APS5_VABI uset_complementRange_nid_postfix(USet*, std::int32_t, std::int32_t);
void APS5_VABI uset_complementAllCodePoints_nid_postfix(USet*, const char16_t*, std::int32_t);
void APS5_VABI uset_complementString_nid_postfix(USet*, const char16_t*, std::int32_t);
std::int8_t APS5_VABI uset_contains_nid_postfix(const USet*, std::int32_t);
std::int8_t APS5_VABI uset_containsRange_nid_postfix(const USet*, std::int32_t, std::int32_t);
std::int8_t APS5_VABI uset_containsAllCodePoints_nid_postfix(const USet*, const char16_t*, std::int32_t);
std::int8_t APS5_VABI uset_equals_nid_postfix(const USet*, const USet*);
std::int32_t APS5_VABI uset_indexOf_nid_postfix(const USet*, std::int32_t);
std::int32_t APS5_VABI uset_charAt_nid_postfix(const USet*, std::int32_t);
std::int32_t APS5_VABI uset_getStringCount_nid_postfix(const USet*);
std::int32_t APS5_VABI uset_getRangeCount_nid_postfix(const USet*);
const char16_t* APS5_VABI uset_getString_nid_postfix(const USet*, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI uset_contains_nid_postfix(const USet*, std::int32_t);
std::int8_t APS5_VABI uset_containsAll_nid_postfix(const USet*, const USet*);
std::int8_t APS5_VABI uset_containsNone_nid_postfix(const USet*, const USet*);
std::int8_t APS5_VABI uset_containsSome_nid_postfix(const USet*, const USet*);
std::int8_t APS5_VABI uset_containsString_nid_postfix(const USet*, const char16_t*, std::int32_t);
std::int8_t APS5_VABI uset_hasStrings_nid_postfix(const USet*);
std::int8_t APS5_VABI uset_isFrozen_nid_postfix(const USet*);
std::int32_t APS5_VABI uset_span_nid_postfix(const USet*, const char16_t*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI uset_spanBack_nid_postfix(const USet*, const char16_t*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI uset_spanUTF8_nid_postfix(const USet*, const char*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI uset_spanBackUTF8_nid_postfix(const USet*, const char*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI uset_toPattern_nid_postfix(const USet*, char16_t*, std::int32_t, std::int8_t, std::int32_t*);
std::int32_t APS5_VABI uset_serialize_nid_postfix(const USet*, std::uint16_t*, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI uset_getSerializedSet_nid_postfix(USerializedSet*, const std::uint16_t*, std::int32_t);
void APS5_VABI uset_setSerializedToOne_nid_postfix(USerializedSet*, std::int32_t);
std::int8_t APS5_VABI uset_serializedContains_nid_postfix(const USerializedSet*, std::int32_t);
std::int32_t APS5_VABI uset_getSerializedRangeCount_nid_postfix(const USerializedSet*);
std::int8_t APS5_VABI uset_getSerializedRange_nid_postfix(const USerializedSet*, std::int32_t, std::int32_t*, std::int32_t*);
std::int32_t APS5_VABI uset_getItemCount_nid_postfix(const USet*);
std::int32_t APS5_VABI uset_getItem_nid_postfix(const USet*, std::int32_t, std::int32_t*, std::int32_t*, char16_t*, std::int32_t, std::int32_t*);
std::int8_t APS5_VABI uset_isEmpty_nid_postfix(const USet*);
void APS5_VABI uset_remove_nid_postfix(USet*, std::int32_t);
void APS5_VABI uset_removeRange_nid_postfix(USet*, std::int32_t, std::int32_t);
void APS5_VABI uset_removeString_nid_postfix(USet*, const char16_t*, std::int32_t);
void APS5_VABI uset_removeAllCodePoints_nid_postfix(USet*, const char16_t*, std::int32_t);
void APS5_VABI uset_removeAll_nid_postfix(USet*, const USet*);
void APS5_VABI uset_removeAllStrings_nid_postfix(USet*);
void APS5_VABI uset_retain_nid_postfix(USet*, std::int32_t, std::int32_t);
void APS5_VABI uset_retainAllCodePoints_nid_postfix(USet*, const char16_t*, std::int32_t);
void APS5_VABI uset_retainString_nid_postfix(USet*, const char16_t*, std::int32_t);
void APS5_VABI uset_retainAll_nid_postfix(USet*, const USet*);
void APS5_VABI uset_complementAll_nid_postfix(USet*, const USet*);
std::int32_t APS5_VABI uset_size_nid_postfix(const USet*);
std::int8_t APS5_VABI uset_resemblesPattern_nid_postfix(const char16_t*, std::int32_t, std::int32_t);
void APS5_VABI uset_set_nid_postfix(USet*, std::int32_t, std::int32_t);
std::int8_t APS5_VABI u_stringHasBinaryProperty_nid_postfix(const char16_t*, std::int32_t, std::int32_t);
}
