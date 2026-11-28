// Derived from icu4c/source/common/cstring.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/cstring.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2011, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/include/LegacyExports.hpp"

extern "C" {

std::int8_t APS5_VABI uprv_isASCIILetter_nid_postfix(char c) {
    return static_cast<std::int8_t>(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
}

}
