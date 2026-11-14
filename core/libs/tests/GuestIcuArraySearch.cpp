#include "prx/libSceIcu/include/LegacyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU array search: %s\n", message);
    std::abort();
}

std::int32_t APS5_VABI CompareInts(const void* context, const void* left, const void* right) {
    (void)context;
    const std::int32_t a = *static_cast<const std::int32_t*>(left);
    const std::int32_t b = *static_cast<const std::int32_t*>(right);
    return a < b ? -1 : (a > b ? 1 : 0);
}

std::int32_t Search(std::int32_t* values, std::int32_t count, std::int32_t key) {
    return uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(values), count, &key, sizeof(std::int32_t), CompareInts, nullptr);
}

void TestFoundAndInsertionPoints() {
    std::int32_t values[] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};
    Require(Search(values, 12, 7) == 3, "search: a unique present value returns its index");
    Require(Search(values, 12, 4) == -3, "search: an absent value returns the complement of its insertion point");
    Require(Search(values, 12, 1) == 0, "search: the first value is found at index 0");
    Require(Search(values, 12, 23) == 11, "search: the last value is found at its index");
}

}

int main() {
    TestFoundAndInsertionPoints();
    return 0;
}
