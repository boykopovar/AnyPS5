#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <type_traits>

namespace {

struct ResourceData {
    void* data = reinterpret_cast<void*>(1);
    const std::int32_t* root = nullptr;
    const std::uint16_t* units16 = nullptr;
    const char* poolKeys = nullptr;
    std::uint32_t rootResource = 0;
    std::int32_t localKeyLimit = 0;
    const std::uint16_t* poolStrings = nullptr;
    std::int32_t poolStringIndexLimit = 0;
    std::int32_t poolStringIndex16Limit = 0;
    std::int8_t noFallback = 0;
    std::int8_t isPoolBundle = 0;
    std::int8_t usesPoolBundle = 0;
    std::int8_t useNativeCompare = 1;
};

struct ResourceEntry {
    char* name = nullptr;
    char* path = nullptr;
    ResourceEntry* parent = nullptr;
    ResourceEntry* alias = nullptr;
    ResourceEntry* pool = nullptr;
    ResourceData resourceData{};
    std::array<char, 3> nameBuffer{};
    std::uint32_t countExisting = 0;
    std::int32_t bogus = 0;
};

struct ResourceBundle {
    const char* key = nullptr;
    ResourceEntry* data = nullptr;
    char* version = nullptr;
    ResourceEntry* topLevelData = nullptr;
    char* resourcePath = nullptr;
    ResourceData resourceData{};
    std::array<char, 64> resourceBuffer{};
    std::int32_t resourcePathLength = 0;
    std::uint32_t resource = 0;
    std::int8_t hasFallback = 0;
    std::int8_t isTopLevel = 0;
    std::uint16_t padding = 0;
    std::uint32_t magic1 = 0;
    std::uint32_t magic2 = 0;
    std::int32_t index = 0;
    std::int32_t size = 0;
};

static_assert(sizeof(ResourceData) == 64);
static_assert(sizeof(ResourceEntry) == 120);
static_assert(sizeof(ResourceBundle) == 200);
static_assert(std::is_same_v<decltype(&ures_countArrayItems_nid_postfix),
    std::int32_t (APS5_VABI*)(const UResourceBundle*, const char*, std::int32_t*)>);

[[noreturn]] void Fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void Require(bool condition, const char* message) {
    if (!condition) Fail(message);
}

template <typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Fail(message);
}

void PutKey(std::array<std::int32_t, 96>& root, std::size_t offset, const char* key) {
    std::memcpy(reinterpret_cast<char*>(root.data()) + offset, key, std::strlen(key) + 1);
}

ResourceBundle Table32Bundle(std::array<std::int32_t, 96>& root) {
    root.fill(0);
    PutKey(root, 0, "alpha");
    PutKey(root, 8, "array");
    PutKey(root, 16, "unknown");
    root[16] = 3;
    root[17] = 0;
    root[18] = 8;
    root[19] = 16;
    root[20] = 0;
    root[21] = static_cast<std::int32_t>(0x80000028);
    root[22] = static_cast<std::int32_t>(0xa0000000);
    root[40] = 4;
    ResourceBundle bundle{};
    bundle.resourceData.root = root.data();
    bundle.resourceData.localKeyLimit = 32;
    bundle.resource = 0x40000010;
    return bundle;
}

void BasicContracts() {
    std::int32_t status = 0;
    Require(ures_countArrayItems_nid_postfix(nullptr, nullptr, nullptr) == 0, "null status");
    status = 7;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(1), reinterpret_cast<const char*>(1), &status) == 0,
        "failed status result");
    Require(status == 7, "failed status changed");
    status = 0;
    Require(ures_countArrayItems_nid_postfix(nullptr, nullptr, &status) == 0, "null bundle result");
    Require(status == 1, "null bundle status");

    ResourceBundle scalar{};
    scalar.resource = 0;
    status = -126;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&scalar), reinterpret_cast<const char*>(1), &status) == 0,
        "non-table result");
    Require(status == 17, "non-table status");
}

void Table32AndCounts() {
    std::array<std::int32_t, 96> root{};
    ResourceBundle bundle = Table32Bundle(root);
    std::int32_t status = 0;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "alpha", &status) == 1,
        "table32 scalar count");
    Require(status == 0, "table32 scalar status");
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "array", &status) == 4,
        "table32 array count");
    Require(status == 0, "table32 array status");
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "unknown", &status) == 0,
        "unknown resource count");
    Require(status == 0, "unknown resource status");
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "missing", &status) == 0,
        "missing count");
    Require(status == 2, "missing status");
    status = 0;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), nullptr, &status) == 0,
        "null key count");
    Require(status == 2, "null key status");

    bundle.resourceData.data = nullptr;
    status = 0;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "alpha", &status) == 0,
        "null data count");
    Require(status == 2, "null data status");
}

