#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>

extern "C" {

std::int8_t APS5_VABI u_isISOControl_nid_postfix(std::int32_t character) {
    return static_cast<std::uint32_t>(character) <= 0x9f && (character <= 0x1f || character >= 0x7f);
}

std::int32_t APS5_VABI u_forDigit_nid_postfix(std::int32_t digit, std::int8_t radix) {
    if (radix < 2 || radix > 36 || digit < 0 || digit >= radix) return 0;
    return digit < 10 ? '0' + digit : 'a' - 10 + digit;
}

}
