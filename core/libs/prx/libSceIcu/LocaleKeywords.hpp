// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "StringsEnumeration.hpp"

extern "C" {
Icu::UEnumeration* APS5_VABI uloc_openKeywordList_nid_postfix(const char* keywords, std::int32_t size, std::int32_t* status);
}
