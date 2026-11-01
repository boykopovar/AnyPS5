// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
const char* const* APS5_VABI uloc_getISOLanguages_nid_postfix();
const char* const* APS5_VABI uloc_getISOCountries_nid_postfix();
const char* APS5_VABI uloc_getISO3Language_nid_postfix(const char*);
const char* APS5_VABI uloc_getISO3Country_nid_postfix(const char*);
std::int32_t APS5_VABI uloc_getParent_nid_postfix(const char*, char*, std::int32_t, std::int32_t*);
}
