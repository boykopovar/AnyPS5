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

void TestStableInsertionKeepsEqualItemsInOrder() {
    Item items[] = {{2, 0}, {1, 1}, {2, 2}, {1, 3}, {2, 4}};
    std::int32_t status = 0;
    uprv_sortArray_nid_postfix(items, 5, sizeof(Item), CompareKeys, nullptr, 1, &status);
    Require(status == 0, "sortArray stable: a valid call succeeds");
    const std::int32_t expectedTags[] = {1, 3, 0, 2, 4};
    for (int index = 0; index < 5; ++index)
        Require(items[index].tag == expectedTags[index], "sortArray stable: equal keys keep their input order");
}

void TestStableSortOfNineOrMoreItems() {
    std::vector<Item> items;
    for (std::int32_t index = 0; index < 12; ++index)
        items.push_back(Item{index % 3, index});
    std::int32_t status = 0;
    uprv_sortArray_nid_postfix(items.data(), static_cast<std::int32_t>(items.size()), sizeof(Item), CompareKeys, nullptr, 1, &status);
    Require(status == 0, "sortArray stable large: a valid call succeeds");
    for (std::size_t index = 1; index < items.size(); ++index) {
        Require(items[index - 1].key <= items[index].key, "sortArray stable large: keys are ordered");
        if (items[index - 1].key == items[index].key)
            Require(items[index - 1].tag < items[index].tag, "sortArray stable large: equal keys keep their input order");
    }
}

}

int main() {
    TestArgumentChecks();
    TestStableInsertionKeepsEqualItemsInOrder();
    TestStableSortOfNineOrMoreItems();
    return 0;
}
