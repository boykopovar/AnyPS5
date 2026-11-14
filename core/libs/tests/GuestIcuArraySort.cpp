#include "prx/libSceIcu/include/LegacyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU array sort: %s\n", message);
    std::abort();
}

struct Item {
    std::int32_t key;
    std::int32_t tag;
};

std::int32_t APS5_VABI CompareKeys(const void* context, const void* left, const void* right) {
    (void)context;
    const std::int32_t a = static_cast<const Item*>(left)->key;
    const std::int32_t b = static_cast<const Item*>(right)->key;
    return a < b ? -1 : (a > b ? 1 : 0);
}

void TestArgumentChecks() {
    Item items[] = {{3, 0}, {1, 1}, {2, 2}};
    std::int32_t status = 0;
    uprv_sortArray_nid_postfix(items, 3, sizeof(Item), CompareKeys, nullptr, 1, &status);
    Require(status == 0 && items[0].key == 1, "sortArray: a valid call with status zero sorts");

    Item untouched[] = {{3, 0}, {1, 1}};
    std::int32_t failed = 1;
    uprv_sortArray_nid_postfix(untouched, 2, sizeof(Item), CompareKeys, nullptr, 1, &failed);
    Require(failed == 1 && untouched[0].key == 3, "sortArray: an existing failure is kept and the array is untouched");

    std::int32_t nullArray = 0;
    uprv_sortArray_nid_postfix(nullptr, 3, sizeof(Item), CompareKeys, nullptr, 1, &nullArray);
    Require(nullArray == 1, "sortArray: a null array with a positive length is an illegal argument");
    std::int32_t negativeLength = 0;
    uprv_sortArray_nid_postfix(items, -1, sizeof(Item), CompareKeys, nullptr, 1, &negativeLength);
    Require(negativeLength == 1, "sortArray: a negative length is an illegal argument");
    std::int32_t zeroSize = 0;
    uprv_sortArray_nid_postfix(items, 3, 0, CompareKeys, nullptr, 1, &zeroSize);
    Require(zeroSize == 1, "sortArray: a zero item size is an illegal argument");
    std::int32_t noCompare = 0;
    uprv_sortArray_nid_postfix(items, 3, sizeof(Item), nullptr, nullptr, 1, &noCompare);
    Require(noCompare == 1, "sortArray: a null comparator is an illegal argument");
    uprv_sortArray_nid_postfix(items, 3, sizeof(Item), CompareKeys, nullptr, 1, nullptr);
    Require(true, "sortArray: a null status pointer returns without touching the array");
}

}

int main() {
    TestArgumentChecks();
    return 0;
}
