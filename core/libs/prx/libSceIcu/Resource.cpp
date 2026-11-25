#include "prx/libSceIcu/Identified.hpp"
#include "prx/libc/include/General.hpp"
#include "uresdata.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

static_assert(sizeof(Icu::DataInfo) == sizeof(UDataInfo));
static_assert(alignof(Icu::DataInfo) == alignof(UDataInfo));
static_assert(offsetof(Icu::DataInfo, size) == offsetof(UDataInfo, size));
static_assert(offsetof(Icu::DataInfo, reservedWord) == offsetof(UDataInfo, reservedWord));
static_assert(offsetof(Icu::DataInfo, isBigEndian) == offsetof(UDataInfo, isBigEndian));
static_assert(offsetof(Icu::DataInfo, charsetFamily) == offsetof(UDataInfo, charsetFamily));
static_assert(offsetof(Icu::DataInfo, sizeofUChar) == offsetof(UDataInfo, sizeofUChar));
static_assert(offsetof(Icu::DataInfo, reservedByte) == offsetof(UDataInfo, reservedByte));
static_assert(offsetof(Icu::DataInfo, dataFormat) == offsetof(UDataInfo, dataFormat));
static_assert(offsetof(Icu::DataInfo, formatVersion) == offsetof(UDataInfo, formatVersion));
static_assert(offsetof(Icu::DataInfo, dataVersion) == offsetof(UDataInfo, dataVersion));

static_assert(sizeof(Icu::ResourceData) == sizeof(::ResourceData));
static_assert(alignof(Icu::ResourceData) == alignof(::ResourceData));
static_assert(offsetof(Icu::ResourceData, data) == offsetof(::ResourceData, data));
static_assert(offsetof(Icu::ResourceData, pRoot) == offsetof(::ResourceData, pRoot));
static_assert(offsetof(Icu::ResourceData, p16BitUnits) == offsetof(::ResourceData, p16BitUnits));
static_assert(offsetof(Icu::ResourceData, poolBundleKeys) == offsetof(::ResourceData, poolBundleKeys));
static_assert(offsetof(Icu::ResourceData, rootRes) == offsetof(::ResourceData, rootRes));
static_assert(offsetof(Icu::ResourceData, localKeyLimit) == offsetof(::ResourceData, localKeyLimit));
static_assert(offsetof(Icu::ResourceData, poolBundleStrings) == offsetof(::ResourceData, poolBundleStrings));
static_assert(offsetof(Icu::ResourceData, poolStringIndexLimit) == offsetof(::ResourceData, poolStringIndexLimit));
static_assert(offsetof(Icu::ResourceData, poolStringIndex16Limit) == offsetof(::ResourceData, poolStringIndex16Limit));
static_assert(offsetof(Icu::ResourceData, noFallback) == offsetof(::ResourceData, noFallback));
static_assert(offsetof(Icu::ResourceData, isPoolBundle) == offsetof(::ResourceData, isPoolBundle));
static_assert(offsetof(Icu::ResourceData, usesPoolBundle) == offsetof(::ResourceData, usesPoolBundle));
static_assert(offsetof(Icu::ResourceData, useNativeStrcmp) == offsetof(::ResourceData, useNativeStrcmp));

