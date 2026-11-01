#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UConstrainedFieldPosition;

extern "C" {
UConstrainedFieldPosition* APS5_VABI ucfpos_open_nid_postfix(std::int32_t*);
void APS5_VABI ucfpos_close_nid_postfix(UConstrainedFieldPosition*);
void APS5_VABI ucfpos_constrainCategory_nid_postfix(UConstrainedFieldPosition*, std::int32_t, std::int32_t*);
void APS5_VABI ucfpos_constrainField_nid_postfix(UConstrainedFieldPosition*, std::int32_t, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI ucfpos_getCategory_nid_postfix(const UConstrainedFieldPosition*, std::int32_t*);
std::int32_t APS5_VABI ucfpos_getField_nid_postfix(const UConstrainedFieldPosition*, std::int32_t*);
void APS5_VABI ucfpos_getIndexes_nid_postfix(const UConstrainedFieldPosition*, std::int32_t*, std::int32_t*, std::int32_t*);
}
