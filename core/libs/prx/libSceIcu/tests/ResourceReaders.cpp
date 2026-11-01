#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint32_t bogus = 0xffffffffu;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU resource readers: %s\n", message);
        std::abort();
    }
}

constexpr std::uint32_t Resource(std::uint32_t type, std::uint32_t offset = 0) {
    return (type << 28) | offset;
}

struct Fixture {
    std::array<std::int32_t, 512> root{};
    std::array<std::uint16_t, 512> units{};
    std::array<std::uint16_t, 64> pool{};
    std::array<char, 10> keys{'b', 'e', 't', 'a', '\0', 'z', 'e', 't', 'a', '\0'};
    IcuResourceData data{};

    Fixture() {
        data.pRoot = root.data();
        data.p16BitUnits = units.data();
        data.poolBundleStrings = pool.data();
        data.poolBundleKeys = keys.data();
        data.localKeyLimit = 64;
        data.useNativeStrcmp = 1;
        std::memcpy(reinterpret_cast<char*>(root.data()) + 8, "alpha", 6);
        std::memcpy(reinterpret_cast<char*>(root.data()) + 16, "omega", 6);
    }

    void Put16(std::size_t byteOffset, std::uint16_t value) {
        std::memcpy(reinterpret_cast<char*>(root.data()) + byteOffset, &value, sizeof(value));
    }
};

void CheckLeaves() {
    Fixture f;
    f.root[8] = 3;
    const std::array<std::uint8_t, 3> bytes{0, 128, 255};
    std::memcpy(f.root.data() + 9, bytes.data(), bytes.size());
    f.root[16] = 3;
    const std::array<char16_t, 4> alias{u'a', 0, 0xd800, 0};
    std::memcpy(f.root.data() + 17, alias.data(), sizeof(alias));
    f.root[32] = 3;
    f.root[33] = std::numeric_limits<std::int32_t>::min();
    f.root[34] = -1;
    f.root[35] = std::numeric_limits<std::int32_t>::max();
    f.root[48] = 3;
    std::memcpy(f.root.data() + 49, alias.data(), sizeof(alias));
    std::int32_t length = -1;
    const auto* binary = res_getBinaryNoTrace_nid_postfix(&f.data, Resource(1, 8), &length);
    Check(length == 3 && binary == reinterpret_cast<const std::uint8_t*>(f.root.data() + 9), "binary length and address");
    Check(std::memcmp(binary, bytes.data(), bytes.size()) == 0, "binary includes zero and high bytes");
    Check(res_getBinaryNoTrace_nid_postfix(&f.data, Resource(1, 8), nullptr) == binary, "binary optional length");
    const auto* text = res_getAlias_nid_postfix(&f.data, Resource(3, 16), &length);
    Check(length == 3 && std::memcmp(text, alias.data(), sizeof(alias)) == 0, "alias embedded zero and surrogate");
    Check(res_getAlias_nid_postfix(&f.data, Resource(3, 16), nullptr) == text, "alias optional length");
    const auto* vector = res_getIntVectorNoTrace_nid_postfix(&f.data, Resource(14, 32), &length);
    Check(length == 3 && vector == f.root.data() + 33 && vector[0] == std::numeric_limits<std::int32_t>::min() && vector[2] == std::numeric_limits<std::int32_t>::max(), "integer vector");
    Check(res_getIntVectorNoTrace_nid_postfix(&f.data, Resource(14, 32), nullptr) == vector, "vector optional length");
    text = res_getStringNoTrace_nid_postfix(&f.data, Resource(0, 48), &length);
    Check(length == 3 && std::memcmp(text, alias.data(), sizeof(alias)) == 0, "legacy string length");
    Check(res_getStringNoTrace_nid_postfix(&f.data, Resource(0, 48), nullptr) == text, "string optional length");
    for (std::uint32_t tag = 0; tag < 16; ++tag) {
        if (tag != 1) Check(res_getBinaryNoTrace_nid_postfix(nullptr, Resource(tag), &length) == nullptr && length == 0, "binary wrong tag");
        if (tag != 3) Check(res_getAlias_nid_postfix(nullptr, Resource(tag), &length) == nullptr && length == 0, "alias wrong tag");
        if (tag != 14) Check(res_getIntVectorNoTrace_nid_postfix(nullptr, Resource(tag), &length) == nullptr && length == 0, "vector wrong tag");
        if (tag != 0 && tag != 6) Check(res_getStringNoTrace_nid_postfix(nullptr, Resource(tag), &length) == nullptr && length == 0, "string wrong tag");
    }
    Check(res_getBinaryNoTrace_nid_postfix(nullptr, Resource(1), &length) != nullptr && length == 0, "empty binary");
    Check(res_getAlias_nid_postfix(nullptr, Resource(3), &length)[0] == 0 && length == 0, "empty alias");
    Check(res_getIntVectorNoTrace_nid_postfix(nullptr, Resource(14), &length) != nullptr && length == 0, "empty vector");
    Check(res_getStringNoTrace_nid_postfix(nullptr, 0, &length)[0] == 0 && length == 0, "empty string");
}

