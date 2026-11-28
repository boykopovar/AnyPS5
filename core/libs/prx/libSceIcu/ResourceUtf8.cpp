// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 1997-2016, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*/
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresbund.cpp

#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ustring.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr std::size_t Icu68ResourceOffset = 172;
constexpr std::size_t Icu68RootOffset = 48;
constexpr std::size_t Icu68SizeOffset = 196;
constexpr std::uint32_t TypeShift = 28;
constexpr std::uint32_t ValueMask = 0x0fffffff;
constexpr std::uint32_t StringType = 0;
constexpr std::uint32_t TableType = 2;
constexpr std::uint32_t AliasType = 3;
constexpr std::uint32_t StringV2Type = 6;
constexpr std::uint32_t ArrayType = 8;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MissingResource = 2;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t UnsupportedError = 16;
constexpr std::int32_t ResourceTypeMismatch = 17;
constexpr std::int32_t StringNotTerminated = -124;
constexpr std::size_t MaximumKeyLength = 0x10000;

bool CopyIn(void* destination, const void* source, std::size_t bytes, int savedErrno, const char* caller) {
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
    return copied;
}

void CopyOut(void* destination, const void* source, std::size_t bytes, int savedErrno, const char* caller) {
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

const char* Fail(std::int32_t* status, std::int32_t value, int savedErrno, const char* caller) {
    CopyOut(status, &value, sizeof(value), savedErrno, caller);
    return nullptr;
}

void ReadGuestString(const char* source, std::string& text, int savedErrno, const char* caller) {
    errno = savedErrno;
    const int error = CopyGuestString_nid_no_patch(source, MaximumKeyLength, text);
    errno = savedErrno;
    if (error != 0) NotImplemented_nid_no_patch(caller);
}

const std::uint8_t* ReadRoot(const UResourceBundle* resB, int savedErrno, const char* caller) {
    const std::uint8_t* root = nullptr;
    CopyIn(&root, reinterpret_cast<const std::uint8_t*>(resB) + Icu68RootOffset, sizeof(root), savedErrno, caller);
    return root;
}

bool ReadString(const std::uint8_t* root, std::uint32_t resource, std::vector<char16_t>& units, int savedErrno,
    const char* caller) {
    if ((resource >> TypeShift) != StringType) return false;
    if (resource == 0) {
        units.clear();
        return true;
    }
    const auto* address = root + static_cast<std::size_t>(resource & ValueMask) * sizeof(std::int32_t);
    std::int32_t length = -1;
    CopyIn(&length, address, sizeof(length), savedErrno, caller);
    if (length < 0) return false;
    units.resize(static_cast<std::size_t>(length));
    if (length > 0) {
        CopyIn(units.data(), address + sizeof(length), static_cast<std::size_t>(length) * sizeof(char16_t), savedErrno,
            caller);
    }
    return true;
}

bool FindTableItem(const std::uint8_t* root, std::uint32_t table, const char* key, std::uint32_t& item,
    int savedErrno, const char* caller) {
    const auto* address = root + static_cast<std::size_t>(table & ValueMask) * sizeof(std::int32_t);
    std::uint16_t count = 0;
    CopyIn(&count, address, sizeof(count), savedErrno, caller);
    const auto* keys = address + sizeof(count);
    std::string wanted;
    ReadGuestString(key, wanted, savedErrno, caller);
    for (std::uint16_t index = 0; index < count; ++index) {
        std::uint16_t keyOffset = 0;
        CopyIn(&keyOffset, keys + static_cast<std::size_t>(index) * sizeof(keyOffset), sizeof(keyOffset), savedErrno,
            caller);
        std::string candidate;
        ReadGuestString(reinterpret_cast<const char*>(root + keyOffset), candidate, savedErrno, caller);
        if (candidate != wanted) continue;
        const auto* values = keys + static_cast<std::size_t>(count) * sizeof(std::uint16_t) +
            ((~count & 1) ? sizeof(std::uint16_t) : 0);
        std::int32_t found = 0;
        CopyIn(&found, values + static_cast<std::size_t>(index) * sizeof(found), sizeof(found), savedErrno, caller);
        item = static_cast<std::uint32_t>(found);
        return true;
    }
    return false;
}

bool ItemByIndex(const std::uint8_t* root, std::uint32_t resource, std::int32_t index, std::uint32_t& item,
    int savedErrno, const char* caller) {
    const std::uint32_t type = resource >> TypeShift;
    const auto* address = root + static_cast<std::size_t>(resource & ValueMask) * sizeof(std::int32_t);
    if (type == StringType) {
        item = resource;
        return index == 0;
    }
    if (type == TableType) {
        std::uint16_t count = 0;
        CopyIn(&count, address, sizeof(count), savedErrno, caller);
        if (index < 0 || index >= count) return false;
        const auto* keys = address + sizeof(count);
        const auto* values = keys + static_cast<std::size_t>(count) * sizeof(std::uint16_t) +
            ((~count & 1) ? sizeof(std::uint16_t) : 0);
        std::int32_t found = 0;
        CopyIn(&found, values + static_cast<std::size_t>(index) * sizeof(found), sizeof(found), savedErrno, caller);
        item = static_cast<std::uint32_t>(found);
        return true;
    }
    if (type == ArrayType) {
        std::int32_t count = 0;
        CopyIn(&count, address, sizeof(count), savedErrno, caller);
        if (index < 0 || index >= count) return false;
        std::int32_t found = 0;
        CopyIn(&found, address + (static_cast<std::size_t>(index) + 1) * sizeof(found), sizeof(found), savedErrno,
            caller);
        item = static_cast<std::uint32_t>(found);
        return true;
    }
    return false;
}

const char* ToUtf8(const std::vector<char16_t>& units, char* dest, std::int32_t* length, std::int8_t forceCopy,
    std::int32_t* status, int savedErrno, const char* caller) {
    std::int32_t capacity = 0;
    if (length) CopyIn(&capacity, length, sizeof(capacity), savedErrno, caller);
    if (capacity < 0 || (capacity > 0 && !dest)) return Fail(status, IllegalArgument, savedErrno, caller);
    if (units.empty()) {
        const std::int32_t zero = 0;
        if (length) CopyOut(length, &zero, sizeof(zero), savedErrno, caller);
        if (!forceCopy) return "";
        if (capacity > 0) {
            const char terminator = 0;
            CopyOut(dest, &terminator, sizeof(terminator), savedErrno, caller);
        } else {
            CopyOut(status, &StringNotTerminated, sizeof(StringNotTerminated), savedErrno, caller);
        }
        return dest;
    }
    if (capacity < static_cast<std::int32_t>(units.size())) {
        std::int32_t utf8Length = 0;
        UErrorCode hostStatus = U_ZERO_ERROR;
        u_strToUTF8(nullptr, 0, &utf8Length, reinterpret_cast<const UChar*>(units.data()),
            static_cast<std::int32_t>(units.size()), &hostStatus);
        if (length) CopyOut(length, &utf8Length, sizeof(utf8Length), savedErrno, caller);
        return Fail(status, BufferOverflow, savedErrno, caller);
    }
    std::int32_t writeCapacity = capacity;
    std::size_t offset = 0;
    if (!forceCopy && units.size() <= 0x2aaaaaaa) {
        const std::int32_t maximum = 3 * static_cast<std::int32_t>(units.size()) + 1;
        if (capacity > maximum) {
            offset = static_cast<std::size_t>(capacity - maximum);
            writeCapacity = maximum;
        }
    }
    std::vector<char> buffer(static_cast<std::size_t>(writeCapacity));
    std::int32_t utf8Length = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    u_strToUTF8(buffer.data(), writeCapacity, &utf8Length, reinterpret_cast<const UChar*>(units.data()),
        static_cast<std::int32_t>(units.size()), &hostStatus);
    const std::int32_t copied = utf8Length < writeCapacity ? utf8Length + 1 : writeCapacity;
    const bool wrote = hostStatus == U_ZERO_ERROR || static_cast<std::int32_t>(hostStatus) == StringNotTerminated;
    if (wrote && copied > 0) {
        CopyOut(dest + offset, buffer.data(), static_cast<std::size_t>(copied), savedErrno, caller);
    }
    if (length) CopyOut(length, &utf8Length, sizeof(utf8Length), savedErrno, caller);
    if (hostStatus != U_ZERO_ERROR) {
        const std::int32_t code = static_cast<std::int32_t>(hostStatus);
        CopyOut(status, &code, sizeof(code), savedErrno, caller);
    }
    return dest + offset;
}

}

