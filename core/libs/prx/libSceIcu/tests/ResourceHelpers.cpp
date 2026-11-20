#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu resource helpers: " + message);
}

struct SyntheticResourceData {
    void* data;
    const std::int32_t* pRoot;
    const std::uint16_t* p16BitUnits;
};

static_assert(offsetof(SyntheticResourceData, pRoot) == 8);
static_assert(offsetof(SyntheticResourceData, p16BitUnits) == 16);

constexpr std::uint32_t Word(std::uint32_t type, std::uint32_t offset) {
    return (type << 28) | offset;
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());

    Require(res_getPublicType_nid_postfix(0x00000012u) == 0, "US-ASCII word is not URES_STRING");
    Require(res_getPublicType_nid_postfix(0x7000002au) == 7, "integer word is not URES_INT");
    Require(res_getPublicType_nid_postfix(0xf0000000u) == -1, "reserved word reported a public type");
    Require(res_getPublicType_nid_postfix(0x80000002u) == 8, "array word is not URES_ARRAY");

    Require(res_countArrayItems_nid_postfix(nullptr, 0x00000012u) == 1, "a string does not count as one item");
    Require(res_countArrayItems_nid_postfix(nullptr, Word(6, 3)) == 1, "a string v2 does not count as one item");
    Require(res_countArrayItems_nid_postfix(nullptr, 0x7000002au) == 1, "an integer does not count as one item");
    Require(res_countArrayItems_nid_postfix(nullptr, Word(1, 0)) == 1, "binary does not count as one item");
    Require(res_countArrayItems_nid_postfix(nullptr, Word(3, 0)) == 1, "an alias does not count as one item");
    Require(res_countArrayItems_nid_postfix(nullptr, Word(14, 0)) == 1, "an integer vector does not count as one item");
    Require(res_countArrayItems_nid_postfix(nullptr, 0xf0000000u) == 0, "an unknown type reported items");
    Require(res_countArrayItems_nid_postfix(nullptr, Word(10, 7)) == 0, "type 10 reported items");
    Require(res_countArrayItems_nid_postfix(nullptr, 0x80000000u) == 0, "a zero offset array reported items");
    Require(res_countArrayItems_nid_postfix(nullptr, 0x20000000u) == 0, "a zero offset table reported items");
    Require(res_countArrayItems_nid_postfix(nullptr, 0x40000000u) == 0, "a zero offset table32 reported items");

    const std::int32_t root[] = {0, 2, 3, 0};
    const std::uint16_t units[] = {0, 5, 7, 0};
    SyntheticResourceData data{nullptr, root, units};

    Require(res_countArrayItems_nid_postfix(&data, 0x80000002u) == 3, "the array count is wrong");
    Require(res_countArrayItems_nid_postfix(&data, 0x40000002u) == 3, "the table32 count is wrong");
    Require(res_countArrayItems_nid_postfix(&data, 0x20000001u) == 2, "the table count is wrong");
    Require(res_countArrayItems_nid_postfix(&data, 0x90000001u) == 5, "the array16 count is wrong");
    Require(res_countArrayItems_nid_postfix(&data, 0x50000002u) == 7, "the table16 count is wrong");
    Require(res_countArrayItems_nid_postfix(&data, 0x90000003u) == 0, "the array16 zero count is wrong");

    return 0;
}