void AllResourceTypes() {
    std::array<std::int32_t, 96> root{};
    ResourceBundle bundle = Table32Bundle(root);
    std::array<std::uint16_t, 32> units16{};
    units16[4] = 5;
    bundle.resourceData.units16 = units16.data();
    std::int32_t status = 0;

    const std::array<std::uint32_t, 5> scalars{
        0x00000000, 0x10000000, 0x60000000, 0x70000001, 0xe0000020};
    for (const auto resource : scalars) {
        root[20] = static_cast<std::int32_t>(resource);
        Require(ures_countArrayItems_nid_postfix(
            reinterpret_cast<const UResourceBundle*>(&bundle), "alpha", &status) == 1,
            "scalar resource count");
        Require(status == 0, "scalar resource status");
    }

    root[40] = 4;
    const std::array<std::uint32_t, 3> wideContainers{
        0x20000028, 0x40000028, 0x80000028};
    for (const auto resource : wideContainers) {
        root[20] = static_cast<std::int32_t>(resource);
        Require(ures_countArrayItems_nid_postfix(
            reinterpret_cast<const UResourceBundle*>(&bundle), "alpha", &status) == 4,
            "wide container count");
        Require(status == 0, "wide container status");
    }

    for (const auto resource : {0x50000004U, 0x90000004U}) {
        root[20] = static_cast<std::int32_t>(resource);
        Require(ures_countArrayItems_nid_postfix(
            reinterpret_cast<const UResourceBundle*>(&bundle), "alpha", &status) == 5,
            "sixteen-bit container count");
        Require(status == 0, "sixteen-bit container status");
    }

    for (const auto resource : {0x20000000U, 0x40000000U, 0x80000000U}) {
        root[20] = static_cast<std::int32_t>(resource);
        Require(ures_countArrayItems_nid_postfix(
            reinterpret_cast<const UResourceBundle*>(&bundle), "alpha", &status) == 0,
            "empty container count");
        Require(status == 0, "empty container status");
    }

    root[20] = static_cast<std::int32_t>(0xa0000000);
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "alpha", &status) == 0,
        "reserved resource count");
    Require(status == 0, "reserved resource status");
}

void TableAndTable16() {
    alignas(4) std::array<std::int32_t, 96> root{};
    root.fill(0);
    PutKey(root, 0, "one");
    PutKey(root, 8, "three");
    auto* units = reinterpret_cast<std::uint16_t*>(root.data());
    const std::size_t tableUnit = 24;
    units[tableUnit] = 2;
    units[tableUnit + 1] = 0;
    units[tableUnit + 2] = 8;
    auto* values = reinterpret_cast<std::uint32_t*>(&units[tableUnit + 4]);
    values[0] = 0;
    values[1] = 0x80000020;
    root[32] = 3;

    ResourceBundle bundle{};
    bundle.resourceData.root = root.data();
    bundle.resourceData.localKeyLimit = 24;
    bundle.resource = 0x2000000c;
    std::int32_t status = 0;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "three", &status) == 3,
        "table count");
    Require(status == 0, "table status");

    std::array<std::uint16_t, 32> table16{};
    table16[4] = 1;
    table16[5] = 0;
    table16[6] = 7;
    bundle.resourceData.units16 = table16.data();
    bundle.resourceData.poolStringIndex16Limit = 100;
    bundle.resource = 0x50000004;
    status = 0;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "one", &status) == 1,
        "table16 count");
    Require(status == 0, "table16 status");
}

void Fallback() {
    std::array<std::int32_t, 96> directRoot{};
    ResourceBundle bundle = Table32Bundle(directRoot);
    std::array<std::int32_t, 96> currentRoot{};
    ResourceBundle currentBundle = Table32Bundle(currentRoot);
    std::array<std::int32_t, 96> parentRoot{};
    ResourceBundle parentBundle = Table32Bundle(parentRoot);
    parentRoot[40] = 6;

    ResourceEntry parent{};
    std::array<char, 5> rootName{'r', 'o', 'o', 't', 0};
    parent.name = rootName.data();
    parent.resourceData = parentBundle.resourceData;
    parent.resourceData.rootResource = parentBundle.resource;

    ResourceEntry current{};
    std::array<char, 3> enName{'e', 'n', 0};
    current.name = enName.data();
    current.resourceData = currentBundle.resourceData;
    current.resourceData.rootResource = currentBundle.resource;
    current.parent = &parent;

    bundle.data = &current;
    bundle.hasFallback = 1;
    std::int32_t status = 0;
    PutKey(currentRoot, 24, "local");
    currentRoot[19] = 24;
    currentRoot[22] = static_cast<std::int32_t>(0x80000028);
    currentRoot[40] = 5;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "local", &status) == 5,
        "same-entry fallback count");
    Require(status == 0, "same-entry fallback status");
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "missing", &status) == 0,
        "fallback missing count");
    Require(status == 2, "fallback missing status");

    PutKey(parentRoot, 24, "parent");
    parentRoot[19] = 24;
    parentRoot[22] = static_cast<std::int32_t>(0x80000028);
    status = 0;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "parent", &status) == 6,
        "fallback parent count");
    Require(status == -127, "fallback parent status");

    bundle.hasFallback = 0;
    bundle.data = reinterpret_cast<ResourceEntry*>(1);
    status = 0;
    Require(ures_countArrayItems_nid_postfix(
        reinterpret_cast<const UResourceBundle*>(&bundle), "parent", &status) == 0,
        "disabled fallback result");
    Require(status == 2, "disabled fallback status");
}

void Throws() {
    std::array<std::int32_t, 96> root{};
    ResourceBundle bundle = Table32Bundle(root);
    root[20] = 0x30000001;
    std::int32_t status = 0;
    RequireThrows([&] {
        ures_countArrayItems_nid_postfix(
            reinterpret_cast<const UResourceBundle*>(&bundle), "alpha", &status);
    }, "alias did not throw");
    RequireThrows([&] {
        ures_countArrayItems_nid_postfix(
            reinterpret_cast<const UResourceBundle*>(1), "alpha", &status);
    }, "invalid bundle did not throw");
}

} // namespace

int main() {
    BasicContracts();
    Table32AndCounts();
    AllResourceTypes();
    TableAndTable16();
    Fallback();
    Throws();
}