extern "C" {

const char* APS5_VABI ures_getUTF8String_nid_postfix(const UResourceBundle* resB, char* dest, std::int32_t* length,
    std::int8_t forceCopy, std::int32_t* status) {
    if (!status) return nullptr;
    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!resB) return Fail(status, IllegalArgument, savedErrno, __func__);

    std::uint32_t resource;
    CopyIn(&resource, reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset, sizeof(resource), savedErrno,
        __func__);
    if ((resource >> TypeShift) != StringType) return Fail(status, ResourceTypeMismatch, savedErrno, __func__);
    const std::uint8_t* root = ReadRoot(resB, savedErrno, __func__);
    if (!root) return Fail(status, UnsupportedError, savedErrno, __func__);

    std::vector<char16_t> units;
    if (!ReadString(root, resource, units, savedErrno, __func__)) {
        return Fail(status, ResourceTypeMismatch, savedErrno, __func__);
    }
    return ToUtf8(units, dest, length, forceCopy, status, savedErrno, __func__);
}

const char* APS5_VABI ures_getUTF8StringByIndex_nid_postfix(const UResourceBundle* resB, std::int32_t index, char* dest,
    std::int32_t* length, std::int8_t forceCopy, std::int32_t* status) {
    if (!status) return nullptr;
    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!resB) return Fail(status, IllegalArgument, savedErrno, __func__);

    std::int32_t size = 0;
    CopyIn(&size, reinterpret_cast<const std::uint8_t*>(resB) + Icu68SizeOffset, sizeof(size), savedErrno, __func__);
    if (index < 0 || index >= size) return Fail(status, MissingResource, savedErrno, __func__);

    std::uint32_t resource;
    CopyIn(&resource, reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset, sizeof(resource), savedErrno,
        __func__);
    const std::uint32_t type = resource >> TypeShift;
    if (type != StringType && type != TableType && type != ArrayType) {
        return Fail(status, ResourceTypeMismatch, savedErrno, __func__);
    }
    const std::uint8_t* root = ReadRoot(resB, savedErrno, __func__);
    if (!root) return Fail(status, UnsupportedError, savedErrno, __func__);

    std::uint32_t item = 0;
    if (!ItemByIndex(root, resource, index, item, savedErrno, __func__)) {
        return Fail(status, MissingResource, savedErrno, __func__);
    }
    std::vector<char16_t> units;
    const std::uint32_t itemType = item >> TypeShift;
    if (itemType != StringType && itemType != StringV2Type && itemType != AliasType) {
        return ToUtf8(units, dest, length, forceCopy, status, savedErrno, __func__);
    }
    if (!ReadString(root, item, units, savedErrno, __func__)) {
        return Fail(status, ResourceTypeMismatch, savedErrno, __func__);
    }
    return ToUtf8(units, dest, length, forceCopy, status, savedErrno, __func__);
}

