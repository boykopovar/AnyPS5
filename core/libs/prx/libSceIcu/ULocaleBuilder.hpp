#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct ULocaleBuilder;

extern "C" {
ULocaleBuilder* APS5_VABI ulocbld_open_nid_postfix();
void APS5_VABI ulocbld_close_nid_postfix(ULocaleBuilder* builder);
void APS5_VABI ulocbld_clear_nid_postfix(ULocaleBuilder* builder);
void APS5_VABI ulocbld_clearExtensions_nid_postfix(ULocaleBuilder* builder);
void APS5_VABI ulocbld_setLanguage_nid_postfix(ULocaleBuilder* builder, const char* language,
                                                std::int32_t length);
void APS5_VABI ulocbld_setRegion_nid_postfix(ULocaleBuilder* builder, const char* region, std::int32_t length);
void APS5_VABI ulocbld_setScript_nid_postfix(ULocaleBuilder* builder, const char* script, std::int32_t length);
void APS5_VABI ulocbld_setVariant_nid_postfix(ULocaleBuilder* builder, const char* variant,
                                               std::int32_t length);
void APS5_VABI ulocbld_setLanguageTag_nid_postfix(ULocaleBuilder* builder, const char* tag,
                                                   std::int32_t length);
void APS5_VABI ulocbld_setLocale_nid_postfix(ULocaleBuilder* builder, const char* locale, std::int32_t length);
std::int32_t APS5_VABI ulocbld_buildLocaleID_nid_postfix(ULocaleBuilder* builder, char* locale,
                                                          std::int32_t capacity, std::int32_t* error);
std::int32_t APS5_VABI ulocbld_buildLanguageTag_nid_postfix(ULocaleBuilder* builder, char* language,
                                                            std::int32_t capacity, std::int32_t* error);
}
