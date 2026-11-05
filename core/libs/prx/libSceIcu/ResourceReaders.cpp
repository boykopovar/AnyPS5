#include "prx/libSceIcu/ResourceReaders.hpp"
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

static_assert(sizeof(IcuResourceData) == 64);
static_assert(offsetof(IcuResourceData, pRoot) == 8);
static_assert(offsetof(IcuResourceData, p16BitUnits) == 16);
static_assert(offsetof(IcuResourceData, poolBundleKeys) == 24);
static_assert(offsetof(IcuResourceData, rootRes) == 32);
static_assert(offsetof(IcuResourceData, localKeyLimit) == 36);
static_assert(offsetof(IcuResourceData, poolBundleStrings) == 40);
static_assert(offsetof(IcuResourceData, poolStringIndexLimit) == 48);
static_assert(offsetof(IcuResourceData, poolStringIndex16Limit) == 52);
static_assert(offsetof(IcuResourceData, noFallback) == 56);
static_assert(sizeof(IcuDataInfo) == 20);

namespace {

constexpr std::uint32_t bogus = 0xffffffffu;
constexpr std::uint32_t offsetMask = 0x0fffffffu;
constexpr std::uint16_t empty16 = 0;
constexpr std::int32_t empty32[2] = {0, 0};
constexpr char16_t emptyString[1] = {0};

void Require(bool condition) {
    if (!condition) throw std::invalid_argument("ICU resource access outside upstream preconditions");
}

const IcuResourceData& Data(const IcuResourceData* data) {
    Require(data != nullptr);
    return *data;
}

const std::int32_t* Words(const IcuResourceData* data, std::uint32_t offset) {
    const auto* root = Data(data).pRoot;
    Require(root != nullptr);
    return root + offset;
}

const std::uint16_t* Units(const IcuResourceData* data, std::uint32_t offset) {
    const auto* units = Data(data).p16BitUnits;
    Require(units != nullptr);
    return units + offset;
}

template<typename T>
T Read(const void* source) {
    T value;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

std::uint32_t From16(const IcuResourceData* data, std::uint16_t value) {
    const auto& bundle = Data(data);
    std::int64_t offset = value;
    if (offset >= bundle.poolStringIndex16Limit)
        offset += static_cast<std::int64_t>(bundle.poolStringIndexLimit) - bundle.poolStringIndex16Limit;
    Require(offset >= 0 && offset <= offsetMask);
    return 0x60000000u | static_cast<std::uint32_t>(offset);
}

const char* Key16(const IcuResourceData* data, std::uint16_t offset) {
    const auto& bundle = Data(data);
    if (offset < bundle.localKeyLimit)
        return reinterpret_cast<const char*>(Words(data, 0)) + offset;
    Require(bundle.poolBundleKeys != nullptr);
    return bundle.poolBundleKeys + (static_cast<std::int64_t>(offset) - bundle.localKeyLimit);
}

const char* Key32(const IcuResourceData* data, std::int32_t offset) {
    if (offset >= 0) return reinterpret_cast<const char*>(Words(data, 0)) + offset;
    Require(Data(data).poolBundleKeys != nullptr);
    return data->poolBundleKeys + (static_cast<std::uint32_t>(offset) & 0x7fffffffu);
}

struct Table {
    const IcuResourceData* data;
    std::int32_t size;
    const std::uint16_t* keys16;
    const std::int32_t* keys32;
    const std::uint16_t* values16;
    const std::int32_t* values32;
    bool searchable;

    const char* Key(std::int32_t index) const {
        return keys16 ? Key16(data, Read<std::uint16_t>(keys16 + index)) : Key32(data, Read<std::int32_t>(keys32 + index));
    }

    std::uint32_t Value(std::int32_t index) const {
        return values16 ? From16(data, Read<std::uint16_t>(values16 + index)) : Read<std::uint32_t>(values32 + index);
    }
};

Table GetTable(const IcuResourceData* data, std::uint32_t resource) {
    Table table{data, 0, nullptr, nullptr, nullptr, nullptr, false};
    const auto offset = resource & offsetMask;
    switch (resource >> 28) {
    case 2:
        if (offset != 0) {
            const auto* units = reinterpret_cast<const std::uint16_t*>(Words(data, offset));
            table.size = Read<std::uint16_t>(units);
            table.keys16 = units + 1;
            table.values32 = reinterpret_cast<const std::int32_t*>(table.keys16 + table.size + ((table.size & 1) == 0 ? 1 : 0));
            table.searchable = true;
        }
        break;
    case 5: {
        const auto* units = Units(data, offset);
        table.size = Read<std::uint16_t>(units);
        table.keys16 = units + 1;
        table.values16 = table.keys16 + table.size;
        table.searchable = true;
        break;
    }
    case 4:
        if (offset != 0) {
            const auto* words = Words(data, offset);
            table.size = Read<std::int32_t>(words);
            table.keys32 = words + 1;
            table.values32 = table.size > 0 ? table.keys32 + table.size : table.keys32;
            table.searchable = true;
        }
        break;
    default:
        break;
    }
    return table;
}

const std::int32_t* Leaf(const IcuResourceData* data, std::uint32_t resource, unsigned type, std::int32_t* length) {
    if ((resource >> 28) != type) {
        if (length) *length = 0;
        return nullptr;
    }
    const auto offset = resource & offsetMask;
    const auto* words = offset == 0 ? empty32 : Words(data, offset);
    if (length) *length = Read<std::int32_t>(words);
    return words + 1;
}

} // namespace

extern "C" {

const char16_t* APS5_VABI res_getAlias_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* length) {
    const auto* result = Leaf(data, resource, 3, length);
    if (result && (resource & offsetMask) == 0) return emptyString;
    return reinterpret_cast<const char16_t*>(result);
}

const std::uint8_t* APS5_VABI res_getBinaryNoTrace_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* length) {
    return reinterpret_cast<const std::uint8_t*>(Leaf(data, resource, 1, length));
}

