// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 1997-2016, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*/
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresbund.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresdata.cpp

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_set>

namespace {

constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t MissingResourceError = 2;
constexpr std::int32_t ResourceTypeMismatch = 17;
constexpr std::int32_t UsingFallbackWarning = -128;
constexpr std::int32_t UsingDefaultWarning = -127;
constexpr std::uint32_t BogusResource = 0xffffffff;
constexpr std::uint32_t ResourceOffsetMask = 0x0fffffff;

enum ResourceType : std::uint32_t {
    String,
    Binary,
    Table,
    Alias,
    Table32,
    Table16,
    StringV2,
    Integer,
    Array,
    Array16,
    IntegerVector = 14
};

struct GuestResourceData {
    void* data;
    const std::int32_t* root;
    const std::uint16_t* units16;
    const char* poolKeys;
    std::uint32_t rootResource;
    std::int32_t localKeyLimit;
    const std::uint16_t* poolStrings;
    std::int32_t poolStringIndexLimit;
    std::int32_t poolStringIndex16Limit;
    std::int8_t noFallback;
    std::int8_t isPoolBundle;
    std::int8_t usesPoolBundle;
    std::int8_t useNativeCompare;
};

struct GuestResourceEntry {
    char* name;
    char* path;
    GuestResourceEntry* parent;
    GuestResourceEntry* alias;
    GuestResourceEntry* pool;
    GuestResourceData resourceData;
    std::array<char, 3> nameBuffer;
    std::uint32_t countExisting;
    std::int32_t bogus;
};

struct GuestResourceBundle {
    const char* key;
    GuestResourceEntry* data;
    char* version;
    GuestResourceEntry* topLevelData;
    char* resourcePath;
    GuestResourceData resourceData;
    std::array<char, 64> resourceBuffer;
    std::int32_t resourcePathLength;
    std::uint32_t resource;
    std::int8_t hasFallback;
    std::int8_t isTopLevel;
    std::uint16_t padding;
    std::uint32_t magic1;
    std::uint32_t magic2;
    std::int32_t index;
    std::int32_t size;
};

static_assert(sizeof(GuestResourceData) == 64);
static_assert(offsetof(GuestResourceEntry, resourceData) == 40);
static_assert(sizeof(GuestResourceEntry) == 120);
static_assert(offsetof(GuestResourceBundle, resourceData) == 40);
static_assert(offsetof(GuestResourceBundle, resourceBuffer) == 104);
static_assert(offsetof(GuestResourceBundle, resource) == 172);
static_assert(offsetof(GuestResourceBundle, magic1) == 180);
static_assert(sizeof(GuestResourceBundle) == 200);

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

template <typename TValue>
TValue Read(const void* source, const char* function) {
    TValue value{};
    if (!CopyIn(&value, source, sizeof(value))) NotImplemented_nid_no_patch(function);
    return value;
}

const void* AddBytes(const void* base, std::uint64_t bytes, const char* function) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - address) NotImplemented_nid_no_patch(function);
    return reinterpret_cast<const void*>(address + static_cast<std::uintptr_t>(bytes));
}

const void* AddElements(
    const void* base, std::uint64_t index, std::uint64_t elementSize, const char* function) {
    if (index > std::numeric_limits<std::uint64_t>::max() / elementSize) NotImplemented_nid_no_patch(function);
    return AddBytes(base, index * elementSize, function);
}

std::string CopyText(const char* source, const char* function) {
    std::string result;
    std::size_t offset = 0;
    for (;;) {
        const auto* current = static_cast<const char*>(AddBytes(source, offset, function));
        const std::size_t pageRemaining = 4096 - reinterpret_cast<std::uintptr_t>(current) % 4096;
        const std::size_t count = pageRemaining < 256 ? pageRemaining : 256;
        std::array<char, 256> buffer{};
        if (!CopyIn(buffer.data(), current, count)) NotImplemented_nid_no_patch(function);
        const auto* end = static_cast<const char*>(std::memchr(buffer.data(), 0, count));
        result.append(buffer.data(), end ? static_cast<std::size_t>(end - buffer.data()) : count);
        if (end) return result;
        if (offset > std::numeric_limits<std::size_t>::max() - count) NotImplemented_nid_no_patch(function);
        offset += count;
    }
}

