// Derived from icu4c/source/common/loclikely.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): ulocimp_getRegionForSupplementalData.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/loclikely.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/SupplementalRegion.hpp"
#include "prx/libSceIcu/TerminateBuffer.hpp"
#include "prx/libSceIcu/include/Availability.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

constexpr std::int32_t kRegionBufferLength = 8;
constexpr std::int32_t kBufferOverflow = 15;

bool Failure(std::int32_t status) {
    return status > 0;
}

char AsciiUpper(char character) {
    return character >= 'a' && character <= 'z' ? static_cast<char>(character - 'a' + 'A') : character;
}

std::string LikelySubtags(const char* localeID, std::int32_t* status) {
    std::int32_t preflight = 0;
    const std::int32_t length = uloc_addLikelySubtags_nid_postfix(localeID, nullptr, 0, &preflight);
    if (Failure(preflight) && preflight != kBufferOverflow) {
        *status = preflight;
        return {};
    }
    if (length < 0) NotImplemented_nid_no_patch(__func__);
    std::string result(static_cast<std::size_t>(length) + 1, '\0');
    uloc_addLikelySubtags_nid_postfix(localeID, result.data(), length + 1, status);
    result.resize(static_cast<std::size_t>(length));
    return result;
}

}

extern "C" {

std::int32_t APS5_VABI ulocimp_getRegionForSupplementalData_nid_postfix(const char* localeID, std::int8_t inferRegion,
                                                                        char* region, std::int32_t regionCapacity,
                                                                        std::int32_t* status) {
    std::int32_t incoming = 0;
    if (!status || !GuestCopyIn_nid_no_patch(&incoming, status, sizeof(incoming))) NotImplemented_nid_no_patch(__func__);
    if (Failure(incoming)) return 0;
    if (regionCapacity < 0 || !GuestWritable_nid_no_patch(region, static_cast<std::size_t>(regionCapacity)))
        NotImplemented_nid_no_patch(__func__);

    std::int32_t local = incoming;
    char buffer[kRegionBufferLength] = {};
    std::int32_t keywordStatus = 0;
    std::int32_t length = uloc_getKeywordValue(localeID, "rg", buffer, kRegionBufferLength, &keywordStatus);
    if (Failure(keywordStatus) || length != 6) {
        length = 0;
    } else {
        for (char* character = buffer; *character != 0; ++character) *character = AsciiUpper(*character);
        length = std::strcmp(buffer + 2, "ZZZZ") == 0 ? 2 : 0;
    }

    if (length == 0) {
        length = uloc_getCountry(localeID, buffer, kRegionBufferLength, &local);
        if (Failure(local)) {
            length = 0;
        } else if (length == 0 && inferRegion != 0) {
            std::int32_t likelyStatus = 0;
            const std::string likely = LikelySubtags(localeID, &likelyStatus);
            if (!Failure(likelyStatus)) {
                length = uloc_getCountry(likely.c_str(), buffer, kRegionBufferLength, &local);
                if (Failure(local)) length = 0;
            }
        }
    }
    if (length < 0 || length >= kRegionBufferLength) NotImplemented_nid_no_patch(__func__);

    buffer[length] = 0;
    if (regionCapacity > 0) std::strncpy(region, buffer, static_cast<std::size_t>(regionCapacity));
    const std::int32_t result = u_terminateChars_nid_postfix(region, regionCapacity, length, &local);
    if (local != incoming && !GuestCopyOut_nid_no_patch(status, &local, sizeof(local)))
        NotImplemented_nid_no_patch(__func__);
    return result;
}

}
