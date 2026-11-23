#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
using CaseContextIterator = std::int32_t (APS5_VABI*)(void*, std::int8_t);
} // namespace Icu

extern "C" {
std::int32_t APS5_VABI ucase_getType_nid_postfix(std::int32_t);
std::int32_t APS5_VABI ucase_getTypeOrIgnorable_nid_postfix(std::int32_t);
std::int8_t APS5_VABI ucase_isSoftDotted_nid_postfix(std::int32_t);
std::int8_t APS5_VABI ucase_isCaseSensitive_nid_postfix(std::int32_t);
std::int32_t APS5_VABI ucase_tolower_nid_postfix(std::int32_t);
std::int32_t APS5_VABI ucase_toupper_nid_postfix(std::int32_t);
std::int32_t APS5_VABI ucase_totitle_nid_postfix(std::int32_t);
std::int32_t APS5_VABI ucase_fold_nid_postfix(std::int32_t, std::uint32_t);
std::int32_t APS5_VABI ucase_toFullLower_nid_postfix(std::int32_t, Icu::CaseContextIterator, void*, const char16_t**, std::int32_t);
std::int32_t APS5_VABI ucase_toFullUpper_nid_postfix(std::int32_t, Icu::CaseContextIterator, void*, const char16_t**, std::int32_t);
std::int32_t APS5_VABI ucase_toFullTitle_nid_postfix(std::int32_t, Icu::CaseContextIterator, void*, const char16_t**, std::int32_t);
std::int32_t APS5_VABI ucase_toFullFolding_nid_postfix(std::int32_t, const char16_t**, std::uint32_t);
}