const std::int32_t* APS5_VABI res_getIntVectorNoTrace_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* length) {
    return Leaf(data, resource, 14, length);
}

const char16_t* APS5_VABI res_getStringNoTrace_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* length) {
    if ((resource >> 28) != 6) {
        const auto* result = Leaf(data, resource, 0, length);
        if (result && resource == 0) return emptyString;
        return reinterpret_cast<const char16_t*>(result);
    }
    const auto& bundle = Data(data);
    const auto offset = resource & offsetMask;
    const std::uint16_t* units;
    if (static_cast<std::int32_t>(offset) < bundle.poolStringIndexLimit) {
        Require(bundle.poolBundleStrings != nullptr);
        units = bundle.poolBundleStrings + offset;
    } else {
        const auto local = static_cast<std::int64_t>(offset) - bundle.poolStringIndexLimit;
        Require(local >= 0 && local <= offsetMask);
        units = Units(data, static_cast<std::uint32_t>(local));
    }
    const auto first = Read<std::uint16_t>(units);
    std::uint32_t count = 0;
    if (first < 0xdc00 || first > 0xdfff) {
        while (Read<std::uint16_t>(units + count) != 0) {
            Require(count < static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()));
            ++count;
        }
    } else if (first < 0xdfef) {
        count = first & 0x3ff;
        ++units;
    } else if (first < 0xdfff) {
        count = (static_cast<std::uint32_t>(first - 0xdfef) << 16) | Read<std::uint16_t>(units + 1);
        units += 2;
    } else {
        count = (static_cast<std::uint32_t>(Read<std::uint16_t>(units + 1)) << 16) | Read<std::uint16_t>(units + 2);
        units += 3;
        Require(count <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()));
    }
    if (length) *length = static_cast<std::int32_t>(count);
    return reinterpret_cast<const char16_t*>(units);
}

std::int32_t APS5_VABI res_countArrayItems_nid_postfix(const IcuResourceData* data, std::uint32_t resource) {
    const auto offset = resource & offsetMask;
    switch (resource >> 28) {
    case 0: case 1: case 3: case 6: case 7: case 14:
        return 1;
    case 4: case 8:
        return offset == 0 ? 0 : Read<std::int32_t>(Words(data, offset));
    case 2:
        return offset == 0 ? 0 : Read<std::uint16_t>(Words(data, offset));
    case 5: case 9:
        return Read<std::uint16_t>(Units(data, offset));
    default:
        return 0;
    }
}

std::uint32_t APS5_VABI res_getArrayItem_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t index) {
    if (index < 0) return bogus;
    const auto offset = resource & offsetMask;
    if ((resource >> 28) == 8 && offset != 0) {
        const auto* words = Words(data, offset);
        if (index < Read<std::int32_t>(words)) return Read<std::uint32_t>(words + 1 + index);
    } else if ((resource >> 28) == 9) {
        const auto* units = Units(data, offset);
        if (index < Read<std::uint16_t>(units)) return From16(data, Read<std::uint16_t>(units + 1 + index));
    }
    return bogus;
}

