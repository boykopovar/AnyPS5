#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI u_tolower();
int APS5_VABI u_totitle();
int APS5_VABI u_toupper();
int APS5_VABI ublock_getCode();
int APS5_VABI ubrk_setUText();
int APS5_VABI ucnv_cbFromUWriteBytes();
int APS5_VABI ucnv_close();
int APS5_VABI ucnv_convertEx();
int APS5_VABI ucnv_fromUnicode();
int APS5_VABI ucnv_getCanonicalName();
int APS5_VABI ucnv_getName();
int APS5_VABI ucnv_open();
int APS5_VABI ucnv_reset();
int APS5_VABI ucnv_setFallback();
int APS5_VABI ucnv_setFromUCallBack();
int APS5_VABI ucnv_setSubstChars();
int APS5_VABI ucnv_setToUCallBack();
int APS5_VABI ucnv_toUChars();
int APS5_VABI ucnv_toUnicode();
int APS5_VABI ucurr_getName();
int APS5_VABI ucurr_openISOCurrencies();
int APS5_VABI uloc_addLikelySubtags();
int APS5_VABI uloc_canonicalize();
int APS5_VABI uloc_countAvailable();
int APS5_VABI uloc_forLanguageTag();
int APS5_VABI uloc_getAvailable();
int APS5_VABI uloc_getBaseName();
int APS5_VABI uloc_getCharacterOrientation();
int APS5_VABI uloc_getCountry();
int APS5_VABI uloc_getDefault();
int APS5_VABI uloc_getKeywordValue();
int APS5_VABI uloc_getLanguage();
int APS5_VABI uloc_getScript();
int APS5_VABI uloc_minimizeSubtags();
int APS5_VABI uloc_setKeywordValue();
int APS5_VABI uloc_toLanguageTag();
int APS5_VABI uloc_toUnicodeLocaleType();
int APS5_VABI unorm2_composePair();
int APS5_VABI unorm2_getNFCInstance();
int APS5_VABI unorm2_getNFDInstance();
int APS5_VABI unorm2_getNFKCInstance();
int APS5_VABI unorm2_getNFKDInstance();
int APS5_VABI unorm2_getRawDecomposition();
int APS5_VABI unorm2_isNormalized();
int APS5_VABI unorm2_normalize();
int APS5_VABI ures_close();
int APS5_VABI ures_getByKey();
int APS5_VABI ures_getStringByKey();
int APS5_VABI ures_open();
int APS5_VABI uscript_getScript();
int APS5_VABI uscript_getShortName();
int APS5_VABI uscript_hasScript();
int APS5_VABI utext_close();
int APS5_VABI utext_openConstUnicodeString();
int APS5_VABI utext_openUChars();
int APS5_VABI utext_setup();

}
