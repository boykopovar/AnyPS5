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
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

constexpr std::size_t Icu68ResourceBundleSize = 200;
constexpr std::size_t Icu68ResourceOffset = 172;
constexpr std::uint32_t IntegerType = 7;
constexpr std::uint32_t TypeShift = 28;
constexpr std::uint32_t ValueMask = 0x0fffffff;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t ResourceTypeMismatch = 17;
constexpr std::int32_t NoneType = -1;
constexpr std::uint32_t ReservedType = 15;
constexpr std::size_t Icu68IndexOffset = 192;
constexpr std::size_t Icu68SizeOffset = 196;
constexpr std::size_t Icu68RootOffset = 48;
constexpr std::size_t Icu68DataOffset = 8;
constexpr std::size_t Icu68TopLevelDataOffset = 24;
constexpr std::uint32_t StringType = 0;
constexpr std::uint32_t IntVectorType = 14;
constexpr std::uint32_t BinaryType = 1;
constexpr std::uint32_t TableType = 2;
constexpr std::uint32_t ArrayType = 8;
constexpr std::int32_t MissingResource = 2;
constexpr std::int32_t UnsupportedError = 16;
constexpr std::size_t Icu68ResDataOffset = 40;
constexpr std::size_t Icu68ResDataSize = 60;

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

bool ReadGuestString(const char* guest, char* buffer, std::size_t capacity, int savedErrno, const char* caller) {
    if (guest == nullptr || capacity < 2) return false;
    buffer[0] = 0;
    for (std::size_t offset = 0; offset + 1 < capacity; ++offset) {
        char value = 0;
        if (!CopyIn(&value, guest + offset, sizeof(value), savedErrno, caller)) return false;
        buffer[offset] = value;
        buffer[offset + 1] = 0;
        if (value == 0) return true;
    }
    return false;
}

bool ReadGuestString(const char* guest, std::string& out, int savedErrno, const char* caller) {
    char buffer[256];
    if (!ReadGuestString(guest, buffer, sizeof(buffer), savedErrno, caller)) return false;
    out = buffer;
    return true;
}


UResourceBundle* Fail(std::int32_t* status, std::int32_t value, int savedErrno, const char* caller) {
    CopyOut(status, &value, sizeof(value), savedErrno, caller);
    return nullptr;
}

std::int32_t ChildSize(const std::uint8_t* root, std::uint32_t resource, int savedErrno, const char* caller) {
    const std::uint32_t type = resource >> TypeShift;
    if (type != TableType && type != ArrayType) return 1;
    const std::uint32_t offset = resource & ValueMask;
    if (offset == 0) return 0;
    const auto* address = root + static_cast<std::size_t>(offset) * sizeof(std::int32_t);
    if (type == TableType) {
        std::uint16_t count = 0;
        CopyIn(&count, address, sizeof(count), savedErrno, caller);
        return count;
    }
    std::int32_t count = 0;
    CopyIn(&count, address, sizeof(count), savedErrno, caller);
    return count < 0 ? 0 : count;
}

UResourceBundle* FillChild(const UResourceBundle* bundle, const std::uint8_t* root, std::uint32_t resource,
    const char* key, UResourceBundle* fillIn, int savedErrno, const char* caller) {
    std::uint8_t data[Icu68ResDataSize];
    CopyIn(data, reinterpret_cast<const std::uint8_t*>(bundle) + Icu68ResDataOffset, sizeof(data), savedErrno, caller);
    const std::int32_t size = ChildSize(root, resource, savedErrno, caller);
    const std::int32_t index = 0;
    CopyOut(reinterpret_cast<std::uint8_t*>(fillIn) + Icu68ResDataOffset, data, sizeof(data), savedErrno, caller);
    CopyOut(fillIn, &key, sizeof(key), savedErrno, caller);
    CopyOut(reinterpret_cast<std::uint8_t*>(fillIn) + Icu68ResourceOffset, &resource, sizeof(resource), savedErrno,
        caller);
    CopyOut(reinterpret_cast<std::uint8_t*>(fillIn) + Icu68IndexOffset, &index, sizeof(index), savedErrno, caller);
    CopyOut(reinterpret_cast<std::uint8_t*>(fillIn) + Icu68SizeOffset, &size, sizeof(size), savedErrno, caller);
    return fillIn;
}

}