namespace {

bool ReadBytes(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool WriteBytes(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool IsWritable(void* address, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(address, bytes);
    errno = savedErrno;
    return ok;
}

template <typename T>
bool ReadValue(T& out, const void* source) {
    return ReadBytes(&out, source, sizeof(out));
}

template <typename T>
bool WriteValue(void* destination, const T& value) {
    return WriteBytes(destination, &value, sizeof(value));
}

::ResourceData ReadResourceData(const Icu::ResourceData* resourceData, const char* caller) {
    if (!resourceData) throw std::invalid_argument(std::string(caller) + ": null resource data");
    ::ResourceData local{};
    if (!ReadValue(local, resourceData)) throw std::invalid_argument(std::string(caller) + ": unreadable resource data");
    return local;
}

void RequirePointer(const void* pointer, const char* what, const char* caller) {
    if (!pointer) throw std::invalid_argument(std::string(caller) + ": " + what);
}

std::string ReadKeyString(const char* source, const char* caller) {
    RequirePointer(source, "null key", caller);
    std::string result;
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    std::size_t offset = 0;
    for (;;) {
        if (offset > std::numeric_limits<std::uintptr_t>::max() - base)
            throw std::invalid_argument(std::string(caller) + ": key address overflow");
        const auto current = base + offset;
        const std::size_t pageRemaining = 4096 - current % 4096;
        const std::size_t count = pageRemaining < 256 ? pageRemaining : 256;
        char buffer[256];
        if (!ReadBytes(buffer, reinterpret_cast<const void*>(current), count))
            throw std::invalid_argument(std::string(caller) + ": unreadable key");
        const auto* end = static_cast<const char*>(std::memchr(buffer, 0, count));
        result.append(buffer, end ? static_cast<std::size_t>(end - buffer) : count);
        if (end) return result;
        offset += count;
    }
}

void WriteLength(std::int32_t* destination, std::int32_t value, const char* caller) {
    if (destination && !WriteValue(destination, value))
        throw std::invalid_argument(std::string(caller) + ": unwritable length");
}

void RequireLengthWritable(std::int32_t* destination, const char* caller) {
    if (destination && !IsWritable(destination, sizeof(*destination)))
        throw std::invalid_argument(std::string(caller) + ": unwritable length");
}

bool IsAcceptableInfo(const UDataInfo& info) {
    return info.size >= 20 &&
           info.isBigEndian == U_IS_BIG_ENDIAN &&
           info.charsetFamily == U_CHARSET_FAMILY &&
           info.sizeofUChar == U_SIZEOF_UCHAR &&
           info.dataFormat[0] == 0x52 &&
           info.dataFormat[1] == 0x65 &&
           info.dataFormat[2] == 0x73 &&
           info.dataFormat[3] == 0x42 &&
           info.formatVersion[0] >= 1 &&
           info.formatVersion[0] <= 3;
}

void ProbeResourceInput(const UDataInfo& info, const void* inBytes, std::int32_t length, const char* caller) {
    RequirePointer(inBytes, "null resource bytes", caller);
    std::uint32_t rootResource = 0;
    if (!ReadValue(rootResource, inBytes))
        throw std::invalid_argument(std::string(caller) + ": unreadable resource bytes");
    if (info.formatVersion[0] == 1 && info.formatVersion[1] == 0) return;
    if (length >= 0 && length / 4 < 6) return;
    const auto rootType = static_cast<UResType>(RES_GET_TYPE(rootResource));
    if (!URES_IS_TABLE(rootType)) return;
    std::array<std::int32_t, 5> baseIndexes{};
    const auto* indexBase = static_cast<const std::int32_t*>(inBytes) + 1;
    if (!ReadBytes(baseIndexes.data(), indexBase, sizeof(baseIndexes)))
        throw std::invalid_argument(std::string(caller) + ": unreadable resource indexes");
    const std::int32_t indexLength = baseIndexes[URES_INDEX_LENGTH] & 0xff;
    if (indexLength <= URES_INDEX_MAX_TABLE_LENGTH) return;
    if (length >= 0 && (length < ((1 + indexLength) << 2) || length < (baseIndexes[URES_INDEX_BUNDLE_TOP] << 2))) return;
    const std::int32_t extraCount = std::min<std::int32_t>(indexLength, 8) - 5;
    if (extraCount > 0) {
        std::array<std::int32_t, 3> extraIndexes{};
        if (!ReadBytes(extraIndexes.data(), indexBase + 5, static_cast<std::size_t>(extraCount) * sizeof(std::int32_t)))
            throw std::invalid_argument(std::string(caller) + ": unreadable extended resource indexes");
    }
}

void RequireRootReadable(const ::ResourceData& data, std::uint32_t offset, const char* caller) {
    RequirePointer(data.pRoot, "null pRoot", caller);
    std::int32_t probe = 0;
    if (!ReadValue(probe, data.pRoot + offset))
        throw std::invalid_argument(std::string(caller) + ": unreadable resource root");
}

void RequireUnits16Readable(const ::ResourceData& data, std::uint32_t offset, const char* caller) {
    RequirePointer(data.p16BitUnits, "null p16BitUnits", caller);
    std::uint16_t probe = 0;
    if (!ReadValue(probe, data.p16BitUnits + offset))
        throw std::invalid_argument(std::string(caller) + ": unreadable 16-bit resource units");
}

void RequireStringReadable(const ::ResourceData& data, std::uint32_t offset, const char* caller) {
    if (static_cast<std::int32_t>(offset) < data.poolStringIndexLimit) {
        RequirePointer(data.poolBundleStrings, "null poolBundleStrings", caller);
        std::uint16_t probe = 0;
        if (!ReadValue(probe, data.poolBundleStrings + offset))
            throw std::invalid_argument(std::string(caller) + ": unreadable pool string");
        return;
    }
    RequireUnits16Readable(data, offset - static_cast<std::uint32_t>(data.poolStringIndexLimit), caller);
}

bool UsesTableData(std::int32_t type, std::uint32_t offset) {
    return type == URES_TABLE16 || ((type == URES_TABLE || type == URES_TABLE32) && offset != 0);
}

} // namespace

extern "C" {

std::int32_t APS5_VABI res_getPublicType_nid_postfix(std::uint32_t resource) {
    static constexpr std::array<std::int32_t, 16> publicTypes{0, 1, 2, 3, 2, 2, 0, 7, 8, 8, -1, -1, -1, -1, 14, -1};
    return publicTypes[resource >> 28];
}

void APS5_VABI res_read_nid_postfix(Icu::ResourceData* resourceData, const Icu::DataInfo* info, const void* inBytes,
                                    std::int32_t length, std::int32_t* errorCode) {
    if (!resourceData) throw std::invalid_argument("res_read: null resource data");
    if (!errorCode) throw std::invalid_argument("res_read: null error code");
    std::int32_t incoming = 0;
    if (!ReadValue(incoming, errorCode)) throw std::invalid_argument("res_read: unreadable error code");
    ::ResourceData localData{};
    if (!WriteValue(resourceData, localData)) throw std::invalid_argument("res_read: unwritable resource data");
    if (U_FAILURE(static_cast<UErrorCode>(incoming))) return;

    RequirePointer(info, "null data info", __func__);
    std::uint16_t infoSize = 0;
    if (!ReadValue(infoSize, info)) throw std::invalid_argument("res_read: unreadable data info");
    UDataInfo localInfo{};
    if (infoSize < 20) {
        localInfo.size = infoSize;
    } else if (!ReadValue(localInfo, info)) {
        throw std::invalid_argument("res_read: unreadable data info");
    }

    auto hostStatus = static_cast<UErrorCode>(incoming);
    if (!IsAcceptableInfo(localInfo)) {
        const std::int32_t invalidFormat = U_INVALID_FORMAT_ERROR;
        if (invalidFormat != incoming && !WriteValue(errorCode, invalidFormat))
            throw std::invalid_argument("res_read: unwritable error code");
        return;
    }

    ProbeResourceInput(localInfo, inBytes, length, __func__);
    ::res_read(&localData, &localInfo, inBytes, length, &hostStatus);
    const auto outgoing = static_cast<std::int32_t>(hostStatus);
    if (outgoing != incoming && !WriteValue(errorCode, outgoing))
        throw std::invalid_argument("res_read: unwritable error code");
    if (!WriteValue(resourceData, localData)) throw std::invalid_argument("res_read: unwritable resource data");
}

const char16_t* APS5_VABI res_getStringNoTrace_nid_postfix(const Icu::ResourceData* resourceData,
                                                           std::uint32_t resource, std::int32_t* length) {
    RequireLengthWritable(length, __func__);
    const std::uint32_t offset = RES_GET_OFFSET(resource);
    const std::int32_t type = RES_GET_TYPE(resource);
    std::int32_t localLength = 0;
    const char16_t* result = nullptr;
    if (type == URES_STRING_V2) {
        const ::ResourceData localData = ReadResourceData(resourceData, __func__);
        RequireStringReadable(localData, offset, __func__);
        result = ::res_getStringNoTrace(&localData, resource, &localLength);
    } else if (resource == offset && resource != 0) {
        const ::ResourceData localData = ReadResourceData(resourceData, __func__);
        RequireRootReadable(localData, resource, __func__);
        result = ::res_getStringNoTrace(&localData, resource, &localLength);
    } else {
        result = ::res_getStringNoTrace(nullptr, resource, &localLength);
    }
    WriteLength(length, localLength, __func__);
    return result;
}

const char16_t* APS5_VABI res_getAlias_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t resource,
                                                   std::int32_t* length) {
    RequireLengthWritable(length, __func__);
    const std::uint32_t offset = RES_GET_OFFSET(resource);
    std::int32_t localLength = 0;
    const char16_t* result = nullptr;
    if (RES_GET_TYPE(resource) == URES_ALIAS && offset != 0) {
        const ::ResourceData localData = ReadResourceData(resourceData, __func__);
        RequireRootReadable(localData, offset, __func__);
        result = ::res_getAlias(&localData, resource, &localLength);
    } else {
        result = ::res_getAlias(nullptr, resource, &localLength);
    }
    WriteLength(length, localLength, __func__);
    return result;
}

const std::uint8_t* APS5_VABI res_getBinaryNoTrace_nid_postfix(const Icu::ResourceData* resourceData,
                                                               std::uint32_t resource, std::int32_t* length) {
    RequireLengthWritable(length, __func__);
    const std::uint32_t offset = RES_GET_OFFSET(resource);
    std::int32_t localLength = 0;
    const std::uint8_t* result = nullptr;
    if (RES_GET_TYPE(resource) == URES_BINARY && offset != 0) {
        const ::ResourceData localData = ReadResourceData(resourceData, __func__);
        RequireRootReadable(localData, offset, __func__);
        result = ::res_getBinaryNoTrace(&localData, resource, &localLength);
    } else {
        result = ::res_getBinaryNoTrace(nullptr, resource, &localLength);
    }
    WriteLength(length, localLength, __func__);
    return result;
}

const std::int32_t* APS5_VABI res_getIntVectorNoTrace_nid_postfix(const Icu::ResourceData* resourceData,
                                                                  std::uint32_t resource, std::int32_t* length) {
    RequireLengthWritable(length, __func__);
    const std::uint32_t offset = RES_GET_OFFSET(resource);
    std::int32_t localLength = 0;
    const std::int32_t* result = nullptr;
    if (RES_GET_TYPE(resource) == URES_INT_VECTOR && offset != 0) {
        const ::ResourceData localData = ReadResourceData(resourceData, __func__);
        RequireRootReadable(localData, offset, __func__);
        result = ::res_getIntVectorNoTrace(&localData, resource, &localLength);
    } else {
        result = ::res_getIntVectorNoTrace(nullptr, resource, &localLength);
    }
    WriteLength(length, localLength, __func__);
    return result;
}

std::int32_t APS5_VABI res_countArrayItems_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t resource) {
    const std::uint32_t offset = RES_GET_OFFSET(resource);
    const std::int32_t type = RES_GET_TYPE(resource);
    if (type == URES_ARRAY16 || type == URES_TABLE16) {
        const ::ResourceData localData = ReadResourceData(resourceData, __func__);
        RequireUnits16Readable(localData, offset, __func__);
        return ::res_countArrayItems(&localData, resource);
    }
    if ((type == URES_ARRAY || type == URES_TABLE32 || type == URES_TABLE) && offset != 0) {
        const ::ResourceData localData = ReadResourceData(resourceData, __func__);
        RequireRootReadable(localData, offset, __func__);
        return ::res_countArrayItems(&localData, resource);
    }
    return ::res_countArrayItems(nullptr, resource);
}

std::uint32_t APS5_VABI res_getArrayItem_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t array,
                                                     std::int32_t index) {
    if (index < 0) return RES_BOGUS;
    const std::uint32_t offset = RES_GET_OFFSET(array);
    const std::int32_t type = RES_GET_TYPE(array);
    if (type == URES_ARRAY16) {
        const ::ResourceData localData = ReadResourceData(resourceData, __func__);
        RequireUnits16Readable(localData, offset, __func__);
        return ::res_getArrayItem(&localData, array, index);
    }
    if (type == URES_ARRAY && offset != 0) {
        const ::ResourceData localData = ReadResourceData(resourceData, __func__);
        RequireRootReadable(localData, offset, __func__);
        return ::res_getArrayItem(&localData, array, index);
    }
    return RES_BOGUS;
}

