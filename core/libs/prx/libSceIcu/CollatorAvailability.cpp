// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ******************************************************************************
 * Copyright (C) 1996-2014, International Business Machines Corporation and
 * others. All Rights Reserved.
 ******************************************************************************
 */
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/coll.cpp
/*
*******************************************************************************
*   Copyright (C) 1996-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*/
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/ucol_res.cpp

#include "prx/libSceIcu/include/CollatorAvailability.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/locid.h"
#include "unicode/ures.h"
#include "uresimp.h"
#include <cstdint>
#include <memory>

namespace {

constexpr auto& IcuGetByKey = ures_getByKey;

struct AvailableLocales {
    std::unique_ptr<icu::Locale[]> List;
    std::int32_t Count = 0;
};

AvailableLocales LoadAvailableLocales() {
    AvailableLocales available;
    UErrorCode status = U_ZERO_ERROR;
    const icu::LocalUResourceBundlePointer index(ures_openDirect(U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "coll", "res_index", &status));
    icu::StackUResourceBundle installed;
    IcuGetByKey(index.getAlias(), "InstalledLocales", installed.getAlias(), &status);
    if (U_SUCCESS(status)) {
        available.Count = ures_getSize(installed.getAlias());
        available.List.reset(new icu::Locale[available.Count]);
        std::int32_t position = 0;
        ures_resetIterator(installed.getAlias());
        while (ures_hasNext(installed.getAlias())) {
            const char* key = nullptr;
            ures_getNextString(installed.getAlias(), nullptr, &key, &status);
            available.List[position++] = icu::Locale(key);
        }
    }
    return available;
}

const AvailableLocales& Available() {
    static const AvailableLocales available = LoadAvailableLocales();
    return available;
}

}

extern "C" {

std::int32_t APS5_VABI ucol_countAvailable_nid_postfix() {
    return Available().Count;
}

const char* APS5_VABI ucol_getAvailable_nid_postfix(std::int32_t localeIndex) {
    const auto& available = Available();
    if (!available.List || localeIndex >= available.Count) return nullptr;
    if (localeIndex < 0) NotImplemented_nid_no_patch(__func__);
    return available.List[localeIndex].getName();
}

}