extern "C" {

void APS5_VABI ures_close(UResourceBundle* resB) {
    if (!resB) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ures_initStackObject_nid_postfix(UResourceBundle* resB) {
    std::memset(resB, 0, Icu68ResourceBundleSize);
}

std::uint32_t APS5_VABI ures_getUInt_nid_postfix(const UResourceBundle* resB, std::int32_t* status) {
    constexpr std::uint32_t failure = UINT32_MAX;
    if (!status) return failure;

    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return failure;
    if (!resB) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return failure;
    }

    std::uint32_t resource;
    const auto* resourceAddress = reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset;
    CopyIn(&resource, resourceAddress, sizeof(resource), savedErrno, __func__);
    if ((resource >> TypeShift) != IntegerType) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return failure;
    }
    return resource & ValueMask;
}

std::int32_t APS5_VABI ures_getType_nid_postfix(const UResourceBundle* resB) {
    if (!resB) return NoneType;
    const int savedErrno = errno;
    std::uint32_t resource;
    const auto* resourceAddress = reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset;
    CopyIn(&resource, resourceAddress, sizeof(resource), savedErrno, __func__);
    const std::uint32_t type = resource >> TypeShift;
    return type == ReservedType ? NoneType : static_cast<std::int32_t>(type);
}

const char* APS5_VABI ures_getKey_nid_postfix(const UResourceBundle* resB) {
    if (!resB) return nullptr;
    const int savedErrno = errno;
    const char* key = nullptr;
    CopyIn(&key, resB, sizeof(key), savedErrno, __func__);
    return key;
}

std::int32_t APS5_VABI ures_getSize_nid_postfix(const UResourceBundle* resB) {
    if (!resB) return 0;
    const int savedErrno = errno;
    std::int32_t size = 0;
    const auto* sizeAddress = reinterpret_cast<const std::uint8_t*>(resB) + Icu68SizeOffset;
    CopyIn(&size, sizeAddress, sizeof(size), savedErrno, __func__);
    return size;
}

std::int32_t APS5_VABI ures_hasNext_nid_postfix(const UResourceBundle* resB) {
    if (!resB) return 0;
    const int savedErrno = errno;
    std::int32_t index = 0;
    std::int32_t size = 0;
    const auto* indexAddress = reinterpret_cast<const std::uint8_t*>(resB) + Icu68IndexOffset;
    const auto* sizeAddress = reinterpret_cast<const std::uint8_t*>(resB) + Icu68SizeOffset;
    CopyIn(&index, indexAddress, sizeof(index), savedErrno, __func__);
    CopyIn(&size, sizeAddress, sizeof(size), savedErrno, __func__);
    return index < size - 1 ? 1 : 0;
}

const char16_t* APS5_VABI ures_getString_nid_postfix(const UResourceBundle* resB, std::int32_t* length,
    std::int32_t* status) {
    if (length) {
        const std::int32_t zeroLength = 0;
        CopyOut(length, &zeroLength, sizeof(zeroLength), errno, __func__);
    }
    if (!status) return nullptr;

    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!resB) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    std::uint32_t resource;
    CopyIn(&resource, reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset, sizeof(resource), savedErrno,
        __func__);
    if ((resource >> TypeShift) != StringType) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    const std::uint32_t offset = resource & ValueMask;
    const std::uint8_t* rootBytes = nullptr;
    CopyIn(&rootBytes, reinterpret_cast<const std::uint8_t*>(resB) + Icu68RootOffset, sizeof(rootBytes), savedErrno,
        __func__);
    if (!rootBytes) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    std::int32_t stringLength;
    const auto* lengthAddress = rootBytes + static_cast<std::size_t>(offset) * sizeof(std::int32_t);
    CopyIn(&stringLength, lengthAddress, sizeof(stringLength), savedErrno, __func__);
    if (stringLength < 0) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }
    if (length) {
        CopyOut(length, &stringLength, sizeof(stringLength), savedErrno, __func__);
    }
    return reinterpret_cast<const char16_t*>(lengthAddress + sizeof(std::int32_t));
}

std::int32_t APS5_VABI ures_getInt_nid_postfix(const UResourceBundle* resB, std::int32_t* status) {
    if (!status) return 0;

    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return 0;
    if (!resB) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return 0;
    }

    std::uint32_t resource;
    CopyIn(&resource, reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset, sizeof(resource), savedErrno,
        __func__);
    if ((resource >> TypeShift) != IntegerType) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return 0;
    }
    return static_cast<std::int32_t>(resource & ValueMask);
}

