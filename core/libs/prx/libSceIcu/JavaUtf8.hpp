#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
char16_t* APS5_VABI u_strFromJavaModifiedUTF8WithSub_nid_postfix(char16_t*, std::int32_t, std::int32_t*, const char*, std::int32_t, std::int32_t, std::int32_t*, std::int32_t*);
char* APS5_VABI u_strToJavaModifiedUTF8_nid_postfix(char*, std::int32_t, std::int32_t*, const char16_t*, std::int32_t, std::int32_t*);
}
