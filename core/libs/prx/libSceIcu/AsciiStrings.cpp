// Adapted from uprv_isASCIILetter in ICU 68.2 common/cstring.cpp:
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cstring.cpp
//
// ICU's notice, kept as the upstream file has it:
//   Copyright (C) 1997-2011, International Business Machines
//   Corporation and others.  All Rights Reserved.
// ICU and its data are distributed under the Unicode license:
//   https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/LICENSE

#include "prx/libSceIcu/include/LegacyExports.hpp"

extern "C" {

std::int8_t APS5_VABI uprv_isASCIILetter_nid_postfix(char c) {
    return static_cast<std::int8_t>(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
}

char APS5_VABI uprv_toupper_nid_postfix(char c) {
    if ('a' <= c && c <= 'z') c = static_cast<char>(c + ('A' - 'a'));
    return c;
}

char APS5_VABI uprv_asciitolower_nid_postfix(char c) {
    if (0x41 <= c && c <= 0x5a) c = static_cast<char>(c + 0x20);
    return c;
}

int APS5_VABI uprv_stricmp_nid_postfix(const char* str1, const char* str2) {
    if (str1 == nullptr) return str2 == nullptr ? 0 : -1;
    if (str2 == nullptr) return 1;
    for (;;) {
        const auto c1 = static_cast<unsigned char>(*str1);
        const auto c2 = static_cast<unsigned char>(*str2);
        if (c1 == 0) return c2 == 0 ? 0 : -1;
        if (c2 == 0) return 1;
        const int rc = static_cast<int>(static_cast<unsigned char>(uprv_asciitolower_nid_postfix(static_cast<char>(c1))))
                     - static_cast<int>(static_cast<unsigned char>(uprv_asciitolower_nid_postfix(static_cast<char>(c2))));
        if (rc != 0) return rc;
        ++str1;
        ++str2;
    }
}

int APS5_VABI uprv_strnicmp_nid_postfix(const char* str1, const char* str2, std::uint32_t n) {
    if (str1 == nullptr) return str2 == nullptr ? 0 : -1;
    if (str2 == nullptr) return 1;
    while (n-- > 0) {
        const auto c1 = static_cast<unsigned char>(*str1);
        const auto c2 = static_cast<unsigned char>(*str2);
        if (c1 == 0) return c2 == 0 ? 0 : -1;
        if (c2 == 0) return 1;
        const int rc = static_cast<int>(static_cast<unsigned char>(uprv_asciitolower_nid_postfix(static_cast<char>(c1))))
                     - static_cast<int>(static_cast<unsigned char>(uprv_asciitolower_nid_postfix(static_cast<char>(c2))));
        if (rc != 0) return rc;
        ++str1;
        ++str2;
    }
    return 0;
}

}
