#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu resource utf8: " + message);
}

constexpr std::size_t BundleSize = 200;
constexpr std::size_t ResourceOffset = 172;
constexpr std::size_t SizeOffset = 196;

void StoreUInt16(std::uint8_t* bundle, std::size_t offset, std::uint16_t value) {
    std::memcpy(bundle + offset, &value, sizeof(value));
}

void StoreUInt32(std::uint8_t* bundle, std::size_t offset, std::uint32_t value) {
    std::memcpy(bundle + offset, &value, sizeof(value));
}

void StoreInt32(std::uint8_t* bundle, std::size_t offset, std::int32_t value) {
    std::memcpy(bundle + offset, &value, sizeof(value));
}

}

int main() {
    alignas(8) std::uint8_t pool[160] = {};
    const std::uint8_t* root = pool;
    StoreInt32(pool, 8, 2);
    StoreUInt16(pool, 12, u'h');
    StoreUInt16(pool, 14, u'i');
    StoreInt32(pool, 24, 1);
    StoreUInt16(pool, 28, u'a');
    StoreInt32(pool, 32, 1);
    StoreUInt16(pool, 36, u'b');
    StoreInt32(pool, 40, 0);
    StoreInt32(pool, 48, 2);
    StoreUInt32(pool, 52, 6u);
    StoreUInt32(pool, 56, 8u);
    StoreUInt16(pool, 64, 2);
    StoreUInt16(pool, 66, 96);
    StoreUInt16(pool, 68, 98);
    StoreUInt32(pool, 72, 6u);
    StoreUInt32(pool, 76, 8u);
    pool[96] = 'a';
    pool[97] = 0;
    pool[98] = 'b';
    pool[99] = 0;
    StoreInt32(pool, 104, 2);
    StoreUInt16(pool, 108, 0x00ff);
    StoreUInt16(pool, 110, 0x00ff);

    alignas(8) std::uint8_t stringBundle[BundleSize] = {};
    std::memcpy(stringBundle + 48, &root, sizeof(root));
    StoreUInt32(stringBundle, ResourceOffset, 2u);
    StoreInt32(stringBundle, SizeOffset, 1);
    auto* stringResource = reinterpret_cast<UResourceBundle*>(stringBundle);

    alignas(8) std::uint8_t emptyBundle[BundleSize] = {};
    std::memcpy(emptyBundle + 48, &root, sizeof(root));
    StoreUInt32(emptyBundle, ResourceOffset, 10u);
    StoreInt32(emptyBundle, SizeOffset, 1);
    auto* emptyResource = reinterpret_cast<UResourceBundle*>(emptyBundle);

    alignas(8) std::uint8_t arrayBundle[BundleSize] = {};
    std::memcpy(arrayBundle + 48, &root, sizeof(root));
    StoreUInt32(arrayBundle, ResourceOffset, (8u << 28) | 12u);
    StoreInt32(arrayBundle, SizeOffset, 2);
    auto* arrayResource = reinterpret_cast<UResourceBundle*>(arrayBundle);

    alignas(8) std::uint8_t tableBundle[BundleSize] = {};
    std::memcpy(tableBundle + 48, &root, sizeof(root));
    StoreUInt32(tableBundle, ResourceOffset, (2u << 28) | 16u);
    StoreInt32(tableBundle, SizeOffset, 2);
    auto* tableResource = reinterpret_cast<UResourceBundle*>(tableBundle);

    alignas(8) std::uint8_t multibyteBundle[BundleSize] = {};
    std::memcpy(multibyteBundle + 48, &root, sizeof(root));
    StoreUInt32(multibyteBundle, ResourceOffset, 26u);
    StoreInt32(multibyteBundle, SizeOffset, 1);
    auto* multibyteResource = reinterpret_cast<UResourceBundle*>(multibyteBundle);

    std::int32_t status = 0;
    std::int32_t length = 8;
    char destination[8] = {};
    const char* result = ures_getUTF8String_nid_postfix(stringResource, destination, &length, 1, &status);
    Require(status == 0 && result == destination && length == 2 && std::memcmp(destination, "hi", 2) == 0,
        "ures_getUTF8String did not return the string");
    Require(destination[2] == 0, "ures_getUTF8String did not terminate the output");

    status = 0;
    length = 0;
    result = ures_getUTF8String_nid_postfix(stringResource, nullptr, &length, 1, &status);
    Require(result == nullptr && status == 15 && length == 2, "ures_getUTF8String did not preflight");

    status = 0;
    length = 16;
    char wide[16] = {};
    result = ures_getUTF8String_nid_postfix(stringResource, wide, &length, 0, &status);
    Require(status == 0 && result == wide + 9 && length == 2 && std::memcmp(result, "hi", 2) == 0,
        "ures_getUTF8String did not place the string at the tail");
    Require(wide[11] == 0, "ures_getUTF8String did not terminate the tail-placed string");

    status = 0;
    length = 2;
    char exact[2] = {};
    result = ures_getUTF8String_nid_postfix(stringResource, exact, &length, 1, &status);
    Require(result == exact && status == -124 && length == 2 && std::memcmp(exact, "hi", 2) == 0,
        "ures_getUTF8String did not report the unterminated buffer");

    status = 0;
    length = 2;
    char tooSmall[4] = {'z', 'z', 'z', 'z'};
    result = ures_getUTF8String_nid_postfix(multibyteResource, tooSmall, &length, 1, &status);
    Require(result == tooSmall && status == 15 && length == 4 && tooSmall[0] == 'z' && tooSmall[3] == 'z',
        "ures_getUTF8String wrote into a buffer it could not fill");

    status = 0;
    length = 0;
    result = ures_getUTF8String_nid_postfix(emptyResource, nullptr, &length, 1, &status);
    Require(result == nullptr && status == -124 && length == 0,
        "ures_getUTF8String did not report the unterminated empty string");

    status = 0;
    length = 4;
    char empty[4] = {'x', 'x', 'x', 'x'};
    result = ures_getUTF8String_nid_postfix(emptyResource, empty, &length, 1, &status);
    Require(result == empty && status == 0 && length == 0 && empty[0] == 0,
        "ures_getUTF8String did not terminate the empty string");

    status = 0;
    length = 4;
    result = ures_getUTF8String_nid_postfix(emptyResource, empty, &length, 0, &status);
    Require(result != nullptr && *result == 0 && status == 0 && length == 0,
        "ures_getUTF8String did not return the empty string");

    status = 0;
    length = 8;
    result = ures_getUTF8String_nid_postfix(arrayResource, destination, &length, 1, &status);
    Require(result == nullptr && status == 17, "ures_getUTF8String accepted an array resource");

    status = 0;
    result = ures_getUTF8String_nid_postfix(nullptr, destination, &length, 1, &status);
    Require(result == nullptr && status == 1, "ures_getUTF8String accepted a null bundle");

    status = 0;
    length = -1;
    result = ures_getUTF8String_nid_postfix(stringResource, destination, &length, 1, &status);
    Require(result == nullptr && status == 1, "ures_getUTF8String accepted a negative capacity");

    result = ures_getUTF8String_nid_postfix(stringResource, destination, &length, 1, nullptr);
    Require(result == nullptr, "ures_getUTF8String accepted a null status");

    status = 0;
    length = 8;
    char indexed[8] = {};
    result = ures_getUTF8StringByIndex_nid_postfix(arrayResource, 0, indexed, &length, 1, &status);
    Require(result == indexed && status == 0 && length == 1 && indexed[0] == 'a',
        "ures_getUTF8StringByIndex did not return the first array string");

    status = 0;
    length = 8;
    result = ures_getUTF8StringByIndex_nid_postfix(arrayResource, 1, indexed, &length, 1, &status);
    Require(result == indexed && status == 0 && length == 1 && indexed[0] == 'b',
        "ures_getUTF8StringByIndex did not return the second array string");

    status = 0;
    length = 8;
    result = ures_getUTF8StringByIndex_nid_postfix(arrayResource, 2, indexed, &length, 1, &status);
    Require(result == nullptr && status == 2, "ures_getUTF8StringByIndex accepted an index past the end");

    status = 0;
    length = 8;
    result = ures_getUTF8StringByIndex_nid_postfix(arrayResource, -1, indexed, &length, 1, &status);
    Require(result == nullptr && status == 2, "ures_getUTF8StringByIndex accepted a negative index");

    status = 0;
    length = 8;
    result = ures_getUTF8StringByIndex_nid_postfix(stringResource, 0, indexed, &length, 1, &status);
    Require(result == indexed && status == 0 && length == 2 && std::memcmp(indexed, "hi", 2) == 0,
        "ures_getUTF8StringByIndex did not return a top-level string");

    status = 0;
    length = 8;
    result = ures_getUTF8StringByIndex_nid_postfix(tableResource, 1, indexed, &length, 1, &status);
    Require(result == indexed && status == 0 && length == 1 && indexed[0] == 'b',
        "ures_getUTF8StringByIndex did not return the table value");

    status = 0;
    length = 8;
    char keyed[8] = {};
    result = ures_getUTF8StringByKey_nid_postfix(tableResource, "b", keyed, &length, 1, &status);
    Require(result == keyed && status == 0 && length == 1 && keyed[0] == 'b',
        "ures_getUTF8StringByKey did not return the keyed string");

    status = 0;
    length = 8;
    result = ures_getUTF8StringByKey_nid_postfix(tableResource, "z", keyed, &length, 1, &status);
    Require(result == nullptr && status == 2, "ures_getUTF8StringByKey accepted a missing key");

    status = 0;
    length = 8;
    result = ures_getUTF8StringByKey_nid_postfix(arrayResource, "a", keyed, &length, 1, &status);
    Require(result == nullptr && status == 17, "ures_getUTF8StringByKey accepted an array");

    status = 0;
    length = 8;
    result = ures_getUTF8StringByKey_nid_postfix(tableResource, nullptr, keyed, &length, 1, &status);
    Require(result == nullptr && status == 2, "ures_getUTF8StringByKey accepted a null key");

    return 0;
}
