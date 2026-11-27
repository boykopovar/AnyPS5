#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
int APS5_VABI u_strToTitle_nid_postfix();
int APS5_VABI ucol_getStrength_nid_postfix();
int APS5_VABI ucol_setStrength_nid_postfix();
int APS5_VABI ucol_strcoll_nid_postfix();
int APS5_VABI ucol_strcollUTF8_nid_postfix();
int APS5_VABI unum_getTextAttribute_nid_postfix();
int APS5_VABI unum_setAttribute_nid_postfix();
int APS5_VABI unum_setTextAttribute_nid_postfix();
}
