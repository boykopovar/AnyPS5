#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/urename.h"
#include "unicode/uvernum.h"
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {

char16_t u_asciiToUpper(char16_t c);
char uprv_toupper(char c);
char uprv_asciitolower(char c);
char uprv_ebcdictolower(char c);
int uprv_stricmp(const char* str1, const char* str2);
int uprv_strnicmp(const char* str1, const char* str2, std::uint32_t n);
std::int8_t uprv_isInvariantString(const char* s, std::int32_t length);
std::int8_t uprv_isInvariantUString(const char16_t* s, std::int32_t length);

}

extern "C" {

char16_t APS5_VABI u_asciiToUpper_nid_postfix(char16_t c) {
    return u_asciiToUpper(c);
}

char APS5_VABI uprv_toupper_nid_postfix(char c) {
    return uprv_toupper(c);
}

char APS5_VABI uprv_asciitolower_nid_postfix(char c) {
    return uprv_asciitolower(c);
}

char APS5_VABI uprv_ebcdictolower_nid_postfix(char c) {
    return uprv_ebcdictolower(c);
}

std::int32_t APS5_VABI uprv_stricmp_nid_postfix(const char* str1, const char* str2) {
    return uprv_stricmp(str1, str2);
}

std::int32_t APS5_VABI uprv_strnicmp_nid_postfix(const char* str1, const char* str2, std::uint32_t n) {
    return uprv_strnicmp(str1, str2, n);
}

std::int8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char* s, std::int32_t length) {
    if (!s && length != 0) APS5_INVALID_ARG_EX;
    return uprv_isInvariantString(s, length);
}

std::int8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t* s, std::int32_t length) {
    if (!s && length != 0) APS5_INVALID_ARG_EX;
    return uprv_isInvariantUString(s, length);
}

}