std::uint32_t APS5_VABI res_getTableItemByIndex_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t table,
                                                            std::int32_t index, const char** key) {
    if (index < 0) return RES_BOGUS;
    const std::uint32_t offset = RES_GET_OFFSET(table);
    const std::int32_t type = RES_GET_TYPE(table);
    if (!UsesTableData(type, offset)) return RES_BOGUS;
    const ::ResourceData localData = ReadResourceData(resourceData, __func__);
    if (type == URES_TABLE16) RequireUnits16Readable(localData, offset, __func__);
    else RequireRootReadable(localData, offset, __func__);
    const char* localKey = nullptr;
    const Resource result = ::res_getTableItemByIndex(&localData, table, index, key ? &localKey : nullptr);
    if (result != RES_BOGUS && key && !WriteValue(key, localKey))
        throw std::invalid_argument("res_getTableItemByIndex: unwritable key");
    return result;
}

std::uint32_t APS5_VABI res_getTableItemByKey_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t table,
                                                          std::int32_t* index, const char** key) {
    if (!key) return RES_BOGUS;
    const char* guestKey = nullptr;
    if (!ReadValue(guestKey, key)) throw std::invalid_argument("res_getTableItemByKey: unreadable key pointer");
    if (!guestKey) return RES_BOGUS;
    const std::uint32_t offset = RES_GET_OFFSET(table);
    const std::int32_t type = RES_GET_TYPE(table);
    if (!UsesTableData(type, offset)) return RES_BOGUS;
    const ::ResourceData localData = ReadResourceData(resourceData, __func__);
    if (!index || !IsWritable(index, sizeof(*index)))
        throw std::invalid_argument("res_getTableItemByKey: unwritable index");
    const std::string keyCopy = ReadKeyString(guestKey, __func__);
    if (type == URES_TABLE16) RequireUnits16Readable(localData, offset, __func__);
    else RequireRootReadable(localData, offset, __func__);
    const char* localKey = keyCopy.c_str();
    std::int32_t localIndex = -1;
    const Resource result = ::res_getTableItemByKey(&localData, table, &localIndex, &localKey);
    if (localIndex >= 0 && !IsWritable(const_cast<char**>(key), sizeof(*key)))
        throw std::invalid_argument("res_getTableItemByKey: unwritable key pointer");
    if (!WriteValue(index, localIndex)) throw std::invalid_argument("res_getTableItemByKey: unwritable index");
    if (localIndex >= 0 && !WriteValue(key, localKey))
        throw std::invalid_argument("res_getTableItemByKey: unwritable key pointer");
    return result;
}

std::uint32_t APS5_VABI res_getResource_nid_postfix(const Icu::ResourceData* resourceData, const char* key) {
    const ::ResourceData localData = ReadResourceData(resourceData, __func__);
    if (!key) return RES_BOGUS;
    const Resource root = localData.rootRes;
    const std::uint32_t offset = RES_GET_OFFSET(root);
    const std::int32_t type = RES_GET_TYPE(root);
    if (!UsesTableData(type, offset)) return RES_BOGUS;
    const std::string keyCopy = ReadKeyString(key, __func__);
    if (type == URES_TABLE16) RequireUnits16Readable(localData, offset, __func__);
    else RequireRootReadable(localData, offset, __func__);
    return ::res_getResource(&localData, keyCopy.c_str());
}

}