const char* APS5_VABI ures_getUTF8StringByKey_nid_postfix(const UResourceBundle* resB, const char* key, char* dest,
    std::int32_t* length, std::int8_t forceCopy, std::int32_t* status) {
    if (!status) return nullptr;
    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!resB) return Fail(status, IllegalArgument, savedErrno, __func__);

    std::uint32_t resource;
    CopyIn(&resource, reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset, sizeof(resource), savedErrno,
        __func__);
    if ((resource >> TypeShift) != TableType) return Fail(status, ResourceTypeMismatch, savedErrno, __func__);
    if (!key) return Fail(status, MissingResource, savedErrno, __func__);
    const std::uint8_t* root = ReadRoot(resB, savedErrno, __func__);
    if (!root) return Fail(status, UnsupportedError, savedErrno, __func__);

    std::uint32_t item = 0;
    if (!FindTableItem(root, resource, key, item, savedErrno, __func__)) {
        return Fail(status, MissingResource, savedErrno, __func__);
    }
    std::vector<char16_t> units;
    if (!ReadString(root, item, units, savedErrno, __func__)) {
        return Fail(status, ResourceTypeMismatch, savedErrno, __func__);
    }
    return ToUtf8(units, dest, length, forceCopy, status, savedErrno, __func__);
}

}
