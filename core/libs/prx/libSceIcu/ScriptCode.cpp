// Derived from icu4c/source/common/uscript.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uscript.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/ScriptCode.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uchar.h"
#include "unicode/uloc.h"
#include "unicode/uscript.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace {

constexpr std::int32_t Icu68ScriptCodeLimit = 193;
constexpr std::int32_t SubtagCapacity = 8;

struct ScriptCodes {
    std::array<std::int32_t, 3> codes{};
    std::int32_t length = 0;
};

ScriptCodes Codes(std::initializer_list<UScriptCode> scripts) {
    ScriptCodes result;
    for (const auto script : scripts) result.codes[result.length++] = script;
    return result;
}

UScriptCode Icu68ScriptForName(const char* name) {
    const auto script = u_getPropertyValueEnum(UCHAR_SCRIPT, name);
    return script >= 0 && script < Icu68ScriptCodeLimit ? static_cast<UScriptCode>(script) : USCRIPT_INVALID_CODE;
}

ScriptCodes CodesFromLocale(const char* locale) {
    UErrorCode status = U_ZERO_ERROR;
    std::array<char, SubtagCapacity> language{};
    uloc_getLanguage(locale, language.data(), SubtagCapacity, &status);
    if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) return {};
    if (std::strcmp(language.data(), "ja") == 0) return Codes({USCRIPT_KATAKANA, USCRIPT_HIRAGANA, USCRIPT_HAN});
    if (std::strcmp(language.data(), "ko") == 0) return Codes({USCRIPT_HANGUL, USCRIPT_HAN});
    std::array<char, SubtagCapacity> script{};
    const auto scriptLength = uloc_getScript(locale, script.data(), SubtagCapacity, &status);
    if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) return {};
    if (std::strcmp(language.data(), "zh") == 0 && std::strcmp(script.data(), "Hant") == 0) {
        return Codes({USCRIPT_HAN, USCRIPT_BOPOMOFO});
    }
    if (scriptLength == 0) return {};
    const auto code = Icu68ScriptForName(script.data());
    if (code == USCRIPT_INVALID_CODE) return {};
    if (code == USCRIPT_SIMPLIFIED_HAN || code == USCRIPT_TRADITIONAL_HAN) return Codes({USCRIPT_HAN});
    return Codes({code});
}

}

extern "C" {

std::int32_t APS5_VABI uscript_getCode_nid_postfix(const char* nameOrAbbrOrLocale, std::int32_t* fillIn, std::int32_t capacity,
                                                   std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    std::int32_t previous;
    if (!GuestCopyIn_nid_no_patch(&previous, error, sizeof(previous))) NotImplemented_nid_no_patch(__func__);
    if (previous > U_ZERO_ERROR) return 0;
    UErrorCode status = U_ZERO_ERROR;
    ScriptCodes found;
    if (!nameOrAbbrOrLocale || (fillIn ? capacity < 0 : capacity != 0)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
    } else {
        if (!std::strchr(nameOrAbbrOrLocale, '-') && !std::strchr(nameOrAbbrOrLocale, '_')) {
            const auto code = Icu68ScriptForName(nameOrAbbrOrLocale);
            if (code != USCRIPT_INVALID_CODE) found = Codes({code});
        }
        if (found.length == 0) found = CodesFromLocale(nameOrAbbrOrLocale);
        if (found.length == 0) NotImplemented_nid_no_patch(__func__);
        if (found.length > capacity) {
            status = U_BUFFER_OVERFLOW_ERROR;
        } else if (!GuestCopyOut_nid_no_patch(fillIn, found.codes.data(), found.length * sizeof(found.codes[0]))) {
            NotImplemented_nid_no_patch(__func__);
        }
    }
    if (status != U_ZERO_ERROR) {
        const std::int32_t updated = status;
        if (!GuestCopyOut_nid_no_patch(error, &updated, sizeof(updated))) NotImplemented_nid_no_patch(__func__);
    }
    return status == U_ILLEGAL_ARGUMENT_ERROR ? 0 : found.length;
}

}
