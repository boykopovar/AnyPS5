#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {
using UnescapeCharacterAt = char16_t (APS5_VABI*)(std::int32_t, void*);
} // namespace Icu

extern "C" {
std::int32_t APS5_VABI u_unescape_nid_postfix(const char*, char16_t*, std::int32_t);
std::int32_t APS5_VABI u_unescapeAt_nid_postfix(Icu::UnescapeCharacterAt, std::int32_t*, std::int32_t, void*);
}
