// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1999-2016, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Vector.hpp"

extern "C" {
std::int32_t APS5_VABI uhash_hashUnicodeString_nid_postfix(Icu::Abi::Element key);
std::int8_t APS5_VABI uhash_compareUnicodeString_nid_postfix(Icu::Abi::Element first, Icu::Abi::Element second);
}
