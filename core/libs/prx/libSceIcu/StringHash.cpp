#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/urename.h"
#include "unicode/uvernum.h"
#include <cstdint>

extern "C" {

std::int32_t ustr_hashUCharsN(const std::uint16_t* s, std::int32_t length);
std::int32_t ustr_hashCharsN(const char* s, std::int32_t length);
std::int32_t ustr_hashICharsN(const char* s, std::int32_t length);

}

extern "C" {

std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const std::uint16_t* s, std::int32_t length) {
    return ustr_hashUCharsN(s, length);
}

std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* s, std::int32_t length) {
    return ustr_hashCharsN(s, length);
}

std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char* s, std::int32_t length) {
    return ustr_hashICharsN(s, length);
}

}