void CheckStrings() {
    Fixture f;
    f.units[1] = u'a';
    f.units[2] = u'b';
    f.units[3] = u'c';
    const std::array<std::uint16_t, 5> shortString{0xdc03, u'x', 0, 0xdc00, 0};
    std::copy(shortString.begin(), shortString.end(), f.units.begin() + 20);
    const std::array<std::uint16_t, 7> mediumString{0xdfef, 4, u'a', u'b', u'c', u'd', 0};
    std::copy(mediumString.begin(), mediumString.end(), f.units.begin() + 30);
    const std::array<std::uint16_t, 8> longString{0xdfff, 0, 4, u'w', u'x', u'y', u'z', 0};
    std::copy(longString.begin(), longString.end(), f.units.begin() + 40);
    for (const auto pair : std::array<std::array<std::int32_t, 3>, 4>{{{1, 1, 3}, {20, 21, 3}, {30, 32, 4}, {40, 43, 4}}}) {
        std::int32_t length = -1;
        const auto* text = res_getStringNoTrace_nid_postfix(&f.data, Resource(6, pair[0]), &length);
        Check(length == pair[2] && text == reinterpret_cast<const char16_t*>(f.units.data() + pair[1]), "string-v2 length encoding");
    }
    f.units[100] = 0xdd01;
    std::fill(f.units.begin() + 101, f.units.begin() + 358, u'q');
    std::int32_t wideLength = -1;
    Check(res_getStringNoTrace_nid_postfix(&f.data, Resource(6, 100), &wideLength) == reinterpret_cast<const char16_t*>(f.units.data() + 101) && wideLength == 257, "short string length uses all ten bits");
    f.data.poolStringIndexLimit = 10;
    f.pool[1] = u'p';
    std::int32_t length = -1;
    Check(res_getStringNoTrace_nid_postfix(&f.data, Resource(6, 1), &length) == reinterpret_cast<const char16_t*>(f.pool.data() + 1) && length == 1, "pooled string");
    Check(res_getStringNoTrace_nid_postfix(&f.data, Resource(6, 11), &length) == reinterpret_cast<const char16_t*>(f.units.data() + 1) && length == 3, "local string after pool range");
    Check(res_getStringNoTrace_nid_postfix(&f.data, Resource(6, 10), &length)[0] == 0 && length == 0, "pool boundary empty local string");
}

