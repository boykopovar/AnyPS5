// Adapted from uprv_isASCIILetter in ICU 68.2 common/cstring.cpp:
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cstring.cpp
//
// ICU's notice, kept as the upstream file has it:
//   Copyright (C) 1997-2011, International Business Machines
//   Corporation and others.  All Rights Reserved.
// ICU and its data are distributed under the Unicode license:
//   https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/LICENSE
// The upstream file is C; this file keeps the same ASCII behaviour in C++.
// The console's ICU build is not measured, so the ASCII family is assumed.

#include "prx/libSceIcu/include/LegacyExports.hpp"

extern "C" {

std::int8_t APS5_VABI uprv_isASCIILetter_nid_postfix(char c) {
    return static_cast<std::int8_t>(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
}

}