const std::int32_t* APS5_VABI ures_getIntVector_nid_postfix(const UResourceBundle* resB, std::int32_t* length,
    std::int32_t* status) {
    if (length) {
        const std::int32_t zeroLength = 0;
        CopyOut(length, &zeroLength, sizeof(zeroLength), errno, __func__);
    }
    if (!status) return nullptr;

    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!resB) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    std::uint32_t resource;
    CopyIn(&resource, reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset, sizeof(resource), savedErrno,
        __func__);
    if ((resource >> TypeShift) != IntVectorType) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    const std::uint32_t offset = resource & ValueMask;
    const std::uint8_t* rootBytes = nullptr;
    CopyIn(&rootBytes, reinterpret_cast<const std::uint8_t*>(resB) + Icu68RootOffset, sizeof(rootBytes), savedErrno,
        __func__);
    if (!rootBytes) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    const auto* vectorAddress = rootBytes + static_cast<std::size_t>(offset) * sizeof(std::int32_t);
    std::int32_t vectorLength;
    CopyIn(&vectorLength, vectorAddress, sizeof(vectorLength), savedErrno, __func__);
    if (vectorLength < 0) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }
    if (length) {
        CopyOut(length, &vectorLength, sizeof(vectorLength), savedErrno, __func__);
    }
    return reinterpret_cast<const std::int32_t*>(vectorAddress + sizeof(std::int32_t));
}

const std::uint8_t* APS5_VABI ures_getBinary_nid_postfix(const UResourceBundle* resB, std::int32_t* length,
    std::int32_t* status) {
    if (length) {
        const std::int32_t zeroLength = 0;
        CopyOut(length, &zeroLength, sizeof(zeroLength), errno, __func__);
    }
    if (!status) return nullptr;

    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!resB) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    std::uint32_t resource;
    CopyIn(&resource, reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset, sizeof(resource), savedErrno,
        __func__);
    if ((resource >> TypeShift) != BinaryType) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    const std::uint32_t offset = resource & ValueMask;
    const std::uint8_t* rootBytes = nullptr;
    CopyIn(&rootBytes, reinterpret_cast<const std::uint8_t*>(resB) + Icu68RootOffset, sizeof(rootBytes), savedErrno,
        __func__);
    if (!rootBytes) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    const auto* binaryAddress = rootBytes + static_cast<std::size_t>(offset) * sizeof(std::int32_t);
    std::int32_t binaryLength;
    CopyIn(&binaryLength, binaryAddress, sizeof(binaryLength), savedErrno, __func__);
    if (binaryLength < 0) {
        currentStatus = ResourceTypeMismatch;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }
    if (length) {
        CopyOut(length, &binaryLength, sizeof(binaryLength), savedErrno, __func__);
    }
    return binaryAddress + sizeof(std::int32_t);
}

extern "C" const char* APS5_VABI ures_getLocaleByType_nid_postfix(const UResourceBundle* resB, std::int32_t type,
    std::int32_t* status) {
    if (!status) return nullptr;
    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!resB) {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    std::size_t entryOffset = 0;
    if (type == 0) {
        entryOffset = Icu68DataOffset;
    } else if (type == 1) {
        entryOffset = Icu68TopLevelDataOffset;
    } else {
        currentStatus = IllegalArgument;
        CopyOut(status, &currentStatus, sizeof(currentStatus), savedErrno, __func__);
        return nullptr;
    }

    const char* entry = nullptr;
    CopyIn(&entry, reinterpret_cast<const std::uint8_t*>(resB) + entryOffset, sizeof(entry), savedErrno, __func__);
    if (!entry) return nullptr;
    const char* name = nullptr;
    CopyIn(&name, entry, sizeof(name), savedErrno, __func__);
    return name;
}

UResourceBundle* APS5_VABI ures_getByKey_nid_postfix(const UResourceBundle* resB, const char* key,
    UResourceBundle* fillIn, std::int32_t* status) {
    if (!status) return nullptr;
    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!resB || !fillIn) return Fail(status, IllegalArgument, savedErrno, __func__);

    std::uint32_t resource;
    CopyIn(&resource, reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset, sizeof(resource), savedErrno,
        __func__);
    if ((resource >> TypeShift) != TableType) return Fail(status, ResourceTypeMismatch, savedErrno, __func__);

    const std::uint32_t offset = resource & ValueMask;
    if (offset == 0 || !key) return Fail(status, MissingResource, savedErrno, __func__);

    const std::uint8_t* root = nullptr;
    CopyIn(&root, reinterpret_cast<const std::uint8_t*>(resB) + Icu68RootOffset, sizeof(root), savedErrno, __func__);
    if (!root) return Fail(status, UnsupportedError, savedErrno, __func__);

    const auto* table = root + static_cast<std::size_t>(offset) * sizeof(std::int32_t);
    std::uint16_t count = 0;
    CopyIn(&count, table, sizeof(count), savedErrno, __func__);
    const auto* keys = table + sizeof(count);
    const char* foundKey = nullptr;
    std::string lookupKey;
    if (!ReadGuestString(key, lookupKey, savedErrno, __func__)) return Fail(status, IllegalArgument, savedErrno, __func__);
    std::int32_t found = -1;
    for (std::uint16_t index = 0; index < count; ++index) {
        std::uint16_t keyOffset = 0;
        CopyIn(&keyOffset, keys + static_cast<std::size_t>(index) * sizeof(keyOffset), sizeof(keyOffset), savedErrno,
            __func__);
        const auto* keyText = reinterpret_cast<const char*>(root + keyOffset);
        char candidate[256] = {};
        if (!ReadGuestString(keyText, candidate, sizeof(candidate), savedErrno, __func__)) continue;
        if (lookupKey == candidate) {
            found = index;
            foundKey = keyText;
            break;
        }
    }
    if (found < 0) return Fail(status, MissingResource, savedErrno, __func__);

    const auto* values = keys + static_cast<std::size_t>(count) * sizeof(std::uint16_t) +
        ((~count & 1) ? sizeof(std::uint16_t) : 0);
    std::int32_t child = 0;
    CopyIn(&child, values + static_cast<std::size_t>(found) * sizeof(child), sizeof(child), savedErrno, __func__);
    return FillChild(resB, root, static_cast<std::uint32_t>(child), foundKey, fillIn, savedErrno, __func__);
}

