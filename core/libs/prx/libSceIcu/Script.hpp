#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI uscript_getScript_nid_postfix(std::int32_t character, std::int32_t* error);
std::int8_t APS5_VABI uscript_hasScript_nid_postfix(std::int32_t character, std::int32_t script);
const char* APS5_VABI uscript_getShortName_nid_postfix(std::int32_t script);
const char* APS5_VABI uscript_getName_nid_postfix(std::int32_t script);
std::int32_t APS5_VABI uscript_getScriptExtensions_nid_postfix(std::int32_t character, std::int32_t* scripts,
                                                            std::int32_t capacity, std::int32_t* error);
std::int32_t APS5_VABI uscript_getUsage_nid_postfix(std::int32_t script);
std::int8_t APS5_VABI uscript_isRightToLeft_nid_postfix(std::int32_t script);
std::int8_t APS5_VABI uscript_breaksBetweenLetters_nid_postfix(std::int32_t script);
std::int8_t APS5_VABI uscript_isCased_nid_postfix(std::int32_t script);
std::int32_t APS5_VABI uscript_getSampleString_nid_postfix(std::int32_t script, char16_t* destination,
                                                        std::int32_t capacity, std::int32_t* error);
}
