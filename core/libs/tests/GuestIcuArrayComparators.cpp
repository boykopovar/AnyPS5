#include "prx/libSceIcu/include/LegacyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU array comparators: %s\n", message);
    std::abort();
}

std::int32_t CompareInt32(std::int32_t left, std::int32_t right) {
    return uprv_int32Comparator_nid_postfix(nullptr, &left, &right);
}

void TestInt32() {
    Require(CompareInt32(1, 2) == -1, "int32: smaller value sorts first");
    Require(CompareInt32(2, 1) == 1, "int32: larger value sorts last");
    Require(CompareInt32(5, 5) == 0, "int32: equal values compare equal");
    Require(CompareInt32(-1, 1) == -2, "int32: negative values subtract exactly");
    Require(CompareInt32(-5, -3) == -2, "int32: two negative values subtract exactly");
    Require(CompareInt32(0, -7) == 7, "int32: zero against a negative value");
}

}

int main() {
    TestInt32();
    return 0;
}
