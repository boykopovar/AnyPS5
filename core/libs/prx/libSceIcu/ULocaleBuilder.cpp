#include "prx/libSceIcu/ULocaleBuilder.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ulocbuilder.h"

namespace {

UErrorCode& Status(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<UErrorCode*>(error);
}

}

extern "C" {

ULocaleBuilder* APS5_VABI ulocbld_open_nid_postfix() {
    return ulocbld_open();
}

void APS5_VABI ulocbld_close_nid_postfix(ULocaleBuilder* builder) {
    ulocbld_close(builder);
}

void APS5_VABI ulocbld_clear_nid_postfix(ULocaleBuilder* builder) {
    ulocbld_clear(builder);
}

void APS5_VABI ulocbld_clearExtensions_nid_postfix(ULocaleBuilder* builder) {
    ulocbld_clearExtensions(builder);
}

void APS5_VABI ulocbld_setLanguage_nid_postfix(ULocaleBuilder* builder, const char* language,
                                                std::int32_t length) {
    ulocbld_setLanguage(builder, language, length);
}

void APS5_VABI ulocbld_setRegion_nid_postfix(ULocaleBuilder* builder, const char* region, std::int32_t length) {
    ulocbld_setRegion(builder, region, length);
}

void APS5_VABI ulocbld_setScript_nid_postfix(ULocaleBuilder* builder, const char* script, std::int32_t length) {
    ulocbld_setScript(builder, script, length);
}

void APS5_VABI ulocbld_setVariant_nid_postfix(ULocaleBuilder* builder, const char* variant,
                                               std::int32_t length) {
    ulocbld_setVariant(builder, variant, length);
}

void APS5_VABI ulocbld_setLanguageTag_nid_postfix(ULocaleBuilder* builder, const char* tag,
                                                   std::int32_t length) {
    ulocbld_setLanguageTag(builder, tag, length);
}

void APS5_VABI ulocbld_setLocale_nid_postfix(ULocaleBuilder* builder, const char* locale, std::int32_t length) {
    ulocbld_setLocale(builder, locale, length);
}

std::int32_t APS5_VABI ulocbld_buildLocaleID_nid_postfix(ULocaleBuilder* builder, char* locale,
                                                          std::int32_t capacity, std::int32_t* error) {
    return ulocbld_buildLocaleID(builder, locale, capacity, &Status(error));
}

std::int32_t APS5_VABI ulocbld_buildLanguageTag_nid_postfix(ULocaleBuilder* builder, char* language,
                                                            std::int32_t capacity, std::int32_t* error) {
    return ulocbld_buildLanguageTag(builder, language, capacity, &Status(error));
}

}