void CheckCountsAndArrays() {
    Fixture f;
    f.root[80] = 3;
    f.units[80] = 4;
    constexpr std::array<std::int32_t, 16> counts{1, 1, 3, 1, 3, 4, 1, 1, 3, 4, 0, 0, 0, 0, 1, 0};
    constexpr std::array<std::int32_t, 16> emptyCounts{1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0};
    for (std::uint32_t tag = 0; tag < 16; ++tag) {
        Check(res_countArrayItems_nid_postfix(&f.data, Resource(tag, 80)) == counts[tag], "count all resource tags");
        Check(res_countArrayItems_nid_postfix(&f.data, Resource(tag)) == emptyCounts[tag], "count all empty resource tags");
    }
    f.root[96] = 3;
    f.root[97] = Resource(7, 1);
    f.root[98] = Resource(7, 2);
    f.root[99] = Resource(7, 3);
    f.units[96] = 3;
    f.units[97] = 1;
    f.units[98] = 11;
    f.units[99] = 13;
    f.data.poolStringIndex16Limit = 10;
    f.data.poolStringIndexLimit = 20;
    for (std::int32_t i = 0; i < 3; ++i) {
        Check(res_getArrayItem_nid_postfix(&f.data, Resource(8, 96), i) == Resource(7, i + 1), "array32 item");
        const std::uint32_t expected = i == 0 ? 1 : (i == 1 ? 21 : 23);
        Check(res_getArrayItem_nid_postfix(&f.data, Resource(9, 96), i) == Resource(6, expected), "array16 pooled offset conversion");
    }
    for (const auto i : {-1, 3, std::numeric_limits<std::int32_t>::max()}) {
        Check(res_getArrayItem_nid_postfix(&f.data, Resource(8, 96), i) == bogus, "array32 invalid index");
        Check(res_getArrayItem_nid_postfix(&f.data, Resource(9, 96), i) == bogus, "array16 invalid index");
    }
    Check(res_getArrayItem_nid_postfix(nullptr, Resource(8), 0) == bogus, "empty array32 does not dereference data");
    Check(res_getArrayItem_nid_postfix(nullptr, Resource(9), -1) == bogus, "negative array index does not dereference data");
    for (std::uint32_t tag = 0; tag < 16; ++tag) if (tag != 8 && tag != 9) Check(res_getArrayItem_nid_postfix(nullptr, Resource(tag), 0) == bogus, "array wrong type");
}

void CheckTables() {
    Fixture f;
    f.Put16(128 * 4, 3);
    f.Put16(128 * 4 + 2, 8);
    f.Put16(128 * 4 + 4, 64);
    f.Put16(128 * 4 + 6, 16);
    f.root[130] = Resource(7, 1);
    f.root[131] = Resource(7, 2);
    f.root[132] = Resource(7, 3);
    f.units[128] = 3;
    f.units[129] = 8;
    f.units[130] = 64;
    f.units[131] = 16;
    f.units[132] = 1;
    f.units[133] = 11;
    f.units[134] = 13;
    f.root[160] = 3;
    f.root[161] = 8;
    f.root[162] = std::numeric_limits<std::int32_t>::min();
    f.root[163] = 16;
    f.root[164] = Resource(7, 1);
    f.root[165] = Resource(7, 2);
    f.root[166] = Resource(7, 3);
    f.data.poolStringIndex16Limit = 10;
    f.data.poolStringIndexLimit = 20;
    constexpr std::array<const char*, 3> names{"alpha", "beta", "omega"};
    for (const auto native : {0, 1}) {
        f.data.useNativeStrcmp = native;
        for (const auto tag : {2u, 4u, 5u}) {
            const auto table = Resource(tag, tag == 4 ? 160 : 128);
            f.data.rootRes = table;
            for (std::int32_t i = 0; i < 3; ++i) {
                const auto expected = tag == 5 ? Resource(6, i == 0 ? 1 : (i == 1 ? 21 : 23)) : Resource(7, i + 1);
                const char* key = nullptr;
                Check(res_getTableItemByIndex_nid_postfix(&f.data, table, i, &key) == expected && std::strcmp(key, names[i]) == 0, "table index lookup");
                Check(res_getTableItemByIndex_nid_postfix(&f.data, table, i, nullptr) == expected, "table index optional key");
                const auto* canonical = key;
                key = names[i];
                std::int32_t index = -77;
                Check(res_getTableItemByKey_nid_postfix(&f.data, table, &index, &key) == expected && index == i && key == canonical, "table key canonical pointer and index");
                Check(res_getResource_nid_postfix(&f.data, names[i]) == expected, "root table lookup");
            }
            for (const auto* missing : {"", "aardvark", "betaz", "zzzz"}) {
                const char* key = missing;
                std::int32_t index = -77;
                Check(res_getTableItemByKey_nid_postfix(&f.data, table, &index, &key) == bogus && index == -1 && key == missing, "missing key preserves pointer and writes minus one");
            }
            for (const auto index : {-1, 3, std::numeric_limits<std::int32_t>::max()}) {
                const char* key = names[0];
                Check(res_getTableItemByIndex_nid_postfix(&f.data, table, index, &key) == bogus && key == names[0], "invalid table index preserves key");
            }
        }
    }
    f.Put16(192 * 4, 2);
    f.Put16(192 * 4 + 2, 8);
    f.Put16(192 * 4 + 4, 16);
    f.Put16(192 * 4 + 6, 0xdead);
    f.root[194] = Resource(7, 42);
    f.root[195] = Resource(7, 99);
    Check(res_getTableItemByIndex_nid_postfix(&f.data, Resource(2, 192), 0, nullptr) == Resource(7, 42), "even table count skips padding");
    Check(res_getTableItemByIndex_nid_postfix(&f.data, Resource(2, 192), 1, nullptr) == Resource(7, 99), "even table last value");
    f.units[192] = 0;
    const char* emptyKey = "alpha";
    std::int32_t emptyIndex = 77;
    Check(res_getTableItemByKey_nid_postfix(&f.data, Resource(5, 192), &emptyIndex, &emptyKey) == bogus && emptyIndex == -1, "empty table16 sets index minus one");
    const char* key = "alpha";
    std::int32_t index = 77;
    Check(res_getTableItemByKey_nid_postfix(nullptr, Resource(2), &index, &key) == bogus && index == 77, "empty table preserves index");
    Check(res_getTableItemByKey_nid_postfix(nullptr, Resource(4), &index, &key) == bogus && index == 77, "empty table32 preserves index");
    Check(res_getTableItemByKey_nid_postfix(nullptr, Resource(2, 128), nullptr, nullptr) == bogus, "null key output short circuit");
    key = nullptr;
    Check(res_getTableItemByKey_nid_postfix(nullptr, Resource(2, 128), nullptr, &key) == bogus, "null lookup key short circuit");
}