void SetStatus(std::int32_t* status, std::int32_t value, const char* function) {
    if (!CopyOut(status, &value, sizeof(value))) NotImplemented_nid_no_patch(function);
}

std::uint32_t Type(std::uint32_t resource) {
    return resource >> 28;
}

std::uint32_t Offset(std::uint32_t resource) {
    return resource & ResourceOffsetMask;
}

bool IsTable(std::uint32_t type) {
    return type == Table || type == Table16 || type == Table32;
}

const char* Key16(
    const GuestResourceData& data, std::uint16_t keyOffset, const char* function) {
    if (static_cast<std::int32_t>(keyOffset) < data.localKeyLimit) {
        return static_cast<const char*>(AddBytes(data.root, keyOffset, function));
    }
    const auto offset = static_cast<std::int64_t>(keyOffset) - data.localKeyLimit;
    if (offset < 0) NotImplemented_nid_no_patch(function);
    return static_cast<const char*>(AddBytes(data.poolKeys, static_cast<std::uint64_t>(offset), function));
}

const char* Key32(
    const GuestResourceData& data, std::int32_t keyOffset, const char* function) {
    if (keyOffset >= 0) {
        return static_cast<const char*>(AddBytes(data.root, static_cast<std::uint32_t>(keyOffset), function));
    }
    return static_cast<const char*>(AddBytes(
        data.poolKeys, static_cast<std::uint32_t>(keyOffset) & 0x7fffffff, function));
}

template <typename TOffset, typename TKey>
std::int32_t FindKey(
    const GuestResourceData& data, const void* offsets, std::int32_t length,
    const std::string& key, TKey keyAddress, const char* function) {
    std::int32_t start = 0;
    std::int32_t limit = length;
    while (start < limit) {
        const std::int32_t middle = start + (limit - start) / 2;
        const auto offset = Read<TOffset>(
            AddElements(offsets, static_cast<std::uint32_t>(middle), sizeof(TOffset), function), function);
        const std::string tableKey = CopyText(keyAddress(data, offset, function), function);
        const int compared = key.compare(tableKey);
        if (compared < 0) {
            limit = middle;
        } else if (compared > 0) {
            start = middle + 1;
        } else {
            return middle;
        }
    }
    return -1;
}

struct Item {
    bool found = false;
    std::uint32_t resource = BogusResource;
};

Item FindItem(
    const GuestResourceData& data, std::uint32_t table, const std::string* key, const char* function) {
    if (!key) return {};
    const std::uint32_t offset = Offset(table);
    switch (Type(table)) {
    case Table: {
        if (offset == 0) return {};
        const void* base = AddElements(data.root, offset, sizeof(std::int32_t), function);
        const auto length = Read<std::uint16_t>(base, function);
        const void* offsets = AddBytes(base, sizeof(std::uint16_t), function);
        const auto index = FindKey<std::uint16_t>(data, offsets, length, *key, Key16, function);
        if (index < 0) return {};
        const std::uint64_t padded = static_cast<std::uint64_t>(length) + ((~length) & 1);
        const void* resources = AddElements(offsets, padded, sizeof(std::uint16_t), function);
        return {true, Read<std::uint32_t>(
            AddElements(resources, static_cast<std::uint32_t>(index), sizeof(std::uint32_t), function), function)};
    }
    case Table16: {
        const void* base = AddElements(data.units16, offset, sizeof(std::uint16_t), function);
        const auto length = Read<std::uint16_t>(base, function);
        const void* offsets = AddBytes(base, sizeof(std::uint16_t), function);
        const auto index = FindKey<std::uint16_t>(data, offsets, length, *key, Key16, function);
        if (index < 0) return {};
        std::int64_t resource16 = Read<std::uint16_t>(AddElements(
            offsets, static_cast<std::uint64_t>(length) + static_cast<std::uint32_t>(index),
            sizeof(std::uint16_t), function), function);
        if (resource16 >= data.poolStringIndex16Limit) {
            resource16 = resource16 - data.poolStringIndex16Limit + data.poolStringIndexLimit;
        }
        if (resource16 < 0 || resource16 > ResourceOffsetMask) NotImplemented_nid_no_patch(function);
        return {true, (StringV2 << 28) | static_cast<std::uint32_t>(resource16)};
    }
    case Table32: {
        if (offset == 0) return {};
        const void* base = AddElements(data.root, offset, sizeof(std::int32_t), function);
        const auto length = Read<std::int32_t>(base, function);
        if (length <= 0) return {};
        const void* offsets = AddBytes(base, sizeof(std::int32_t), function);
        const auto index = FindKey<std::int32_t>(data, offsets, length, *key, Key32, function);
        if (index < 0) return {};
        const void* resources = AddElements(offsets, static_cast<std::uint32_t>(length), sizeof(std::int32_t), function);
        return {true, Read<std::uint32_t>(
            AddElements(resources, static_cast<std::uint32_t>(index), sizeof(std::uint32_t), function), function)};
    }
    default:
        return {};
    }
}

