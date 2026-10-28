#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
char16_t* APS5_VABI u_strFromUTF32_nid_postfix(char16_t*, std::int32_t, std::int32_t*, const std::int32_t*, std::int32_t, std::int32_t*);
char16_t* APS5_VABI u_strFromUTF32WithSub_nid_postfix(char16_t*, std::int32_t, std::int32_t*, const std::int32_t*, std::int32_t, std::int32_t, std::int32_t*, std::int32_t*);
}