IcuDataInfo Info(std::uint8_t version, std::uint8_t minor = 0) {
    IcuDataInfo info{};
    info.size = 20;
    info.sizeofUChar = 2;
    std::memcpy(info.dataFormat, "ResB", 4);
    info.formatVersion[0] = version;
    info.formatVersion[1] = minor;
    return info;
}

void CheckRead() {
    std::array<std::int32_t, 32> root{};
    root[0] = Resource(2);
    root[1] = 8;
    root[2] = 12;
    root[3] = 32;
    root[4] = 32;
    root[5] = 0;
    root[6] = 1;
    root[7] = 16;
    IcuResourceData data{};
    for (const auto pair : std::array<std::array<std::uint8_t, 2>, 4>{{{1, 0}, {1, 1}, {2, 0}, {3, 0}}}) {
        auto info = Info(pair[0], pair[1]);
        std::int32_t error = 0;
        res_read_nid_postfix(&data, &info, root.data(), sizeof(root), &error);
        Check(error == 0 && data.pRoot == root.data() && data.rootRes == Resource(2) && data.data == nullptr, "read supported resource format");
        Check(data.useNativeStrcmp == 1, "ASCII comparison flag");
        Check(data.poolBundleKeys == nullptr && data.poolBundleStrings == nullptr, "read leaves pool attachments unset");
        if (pair[0] == 1 && pair[1] == 0) Check(data.localKeyLimit == 65536 && *data.p16BitUnits == 0, "format 1.0 defaults");
        else Check(data.localKeyLimit == 48 && data.p16BitUnits == reinterpret_cast<const std::uint16_t*>(root.data() + 12) && data.noFallback == 1, "indexed resource fields");
    }
    auto info = Info(3);
    root[1] = (0x123456 << 8) | 8;
    root[6] = 0x4567a007;
    std::int32_t error = -1;
    res_read_nid_postfix(&data, &info, root.data(), -1, &error);
    Check(error == -1 && data.poolStringIndexLimit == 0x0a123456 && data.poolStringIndex16Limit == 0x4567, "format3 pool split fields and warning preserved");
    Check(data.noFallback == 1 && data.isPoolBundle == 1 && data.usesPoolBundle == 1, "resource attributes");
    error = 7;
    std::memset(&data, 0x5a, sizeof(data));
    res_read_nid_postfix(&data, nullptr, nullptr, 0, &error);
    const std::array<unsigned char, sizeof(data)> zero{};
    Check(error == 7 && std::memcmp(&data, zero.data(), zero.size()) == 0, "preexisting error clears data before returning");
    root[1] = 8;
    root[6] = 0;
    for (unsigned field = 0; field < 7; ++field) {
        auto invalid = info;
        if (field == 0) invalid.size = 19;
        if (field == 1) invalid.isBigEndian = 1;
        if (field == 2) invalid.charsetFamily = 1;
        if (field == 3) invalid.sizeofUChar = 4;
        if (field == 4) invalid.dataFormat[3] = 'X';
        if (field == 5) invalid.formatVersion[0] = 0;
        if (field == 6) invalid.formatVersion[0] = 4;
        error = 0;
        res_read_nid_postfix(&data, &invalid, root.data(), sizeof(root), &error);
        Check(error == 3, "invalid data header");
    }
    for (const auto length : {0, 1, 2, 3}) {
        error = -1;
        std::memset(&data, 0x5a, sizeof(data));
        bool threw = false;
        try {
            res_read_nid_postfix(&data, &info, root.data(), length, &error);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Check(threw && error == -1 && std::memcmp(&data, zero.data(), zero.size()) == 0, "sub-word input throws after clearing data and preserves error");
    }
    for (const auto length : {4, 20, 35, 127}) {
        error = 0;
        res_read_nid_postfix(&data, &info, root.data(), length, &error);
        Check(error == 3, "truncated bundle length");
    }
    root[0] = Resource(8);
    error = 0;
    res_read_nid_postfix(&data, &info, root.data(), sizeof(root), &error);
    Check(error == 3, "non-table root rejected");
    root[0] = Resource(2);
    root[1] = 4;
    error = 0;
    res_read_nid_postfix(&data, &info, root.data(), sizeof(root), &error);
    Check(error == 3, "index count too small");
    root[1] = 7;
    root[6] = 4;
    error = 0;
    res_read_nid_postfix(&data, &info, root.data(), sizeof(root), &error);
    Check(error == 3, "pool attributes require checksum slot");
    std::array<std::int32_t, 6> legacy{static_cast<std::int32_t>(Resource(2)), 5, -1, 0, 6, 0};
    auto legacyInfo = Info(1, 1);
    error = 0;
    res_read_nid_postfix(&data, &legacyInfo, legacy.data(), sizeof(legacy), &error);
    Check(error == 0 && data.localKeyLimit == 0 && data.pRoot == legacy.data(), "negative unused keys top is ignored as upstream");
    for (const auto top : {-1, std::numeric_limits<std::int32_t>::max()}) {
        legacy[4] = top;
        error = 0;
        res_read_nid_postfix(&data, &legacyInfo, legacy.data(), -1, &error);
        Check(error == 0 && data.pRoot == legacy.data(), "unknown length ignores bundle top");
    }
    std::array<std::int32_t, 4> negativeTable{0, -1, 0, 0};
    data.pRoot = negativeTable.data();
    const char* key = "missing";
    const char* originalKey = key;
    std::int32_t index = 42;
    Check(res_countArrayItems_nid_postfix(&data, Resource(4, 1)) == -1, "negative table count is returned unchanged");
    Check(res_getTableItemByIndex_nid_postfix(&data, Resource(4, 1), 0, &key) == bogus && key == originalKey, "negative table count rejects index without output");
    Check(res_getTableItemByKey_nid_postfix(&data, Resource(4, 1), &index, &key) == bogus && index == -1 && key == originalKey, "negative table count search reports missing");
}

} // namespace

int main() {
    CheckLeaves();
    CheckStrings();
    CheckCountsAndArrays();
    CheckTables();
    CheckRead();
    std::puts("ICU resource readers tests passed");
}
