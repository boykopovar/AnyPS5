#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
void* APS5_VABI uprv_dl_open_nid_postfix(const char* library, std::int32_t* status);
void* APS5_VABI uprv_dlsym_func_nid_postfix(void* library, const char* symbol, std::int32_t* status);
void APS5_VABI uprv_dl_close_nid_postfix(void* library, std::int32_t* status);
std::int32_t APS5_VABI uprv_isASCIILetter_nid_postfix(char value);
}
