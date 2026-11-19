#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
int APS5_VABI u_strToTitle_nid_postfix();
int APS5_VABI ucol_countAvailable_nid_postfix();
int APS5_VABI ucol_getAvailable_nid_postfix();
int APS5_VABI ucol_getContractionsAndExpansions_nid_postfix();
int APS5_VABI ucol_getKeywordValues_nid_postfix();
int APS5_VABI ucol_getKeywordValuesForLocale_nid_postfix();
int APS5_VABI ucol_getRules_nid_postfix();
int APS5_VABI ucol_getStrength_nid_postfix();
int APS5_VABI ucol_setStrength_nid_postfix();
int APS5_VABI ucol_strcoll_nid_postfix();
int APS5_VABI ucol_strcollIter_nid_postfix();
int APS5_VABI ucol_strcollUTF8_nid_postfix();
int APS5_VABI unum_formatDoubleForFields_nid_postfix();
int APS5_VABI unum_getTextAttribute_nid_postfix();
int APS5_VABI unum_setAttribute_nid_postfix();
int APS5_VABI unum_setTextAttribute_nid_postfix();
int APS5_VABI unumf_formatDecimal_nid_postfix();
int APS5_VABI unumf_formatDouble_nid_postfix();
int APS5_VABI unumf_openForSkeletonAndLocale_nid_postfix();
int APS5_VABI unumf_resultGetAllFieldPositions_nid_postfix();
int APS5_VABI unumf_resultToString_nid_postfix();
int APS5_VABI unumsys_getName_nid_postfix();
int APS5_VABI unumsys_isAlgorithmic_nid_postfix();
int APS5_VABI unumsys_openAvailableNames_nid_postfix();
}
