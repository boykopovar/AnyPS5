// Derived from icu4c/source/common/putil.cpp, locid.cpp and uloc.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/locid.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uloc.cpp
//
// The upstream files' notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include "prx/libSceIcu/include/LegacyExports.hpp"
#include <cerrno>
#include <limits>

namespace {

constexpr int MessagesCategory = 6;
constexpr std::size_t LanguageCapacity = 12;

bool IsPosixDefault(const char* id) {
    return !id || std::strcmp(id, "C") == 0 || std::strcmp(id, "POSIX") == 0;
}

const char* PosixId() {
    const char* id = setlocale_nid_postfix(MessagesCategory, nullptr);
    if (IsPosixDefault(id)) {
        id = getenv_nid_postfix("LC_ALL");
        if (!id) {
            id = getenv_nid_postfix("LC_MESSAGES");
            if (!id) id = getenv_nid_postfix("LANG");
        }
    }
    return IsPosixDefault(id) ? "en_US_POSIX" : id;
}

std::string CorrectedPosixId(const char* posixId) {
    std::string corrected(posixId);
    corrected.resize(std::min(corrected.find('.'), corrected.size()));
    corrected.resize(std::min(corrected.find('@'), corrected.size()));
    if (corrected == "C" || corrected == "POSIX") corrected = "en_US_POSIX";
    if (const char* at = std::strrchr(posixId, '@')) {
        std::string_view variant(at + 1);
        if (variant == "nynorsk") variant = "NY";
        corrected += corrected.find('_') == std::string::npos ? "__" : "_";
        corrected += variant.substr(0, variant.find('.'));
    }
    return corrected;
}

const std::string& SystemLocaleId() {
    static const std::string id = CorrectedPosixId(PosixId());
    return id;
}

std::string Canonicalize(const char* id) {
    std::int32_t status = 0;
    const std::int32_t length = uloc_canonicalize(id, nullptr, 0, &status);
    if (status > 0 && status != Icu::BufferOverflow) NotImplemented_nid_no_patch("uloc_getDefault: the system locale ID does not canonicalize");
    std::string name(static_cast<std::size_t>(length) + 1, '\0');
    status = 0;
    uloc_canonicalize(id, name.data(), length + 1, &status);
    if (status > 0) NotImplemented_nid_no_patch("uloc_getDefault: the system locale ID does not canonicalize");
    name.resize(static_cast<std::size_t>(length));
    return name;
}

std::size_t LanguageLength(const std::string& name) {
    const auto at = name.find('@');
    const auto separator = name.find('_');
    if (separator != std::string::npos && (at == std::string::npos || separator < at)) return separator;
    return std::min(name.find_first_of("@."), name.size());
}

std::mutex defaultMutex;
const char* defaultName = nullptr;

std::set<std::string>& DefaultNames() {
    static std::set<std::string> names;
    return names;
}

const char* SetSystemDefault() {
    std::string name = Canonicalize(SystemLocaleId().c_str());
    if (LanguageLength(name) >= LanguageCapacity) name.clear();
    std::lock_guard lock(defaultMutex);
    defaultName = DefaultNames().insert(std::move(name)).first->c_str();
    return defaultName;
}

}

extern "C" {

const char* APS5_VABI uprv_getDefaultLocaleID_nid_postfix() {
    return SystemLocaleId().c_str();
}

const char* APS5_VABI uloc_getDefault() {
    {
        std::lock_guard lock(defaultMutex);
        if (defaultName) return defaultName;
    }
    return SetSystemDefault();
}

}

namespace {

bool CopyLocaleId(const char* source, std::string& copied) {
    constexpr int GuestOutOfMemory = 12;
    constexpr int GuestNameTooLong = 63;
    constexpr std::size_t maxSize = std::numeric_limits<std::int32_t>::max();
    for (std::size_t size = 256;; size = std::min(size * 2, maxSize)) {
        const int savedErrno = errno;
        const int error = CopyGuestString_nid_no_patch(source, size, copied);
        errno = savedErrno;
        if (error == 0) return true;
        if (error == GuestOutOfMemory) return false;
        if (error != GuestNameTooLong) NotImplemented_nid_no_patch("uloc_setDefault: unreadable locale ID");
        if (size == maxSize) return false;
    }
}

bool ResolveName(const char* id, std::string& resolved) {
    std::int32_t status = 0;
    const int savedErrno = errno;
    const std::int32_t length = uloc_getName_nid_postfix(id, nullptr, 0, &status);
    errno = savedErrno;
    if ((status > 0 && status != Icu::BufferOverflow) || length < 0) return false;
    if (length == 0) {
        resolved.clear();
        return true;
    }
    if (length == std::numeric_limits<std::int32_t>::max()) return false;
    resolved.assign(static_cast<std::size_t>(length) + 1, '\0');
    status = 0;
    const std::int32_t written = uloc_getName_nid_postfix(id, resolved.data(), length + 1, &status);
    errno = savedErrno;
    if (status > 0 || written < 0 || written > length) return false;
    resolved.resize(static_cast<std::size_t>(written));
    return true;
}

void StoreExplicitDefault(std::string name) {
    if (LanguageLength(name) >= LanguageCapacity) name.clear();
    std::lock_guard lock(defaultMutex);
    defaultName = DefaultNames().insert(std::move(name)).first->c_str();
}

}

extern "C" {

void APS5_VABI uloc_setDefault_nid_postfix(const char* newDefaultLocale, std::int32_t* status) {
    Icu::RequireStatus(status);
    std::int32_t incoming = 0;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&incoming, status, sizeof(incoming));
    errno = savedErrno;
    if (!readable) NotImplemented_nid_no_patch("uloc_setDefault: unreadable UErrorCode");
    if (incoming > 0) return;
    if (!newDefaultLocale) {
        SetSystemDefault();
        return;
    }
    std::string copied;
    if (!CopyLocaleId(newDefaultLocale, copied)) return;
    std::string resolved;
    if (!ResolveName(copied.c_str(), resolved)) return;
    StoreExplicitDefault(std::move(resolved));
}

}