std::int32_t Count(
    const GuestResourceData& data, std::uint32_t resource, const char* function) {
    const std::uint32_t offset = Offset(resource);
    switch (Type(resource)) {
    case String:
    case StringV2:
    case Binary:
    case Integer:
    case IntegerVector:
        return 1;
    case Alias:
        NotImplemented_nid_no_patch(function);
    case Array:
    case Table32:
        return offset == 0 ? 0 : Read<std::int32_t>(
            AddElements(data.root, offset, sizeof(std::int32_t), function), function);
    case Table:
        return offset == 0 ? 0 : Read<std::uint16_t>(
            AddBytes(data.root, offset, function), function);
    case Array16:
    case Table16:
        return Read<std::uint16_t>(
            AddElements(data.units16, offset, sizeof(std::uint16_t), function), function);
    default:
        return 0;
    }
}

struct Selection {
    GuestResourceData data{};
    std::uint32_t resource = BogusResource;
    std::int32_t status = 0;
    bool found = false;
};

Selection Select(
    const GuestResourceBundle& bundle, const std::string* key,
    std::int32_t status, const char* function) {
    const Item direct = FindItem(bundle.resourceData, bundle.resource, key, function);
    if (direct.found) return {bundle.resourceData, direct.resource, status, true};
    if (bundle.hasFallback != 1) return {{}, BogusResource, MissingResourceError, false};

    auto* entryAddress = bundle.data;
    std::unordered_set<const GuestResourceEntry*> visited;
    GuestResourceEntry entry{};
    Item item{};
    while (entryAddress) {
        if (!visited.insert(entryAddress).second) NotImplemented_nid_no_patch(function);
        entry = Read<GuestResourceEntry>(entryAddress, function);
        if (entry.bogus == 0) {
            item = FindItem(entry.resourceData, entry.resourceData.rootResource, key, function);
            if (item.found) break;
        }
        entryAddress = entry.parent;
    }
    if (!item.found) return {{}, BogusResource, MissingResourceError, false};
    const std::string name = CopyText(entry.name, function);
    const char* defaultLocale = uloc_getDefault();
    status = name == "root" || (defaultLocale && name == defaultLocale)
        ? UsingDefaultWarning
        : UsingFallbackWarning;
    return {entry.resourceData, item.resource, status, true};
}

} // namespace

extern "C" std::int32_t APS5_VABI ures_countArrayItems_nid_postfix(
    const UResourceBundle* resourceBundle, const char* resourceKey, std::int32_t* status) {
    if (!status) return 0;
    const auto originalStatus = Read<std::int32_t>(status, __func__);
    if (originalStatus > 0) return 0;
    if (!resourceBundle) {
        SetStatus(status, IllegalArgumentError, __func__);
        return 0;
    }
    const auto bundle = Read<GuestResourceBundle>(resourceBundle, __func__);
    if (!IsTable(Type(bundle.resource))) {
        SetStatus(status, ResourceTypeMismatch, __func__);
        return 0;
    }
    std::string keyStorage;
    const std::string* key = nullptr;
    if (resourceKey) {
        keyStorage = CopyText(resourceKey, __func__);
        key = &keyStorage;
    }
    const Selection selected = Select(bundle, key, originalStatus, __func__);
    if (!selected.found || !selected.data.data) {
        SetStatus(status, MissingResourceError, __func__);
        return 0;
    }
    const auto result = Count(selected.data, selected.resource, __func__);
    if (selected.status != originalStatus) SetStatus(status, selected.status, __func__);
    return result;
}
