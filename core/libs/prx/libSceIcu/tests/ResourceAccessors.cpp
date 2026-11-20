#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu resource: " + message);
}

constexpr std::size_t BundleSize = 200;
constexpr std::size_t KeyOffset = 0;
constexpr std::size_t ResourceOffset = 172;
constexpr std::size_t IndexOffset = 192;
constexpr std::size_t SizeOffset = 196;

void StoreUInt32(std::uint8_t* bundle, std::size_t offset, std::uint32_t value) {
    std::memcpy(bundle + offset, &value, sizeof(value));
}

void StoreInt32(std::uint8_t* bundle, std::size_t offset, std::int32_t value) {
    std::memcpy(bundle + offset, &value, sizeof(value));
}

void StoreUInt16(std::uint8_t* bundle, std::size_t offset, std::uint16_t value) {
    std::memcpy(bundle + offset, &value, sizeof(value));
}

}

int main() {
    static const char key[] = "greeting";
    alignas(8) std::uint8_t bundle[BundleSize] = {};
    const char* keyPointer = key;
    std::memcpy(bundle + KeyOffset, &keyPointer, sizeof(keyPointer));
    StoreUInt32(bundle, ResourceOffset, (7u << 28) | 42u);
    StoreInt32(bundle, IndexOffset, 1);
    StoreInt32(bundle, SizeOffset, 3);

    auto* resource = reinterpret_cast<UResourceBundle*>(bundle);
    Require(ures_getKey_nid_postfix(resource) == key, "ures_getKey did not return the key");
    Require(ures_getType_nid_postfix(resource) == 7, "ures_getType did not report an integer");
    Require(ures_getSize_nid_postfix(resource) == 3, "ures_getSize did not report the size");
    Require(ures_hasNext_nid_postfix(resource) == 1, "ures_hasNext did not find a following item");

    StoreInt32(bundle, IndexOffset, 2);
    Require(ures_hasNext_nid_postfix(resource) == 0, "ures_hasNext found an item past the end");
    StoreInt32(bundle, IndexOffset, 0);
    Require(ures_hasNext_nid_postfix(resource) == 1, "ures_hasNext rejected the first item");

    StoreUInt32(bundle, ResourceOffset, 0xF0000005u);
    Require(ures_getType_nid_postfix(resource) == -1, "ures_getType did not report a missing resource");

    StoreUInt32(bundle, ResourceOffset, (0u << 28) | 0u);
    Require(ures_getType_nid_postfix(resource) == 0, "ures_getType did not report a string");

    Require(ures_getKey_nid_postfix(nullptr) == nullptr, "ures_getKey accepted a null bundle");
    Require(ures_getType_nid_postfix(nullptr) == -1, "ures_getType accepted a null bundle");
    Require(ures_getSize_nid_postfix(nullptr) == 0, "ures_getSize accepted a null bundle");
    {
        std::int32_t intStatus = 0;
        StoreUInt32(bundle, ResourceOffset, (7u << 28) | 42u);
        Require(ures_getInt_nid_postfix(resource, &intStatus) == 42, "ures_getInt did not return the value");
        Require(intStatus == 0, "ures_getInt changed the status on success");
        StoreUInt32(bundle, ResourceOffset, 0u);
        intStatus = 0;
        Require(ures_getInt_nid_postfix(resource, &intStatus) == 0, "ures_getInt accepted a string resource");
        Require(intStatus == 17, "ures_getInt did not report a type mismatch");
        intStatus = 0;
        Require(ures_getInt_nid_postfix(nullptr, &intStatus) == 0, "ures_getInt accepted a null bundle");
        Require(intStatus == 1, "ures_getInt did not report an illegal argument");
    }
    {
        alignas(8) std::uint8_t vectorBundle[BundleSize] = {};
        alignas(8) std::int32_t pool[3] = {2, 10, 20};
        const std::int32_t* poolPointer = pool;
        std::memcpy(vectorBundle + 48, &poolPointer, sizeof(poolPointer));
        StoreUInt32(vectorBundle, ResourceOffset, 14u << 28);
        auto* vectorResource = reinterpret_cast<UResourceBundle*>(vectorBundle);
        std::int32_t vectorStatus = 0;
        std::int32_t vectorLength = -1;
        const std::int32_t* vector = ures_getIntVector_nid_postfix(vectorResource, &vectorLength, &vectorStatus);
        Require(vectorStatus == 0 && vector != nullptr, "ures_getIntVector returned no vector");
        Require(vectorLength == 2, "ures_getIntVector reported the wrong length");
        Require(vector[0] == 10 && vector[1] == 20, "ures_getIntVector returned the wrong values");
        StoreUInt32(vectorBundle, ResourceOffset, 7u << 28);
        vectorStatus = 0;
        Require(ures_getIntVector_nid_postfix(vectorResource, &vectorLength, &vectorStatus) == nullptr,
            "ures_getIntVector accepted an integer resource");
        Require(vectorStatus == 17, "ures_getIntVector did not report a type mismatch");
        vectorStatus = 0;
        Require(ures_getIntVector_nid_postfix(nullptr, &vectorLength, &vectorStatus) == nullptr,
            "ures_getIntVector accepted a null bundle");
    }
    {
        alignas(8) std::uint8_t binaryBundle[BundleSize] = {};
        alignas(8) std::uint8_t binaryPool[8] = {};
        const std::int32_t binaryLengthValue = 3;
        std::memcpy(binaryPool, &binaryLengthValue, sizeof(binaryLengthValue));
        binaryPool[4] = 'a';
        binaryPool[5] = 'b';
        binaryPool[6] = 'c';
        const std::uint8_t* binaryPoolPointer = binaryPool;
        std::memcpy(binaryBundle + 48, &binaryPoolPointer, sizeof(binaryPoolPointer));
        StoreUInt32(binaryBundle, ResourceOffset, 1u << 28);
        auto* binaryResource = reinterpret_cast<UResourceBundle*>(binaryBundle);
        std::int32_t binaryStatus = 0;
        std::int32_t binaryLength = -1;
        const std::uint8_t* binary = ures_getBinary_nid_postfix(binaryResource, &binaryLength, &binaryStatus);
        Require(binaryStatus == 0 && binary != nullptr, "ures_getBinary returned no binary");
        Require(binaryLength == 3, "ures_getBinary reported the wrong length");
        Require(binary[0] == 'a' && binary[1] == 'b' && binary[2] == 'c', "ures_getBinary returned the wrong bytes");
        StoreUInt32(binaryBundle, ResourceOffset, 7u << 28);
        binaryStatus = 0;
        Require(ures_getBinary_nid_postfix(binaryResource, &binaryLength, &binaryStatus) == nullptr,
            "ures_getBinary accepted an integer resource");
        Require(binaryStatus == 17, "ures_getBinary did not report a type mismatch");
    }
    {
        static const char actualLocale[] = "en-US";
        static const char validLocale[] = "en";
        alignas(8) std::uint8_t entryActual[16] = {};
        alignas(8) std::uint8_t entryValid[16] = {};
        const char* actualPointer = actualLocale;
        const char* validPointer = validLocale;
        std::memcpy(entryActual, &actualPointer, sizeof(actualPointer));
        std::memcpy(entryValid, &validPointer, sizeof(validPointer));

        alignas(8) std::uint8_t localeBundle[BundleSize] = {};
        const std::uint8_t* actualEntry = entryActual;
        const std::uint8_t* validEntry = entryValid;
        std::memcpy(localeBundle + 8, &actualEntry, sizeof(actualEntry));
        std::memcpy(localeBundle + 24, &validEntry, sizeof(validEntry));
        auto* localeResource = reinterpret_cast<UResourceBundle*>(localeBundle);

        std::int32_t localeStatus = 0;
        const char* actual = ures_getLocaleByType_nid_postfix(localeResource, 0, &localeStatus);
        Require(localeStatus == 0 && actual != nullptr && std::strcmp(actual, "en-US") == 0,
            "ures_getLocaleByType did not return the actual locale");
        localeStatus = 0;
        const char* valid = ures_getLocaleByType_nid_postfix(localeResource, 1, &localeStatus);
        Require(localeStatus == 0 && valid != nullptr && std::strcmp(valid, "en") == 0,
            "ures_getLocaleByType did not return the valid locale");
        localeStatus = 0;
        Require(ures_getLocaleByType_nid_postfix(localeResource, 2, &localeStatus) == nullptr,
            "ures_getLocaleByType accepted an invalid locale type");
        Require(localeStatus == 1, "ures_getLocaleByType did not report an illegal argument");
    }

    Require(ures_hasNext_nid_postfix(nullptr) == 0, "ures_hasNext accepted a null bundle");

    {
        alignas(8) std::uint8_t stringBundle[BundleSize] = {};
        alignas(8) std::uint8_t pool[16] = {};
        const std::int32_t unitCount = 3;
        std::memcpy(pool, &unitCount, sizeof(unitCount));
        const char16_t units[] = {u'h', u'i', 0};
        std::memcpy(pool + sizeof(unitCount), units, sizeof(units));
        const std::uint8_t* poolPointer = pool;
        std::memcpy(stringBundle + 48, &poolPointer, sizeof(poolPointer));
        StoreUInt32(stringBundle, ResourceOffset, 0u);
        auto* stringResource = reinterpret_cast<UResourceBundle*>(stringBundle);
        std::int32_t status = 0;
        std::int32_t stringLength = -1;
        const char16_t* string = ures_getString_nid_postfix(stringResource, &stringLength, &status);
        Require(status == 0 && string != nullptr, "ures_getString returned no string");
        Require(stringLength == 3, "ures_getString reported the wrong length");
        Require(reinterpret_cast<const std::uint8_t*>(string) == pool + sizeof(unitCount),
            "ures_getString returned a pointer that is not past the length prefix");
        Require(std::memcmp(string, units, sizeof(units)) == 0, "ures_getString returned the wrong units");

        StoreUInt32(stringBundle, ResourceOffset, 7u << 28);
        status = 0;
        Require(ures_getString_nid_postfix(stringResource, &stringLength, &status) == nullptr,
            "ures_getString accepted an integer resource");
        Require(status == 17, "ures_getString did not report a type mismatch");

        status = 0;
        Require(ures_getString_nid_postfix(nullptr, &stringLength, &status) == nullptr,
            "ures_getString accepted a null bundle");
        Require(status == 1, "ures_getString did not report an illegal argument");
    }
    {
        alignas(8) std::uint8_t tableBundle[BundleSize] = {};
        alignas(8) std::uint8_t tablePool[128] = {};
        const std::uint8_t* tableRoot = tablePool;
        std::memcpy(tableBundle + 48, &tableRoot, sizeof(tableRoot));
        StoreUInt16(tablePool, 4, 2);
        StoreUInt16(tablePool, 6, 24);
        StoreUInt16(tablePool, 8, 26);
        StoreUInt32(tablePool, 12, (7u << 28) | 7u);
        StoreUInt32(tablePool, 16, 8u);
        StoreInt32(tablePool, 32, 2);
        tablePool[24] = 'a';
        tablePool[25] = 0;
        tablePool[26] = 'b';
        tablePool[27] = 0;
        tablePool[36] = 'h';
        tablePool[38] = 'i';
        tablePool[40] = 0;
        StoreUInt32(tableBundle, ResourceOffset, (2u << 28) | 1u);
        StoreInt32(tableBundle, IndexOffset, 0);
        StoreInt32(tableBundle, SizeOffset, 2);
        auto* tableResource = reinterpret_cast<UResourceBundle*>(tableBundle);
        alignas(8) std::uint8_t tableFill[BundleSize] = {};
        auto* tableChild = reinterpret_cast<UResourceBundle*>(tableFill);

        std::int32_t tableStatus = 0;
        Require(ures_getByKey_nid_postfix(tableResource, "a", tableChild, &tableStatus) == tableChild,
            "ures_getByKey returned no bundle");
        Require(tableStatus == 0, "ures_getByKey changed the status on success");
        Require(ures_getKey_nid_postfix(tableChild) == reinterpret_cast<const char*>(tablePool + 24),
            "ures_getByKey returned the wrong key");
        Require(ures_getInt_nid_postfix(tableChild, &tableStatus) == 7, "ures_getByKey returned the wrong integer");
        Require(tableStatus == 0, "ures_getByKey left a failing status");

        tableStatus = 0;
        Require(ures_getByKey_nid_postfix(tableResource, "b", tableChild, &tableStatus) == tableChild,
            "ures_getByKey did not find the string entry");
        Require(ures_getSize_nid_postfix(tableChild) == 1, "ures_getByKey reported the wrong string size");
        std::int32_t tableLength = -1;
        const char16_t* tableString = ures_getString_nid_postfix(tableChild, &tableLength, &tableStatus);
        Require(tableStatus == 0 && tableString != nullptr, "ures_getByKey returned no string");
        Require(tableLength == 2 && tableString[0] == u'h' && tableString[1] == u'i',
            "ures_getByKey returned the wrong string");

        tableStatus = 0;
        std::int32_t byKeyLength = -1;
        const char16_t* byKeyString = ures_getStringByKey_nid_postfix(tableResource, "b", &byKeyLength, &tableStatus);
        Require(tableStatus == 0 && byKeyString != nullptr, "ures_getStringByKey returned no string");
        Require(byKeyLength == 2, "ures_getStringByKey reported the wrong length");
        Require(byKeyString[0] == u'h', "ures_getStringByKey returned the wrong first unit");
        Require(byKeyString[1] == u'i', "ures_getStringByKey returned the wrong second unit");

        tableStatus = 0;
        byKeyLength = -1;
        Require(ures_getStringByKey_nid_postfix(tableResource, "a", &byKeyLength, &tableStatus) == nullptr,
            "ures_getStringByKey accepted an integer entry");
        Require(tableStatus == 17, "ures_getStringByKey did not report a type mismatch");

        tableStatus = 0;
        Require(ures_getStringByKey_nid_postfix(tableResource, "z", &byKeyLength, &tableStatus) == nullptr,
            "ures_getStringByKey accepted a missing key");
        Require(tableStatus == 2, "ures_getStringByKey did not report a missing resource");

        Require(ures_getStringByKey_nid_postfix(tableResource, "b", &byKeyLength, nullptr) == nullptr,
            "ures_getStringByKey accepted a null status");

        tableStatus = 0;
        Require(ures_getByKey_nid_postfix(tableResource, "z", tableChild, &tableStatus) == nullptr,
            "ures_getByKey accepted a missing key");
        Require(tableStatus == 2, "ures_getByKey did not report a missing resource");

        tableStatus = 0;
        Require(ures_getByKey_nid_postfix(tableResource, "a", nullptr, &tableStatus) == nullptr,
            "ures_getByKey accepted a null fill-in");
        Require(tableStatus == 1, "ures_getByKey did not report a null fill-in");

        tableStatus = 0;
        Require(ures_getByIndex_nid_postfix(tableResource, 0, tableChild, &tableStatus) == nullptr,
            "ures_getByIndex accepted a table");
        Require(tableStatus == 17, "ures_getByIndex did not report a table type mismatch");
    }
    {
        alignas(8) std::uint8_t arrayBundle[BundleSize] = {};
        alignas(8) std::uint8_t arrayPool[64] = {};
        const std::uint8_t* arrayRoot = arrayPool;
        std::memcpy(arrayBundle + 48, &arrayRoot, sizeof(arrayRoot));
        StoreInt32(arrayPool, 16, 3);
        StoreUInt32(arrayPool, 20, (7u << 28) | 11u);
        StoreUInt32(arrayPool, 24, (7u << 28) | 22u);
        StoreUInt32(arrayPool, 28, (7u << 28) | 33u);
        StoreUInt32(arrayBundle, ResourceOffset, (8u << 28) | 4u);
        StoreInt32(arrayBundle, IndexOffset, 0);
        StoreInt32(arrayBundle, SizeOffset, 3);
        auto* arrayResource = reinterpret_cast<UResourceBundle*>(arrayBundle);
        alignas(8) std::uint8_t arrayFill[BundleSize] = {};
        auto* arrayChild = reinterpret_cast<UResourceBundle*>(arrayFill);

        std::int32_t arrayStatus = 0;
        Require(ures_getByIndex_nid_postfix(arrayResource, 0, arrayChild, &arrayStatus) == arrayChild,
            "ures_getByIndex returned no bundle");
        Require(arrayStatus == 0, "ures_getByIndex changed the status on success");
        Require(ures_getKey_nid_postfix(arrayChild) == nullptr, "ures_getByIndex returned a key");
        Require(ures_getSize_nid_postfix(arrayChild) == 1, "ures_getByIndex reported the wrong child size");
        Require(ures_getInt_nid_postfix(arrayChild, &arrayStatus) == 11, "ures_getByIndex returned the wrong first value");

        arrayStatus = 0;
        Require(ures_getByIndex_nid_postfix(arrayResource, 1, arrayChild, &arrayStatus) == arrayChild,
            "ures_getByIndex rejected the second index");
        Require(ures_getInt_nid_postfix(arrayChild, &arrayStatus) == 22, "ures_getByIndex returned the wrong second value");

        arrayStatus = 0;
        Require(ures_getByIndex_nid_postfix(arrayResource, 2, arrayChild, &arrayStatus) == arrayChild,
            "ures_getByIndex rejected the third index");
        Require(ures_getInt_nid_postfix(arrayChild, &arrayStatus) == 33, "ures_getByIndex returned the wrong third value");

        arrayStatus = 0;
        Require(ures_getByIndex_nid_postfix(arrayResource, 3, arrayChild, &arrayStatus) == nullptr,
            "ures_getByIndex accepted an index past the end");
        Require(arrayStatus == 2, "ures_getByIndex did not report a missing resource");

        arrayStatus = 0;
        Require(ures_getByIndex_nid_postfix(arrayResource, -1, arrayChild, &arrayStatus) == nullptr,
            "ures_getByIndex accepted a negative index");
        Require(arrayStatus == 2, "ures_getByIndex did not report a negative index");

        arrayStatus = 0;
        Require(ures_getByIndex_nid_postfix(arrayResource, 0, nullptr, &arrayStatus) == nullptr,
            "ures_getByIndex accepted a null fill-in");
        Require(arrayStatus == 1, "ures_getByIndex did not report a null fill-in");
    }

    {
        alignas(8) std::uint8_t cappedBundle[BundleSize] = {};
        alignas(8) std::uint8_t cappedPool[512] = {};
        const std::uint8_t* cappedRoot = cappedPool;
        std::memcpy(cappedBundle + 48, &cappedRoot, sizeof(cappedRoot));
        StoreUInt16(cappedPool, 4, 1);
        StoreUInt16(cappedPool, 6, 16);
        StoreUInt32(cappedPool, 8, (7u << 28) | 5u);
        std::memset(cappedPool + 16, 'x', 300);
        cappedPool[316] = 0;
        StoreUInt32(cappedBundle, ResourceOffset, (2u << 28) | 1u);
        StoreInt32(cappedBundle, SizeOffset, 1);
        auto* cappedResource = reinterpret_cast<UResourceBundle*>(cappedBundle);
        alignas(8) std::uint8_t cappedFill[BundleSize] = {};
        auto* cappedChild = reinterpret_cast<UResourceBundle*>(cappedFill);

        std::int32_t cappedStatus = 0;
        const std::string longKey(300, 'y');
        Require(ures_getByKey_nid_postfix(cappedResource, longKey.c_str(), cappedChild, &cappedStatus) == nullptr,
            "a 300-character key was accepted");
        Require(cappedStatus == 1, "a 300-character key did not report an illegal argument");

        cappedStatus = 0;
        Require(ures_getByKey_nid_postfix(cappedResource, "short", cappedChild, &cappedStatus) == nullptr,
            "a pool key with no NUL inside the cap was matched");
        Require(cappedStatus == 2, "a pool key with no NUL inside the cap did not report a missing resource");
    }
    return 0;
}
