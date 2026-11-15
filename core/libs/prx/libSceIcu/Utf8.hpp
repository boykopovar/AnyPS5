#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI utf8_nextCharSafeBody_nid_postfix(const std::uint8_t* text, std::int32_t* index, std::int32_t length, std::int32_t lead, std::int8_t strict);
std::int32_t APS5_VABI utf8_prevCharSafeBody_nid_postfix(const std::uint8_t* text, std::int32_t start, std::int32_t* index, std::int32_t trail, std::int8_t strict);
std::int32_t APS5_VABI utf8_appendCharSafeBody_nid_postfix(std::uint8_t* text, std::int32_t index, std::int32_t capacity, std::int32_t value, std::int8_t* isError);
std::int32_t APS5_VABI utf8_back1SafeBody_nid_postfix(const std::uint8_t* text, std::int32_t start, std::int32_t index);
}
