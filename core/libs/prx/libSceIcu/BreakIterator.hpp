#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
void* APS5_VABI ubrk_open(std::int32_t, const char*, const char16_t*, std::int32_t, std::int32_t*);
void APS5_VABI ubrk_close(void*);
void APS5_VABI ubrk_setText(void*, const char16_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI ubrk_first(void*);
std::int32_t APS5_VABI ubrk_last(void*);
std::int32_t APS5_VABI ubrk_next(void*);
std::int32_t APS5_VABI ubrk_previous(void*);
std::int32_t APS5_VABI ubrk_current(const void*);
std::int32_t APS5_VABI ubrk_following(void*, std::int32_t);
std::int32_t APS5_VABI ubrk_preceding(void*, std::int32_t);
std::int8_t APS5_VABI ubrk_isBoundary(void*, std::int32_t);
std::int32_t APS5_VABI ubrk_getRuleStatus(void*);
std::int32_t APS5_VABI ubrk_countAvailable();
const char* APS5_VABI ubrk_getAvailable(std::int32_t);
}