UResourceBundle* APS5_VABI ures_getByIndex_nid_postfix(const UResourceBundle* resB, std::int32_t index,
    UResourceBundle* fillIn, std::int32_t* status) {
    if (!status) return nullptr;
    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;
    if (!resB || !fillIn) return Fail(status, IllegalArgument, savedErrno, __func__);

    std::int32_t size = 0;
    CopyIn(&size, reinterpret_cast<const std::uint8_t*>(resB) + Icu68SizeOffset, sizeof(size), savedErrno, __func__);
    if (index < 0 || index >= size) return Fail(status, MissingResource, savedErrno, __func__);

    std::uint32_t resource;
    CopyIn(&resource, reinterpret_cast<const std::uint8_t*>(resB) + Icu68ResourceOffset, sizeof(resource), savedErrno,
        __func__);
    if ((resource >> TypeShift) != ArrayType) return Fail(status, ResourceTypeMismatch, savedErrno, __func__);

    const std::uint32_t offset = resource & ValueMask;
    if (offset == 0) return Fail(status, MissingResource, savedErrno, __func__);
    const std::uint8_t* root = nullptr;
    CopyIn(&root, reinterpret_cast<const std::uint8_t*>(resB) + Icu68RootOffset, sizeof(root), savedErrno, __func__);
    if (!root) return Fail(status, UnsupportedError, savedErrno, __func__);

    const auto* array = root + static_cast<std::size_t>(offset) * sizeof(std::int32_t);
    std::int32_t count = 0;
    CopyIn(&count, array, sizeof(count), savedErrno, __func__);
    if (index >= count) return Fail(status, MissingResource, savedErrno, __func__);
    std::int32_t value = 0;
    CopyIn(&value, array + (static_cast<std::size_t>(index) + 1) * sizeof(value), sizeof(value), savedErrno,
        __func__);
    return FillChild(resB, root, static_cast<std::uint32_t>(value), nullptr, fillIn, savedErrno, __func__);
}

const char16_t* APS5_VABI ures_getStringByKey_nid_postfix(const UResourceBundle* resB, const char* key,
    std::int32_t* length, std::int32_t* status) {
    if (length) {
        const std::int32_t zeroLength = 0;
        CopyOut(length, &zeroLength, sizeof(zeroLength), errno, __func__);
    }
    if (!status) return nullptr;

    const int savedErrno = errno;
    std::int32_t currentStatus;
    CopyIn(&currentStatus, status, sizeof(currentStatus), savedErrno, __func__);
    if (currentStatus > 0) return nullptr;

    alignas(8) std::uint8_t storage[Icu68ResourceBundleSize] = {};
    auto* child = reinterpret_cast<UResourceBundle*>(storage);
    ures_initStackObject_nid_postfix(child);

    std::int32_t lookupStatus = 0;
    if (ures_getByKey_nid_postfix(resB, key, child, &lookupStatus) == nullptr || lookupStatus > 0) {
        if (lookupStatus == 0) lookupStatus = MissingResource;
        CopyOut(status, &lookupStatus, sizeof(lookupStatus), savedErrno, __func__);
        return nullptr;
    }
    const char16_t* result = ures_getString_nid_postfix(child, length, &lookupStatus);
    if (lookupStatus > 0) {
        CopyOut(status, &lookupStatus, sizeof(lookupStatus), savedErrno, __func__);
    }
    return result;
}

}