std::uint32_t APS5_VABI res_getTableItemByIndex_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t index, const char** key) {
    if (index < 0) return bogus;
    const auto table = GetTable(data, resource);
    if (index >= table.size) return bogus;
    if (key) *key = table.Key(index);
    return table.Value(index);
}

std::uint32_t APS5_VABI res_getTableItemByKey_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* index, const char** key) {
    if (key == nullptr || *key == nullptr) return bogus;
    const auto table = GetTable(data, resource);
    if (!table.searchable) return bogus;
    Require(index != nullptr);
    std::int32_t low = 0;
    std::int32_t high = table.size;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        const auto* candidate = table.Key(middle);
        const auto order = std::strcmp(*key, candidate);
        if (order < 0) high = middle;
        else if (order > 0) low = middle + 1;
        else {
            *key = candidate;
            *index = middle;
            return table.Value(middle);
        }
    }
    *index = -1;
    return bogus;
}

std::uint32_t APS5_VABI res_getResource_nid_postfix(const IcuResourceData* data, const char* key) {
    std::int32_t index;
    return res_getTableItemByKey_nid_postfix(data, Data(data).rootRes, &index, &key);
}

void APS5_VABI res_read_nid_postfix(IcuResourceData* data, const IcuDataInfo* info, const void* bytes, std::int32_t length, std::int32_t* error) {
    Require(data != nullptr);
    std::memset(data, 0, sizeof(*data));
    Require(error != nullptr);
    if (*error > 0) return;
    Require(info != nullptr);
    if (info->size < 20 || info->isBigEndian != 0 || info->charsetFamily != 0 || info->sizeofUChar != 2 ||
        info->dataFormat[0] != 'R' || info->dataFormat[1] != 'e' || info->dataFormat[2] != 's' || info->dataFormat[3] != 'B' ||
        info->formatVersion[0] < 1 || info->formatVersion[0] > 3) {
        *error = 3;
        return;
    }
    Require(bytes != nullptr);
    Require(reinterpret_cast<std::uintptr_t>(bytes) % alignof(std::int32_t) == 0);
    Require(length < 0 || length >= 4);
    data->pRoot = static_cast<const std::int32_t*>(bytes);
    data->rootRes = Read<std::uint32_t>(bytes);
    data->p16BitUnits = &empty16;
    const bool original = info->formatVersion[0] == 1 && info->formatVersion[1] == 0;
    const auto type = data->rootRes >> 28;
    if ((length >= 0 && length / 4 < (original ? 1 : 6)) || (type != 2 && type != 4 && type != 5)) {
        *error = 3;
        return;
    }
    if (original) {
        data->localKeyLimit = 65536;
    } else {
        const auto* indexes = data->pRoot + 1;
        const auto indexCount = Read<std::uint32_t>(indexes) & 255u;
        if (indexCount <= 4 || (length >= 0 && static_cast<std::uint32_t>(length / 4) < 1 + indexCount)) {
            *error = 3;
            return;
        }
        if (length >= 0) {
            const auto bundleTop = Read<std::int32_t>(indexes + 3);
            Require(bundleTop >= 0 && bundleTop <= std::numeric_limits<std::int32_t>::max() / 4);
            if (length < bundleTop * 4) {
                *error = 3;
                return;
            }
        }
        const auto keysTop = Read<std::int32_t>(indexes + 1);
        if (keysTop > static_cast<std::int32_t>(1 + indexCount)) {
            Require(keysTop <= std::numeric_limits<std::int32_t>::max() / 4);
            data->localKeyLimit = keysTop * 4;
        }
        if (info->formatVersion[0] >= 3) data->poolStringIndexLimit = Read<std::uint32_t>(indexes) >> 8;
        if (indexCount > 5) {
            const auto attributes = Read<std::uint32_t>(indexes + 5);
            data->noFallback = attributes & 1;
            data->isPoolBundle = (attributes & 2) != 0;
            data->usesPoolBundle = (attributes & 4) != 0;
            data->poolStringIndexLimit |= static_cast<std::int32_t>((attributes & 0xf000u) << 12);
            data->poolStringIndex16Limit = attributes >> 16;
        }
        if ((data->isPoolBundle || data->usesPoolBundle) && indexCount <= 7) {
            *error = 3;
            return;
        }
        if (indexCount > 6 && Read<std::int32_t>(indexes + 6) > keysTop) {
            Require(keysTop >= 0);
            data->p16BitUnits = reinterpret_cast<const std::uint16_t*>(data->pRoot + keysTop);
        }
    }
    data->useNativeStrcmp = 1;
}

}
