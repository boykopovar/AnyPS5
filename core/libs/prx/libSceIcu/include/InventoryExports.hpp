#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI ubrk_close();
int APS5_VABI ubrk_countAvailable();
int APS5_VABI ubrk_current();
int APS5_VABI ubrk_first();
int APS5_VABI ubrk_following();
int APS5_VABI ubrk_getAvailable();
int APS5_VABI ubrk_getRuleStatus();
int APS5_VABI ubrk_isBoundary();
int APS5_VABI ubrk_last();
int APS5_VABI ubrk_next();
int APS5_VABI ubrk_open();
int APS5_VABI ubrk_preceding();
int APS5_VABI ubrk_previous();
int APS5_VABI ubrk_setText();
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
int APS5_VABI udata_setCommonData();
int APS5_VABI udata_setFileAccess();
int APS5_VABI uloc_addLikelySubtags();
std::int32_t APS5_VABI uloc_canonicalize(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
int APS5_VABI uloc_countAvailable();
std::int32_t APS5_VABI uloc_forLanguageTag(const char* langtag, char* localeID, std::int32_t capacity, std::int32_t* parsedLength, std::int32_t* status);
int APS5_VABI uloc_getAvailable();
std::int32_t APS5_VABI uloc_getBaseName(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
int APS5_VABI uloc_getCharacterOrientation();
std::int32_t APS5_VABI uloc_getCountry(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
int APS5_VABI uloc_getDefault();
std::int32_t APS5_VABI uloc_getKeywordValue(const char* locale, const char* keyword, char* result,
                                          std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getLanguage(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_getScript(const char* locale, char* result, std::int32_t capacity, std::int32_t* status);
int APS5_VABI uloc_minimizeSubtags();
int APS5_VABI uloc_setKeywordValue();
int APS5_VABI uloc_toLanguageTag();
int APS5_VABI uloc_toUnicodeLocaleType();
int APS5_VABI ures_getByKey();
int APS5_VABI ures_getStringByKey();
int APS5_VABI ures_open();
int APS5_VABI utext_openConstUnicodeString();
int APS5_VABI utext_openUChars();
int APS5_VABI utext_setup();

}
