#include "prx/libSceIcu/CodePoints.hpp"
#include "unicode/uchar.h"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI u_tolower_nid_postfix(std::int32_t character) {
    return u_tolower(character);
}

std::int32_t APS5_VABI u_toupper_nid_postfix(std::int32_t character) {
    return u_toupper(character);
}

std::int32_t APS5_VABI u_totitle_nid_postfix(std::int32_t character) {
    return u_totitle(character);
}

std::int32_t APS5_VABI ublock_getCode_nid_postfix(std::int32_t character) {
    return ublock_getCode(character);
}

}
