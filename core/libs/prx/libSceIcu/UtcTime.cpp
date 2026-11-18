// Derived from icu4c/source/common/putil.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): uprv_getUTCtime and uprv_getRawUTCtime.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <cstdint>

namespace {

constexpr std::int64_t MillisPerSecond = 1000;
constexpr std::int64_t MicrosPerMilli = 1000;

}

extern "C" {

double APS5_VABI uprv_getRawUTCtime_nid_postfix() {
    KernelTimeval now{};
    if (gettimeofday_nid_postfix(&now, nullptr) != 0) NotImplemented_nid_no_patch(__func__);
    return static_cast<double>(now.tv_sec * MillisPerSecond + now.tv_usec / MicrosPerMilli);
}

double APS5_VABI uprv_getUTCtime_nid_postfix() {
    return uprv_getRawUTCtime_nid_postfix();
}

}
