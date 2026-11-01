#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

using namespace Icu::Abi;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU vector check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Check(false);
}

std::vector<void*> deleted;

void APS5_VABI Delete(void* value) { deleted.push_back(value); }

std::int8_t APS5_VABI Equal(Element left, Element right) { return left.integer == right.integer ? -7 : 0; }

std::int8_t APS5_VABI IndirectEqual(Element left, Element right) {
    return static_cast<Element*>(left.pointer)->integer == right.integer ? 5 : 0;
}

std::int8_t APS5_VABI Compare(Element left, Element right) {
    return (left.integer > right.integer) - (left.integer < right.integer);
}

std::int32_t APS5_VABI StableCompare(const void* context, const void* left, const void* right) {
    Check(context == &deleted);
    return Compare(*static_cast<const Element*>(left), *static_cast<const Element*>(right));
}

void APS5_VABI Assign(Element* destination, Element* source) { *destination = *source; }

void Basics() {
    Vector vector{};
    std::int32_t status = 3;
    _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(&vector, 1, &status);
    Check(vector.elements == nullptr && vector.capacity == 0 && status == 3);
    status = -1;
    _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(&vector, -9, &status);
    Check(vector.capacity == 8 && status == -1);
    Check(_ZNK6icu_687UVector9elementAtEi_nid_postfix(&vector, 0) == nullptr);
    Check(_ZNK6icu_687UVector10elementAtiEi_nid_postfix(&vector, -1) == 0);
    Check(_ZNK6icu_687UVector7toArrayEPPv_nid_postfix(&vector, nullptr) == nullptr);
    for (int value : {3, -8, 3, 9}) _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&vector, value, &status);
    Check(vector.count == 4 && vector.elements[1].integer == -8);
    Check(reinterpret_cast<std::uintptr_t>(vector.elements[1].pointer) == 0xfffffff8u);
    Element key{nullptr};
    key.integer = 3;
    Check(_ZNK6icu_687UVector7indexOfE8UElementia_nid_postfix(&vector, key, 1, 0) == 2);
    Check(_ZNK6icu_687UVector7indexOfE8UElementia_nid_postfix(&vector, key, 4, 0) == -1);
    Invalid([&] { _ZNK6icu_687UVector7indexOfE8UElementia_nid_postfix(&vector, key, -1, 0); });
    _ZN6icu_687UVector15insertElementAtEiiR10UErrorCode_nid_postfix(&vector, 7, 1, &status);
    _ZN6icu_687UVector12setElementAtEii_nid_postfix(&vector, 6, 0);
    _ZN6icu_687UVector12setElementAtEii_nid_postfix(&vector, 11, -1);
    Check(vector.count == 5 && vector.elements[0].integer == 6 && vector.elements[1].integer == 7);
    status = 4;
    _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&vector, 5, &status);
    Check(vector.count == 6 && status == 4);
    Check(_ZN6icu_687UVector14ensureCapacityEiR10UErrorCode_nid_postfix(&vector, 30, &status) == 1);
    Check(vector.capacity == 30 && status == 4);
    Check(!_ZN6icu_687UVector14ensureCapacityEiR10UErrorCode_nid_postfix(&vector, -1, &status) && status == 1);
    status = 0;
    Check(!_ZN6icu_687UVector14ensureCapacityEiR10UErrorCode_nid_postfix(&vector, std::numeric_limits<std::int32_t>::max(), &status));
    Check(status == 1 && vector.capacity == 30);
    _ZN6icu_687UVector7setSizeEiR10UErrorCode_nid_postfix(&vector, 9, &status);
    Check(vector.count == 9 && vector.elements[8].pointer == nullptr);
    _ZN6icu_687UVector7setSizeEiR10UErrorCode_nid_postfix(&vector, -1, &status);
    Check(vector.count == 9);
    _ZN6icu_687UVector7setSizeEiR10UErrorCode_nid_postfix(&vector, 2, &status);
    Check(vector.count == 2);
    std::array<void*, 2> output{};
    Check(_ZNK6icu_687UVector7toArrayEPPv_nid_postfix(&vector, output.data()) == output.data());
    Check(output[0] == vector.elements[0].pointer && output[1] == vector.elements[1].pointer);
    Invalid([&] { _ZNK6icu_687UVector7toArrayEPPv_nid_postfix(&vector, nullptr); });
    Invalid([] { _ZNK6icu_687UVector9elementAtEi_nid_postfix(nullptr, 0); });
    _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(&vector);
    Check(vector.count == 0);
    std::free(vector.elements);
}

void Callbacks() {
    int values[4]{};
    Vector vector{};
    std::int32_t status = 0;
    _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(&vector, 1, &status);
    Check(_ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(&vector, Delete) == nullptr);
    _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(&vector, values, &status);
    _ZN6icu_687UVector15insertElementAtEPviR10UErrorCode_nid_postfix(&vector, values + 1, 0, &status);
    Check(_ZNK6icu_687UVector7indexOfEPvi_nid_postfix(&vector, values, 0) == 1);
    deleted.clear();
    _ZN6icu_687UVector12setElementAtEPvi_nid_postfix(&vector, values + 2, 1);
    Check(deleted == std::vector<void*>{values});
    Check(_ZN6icu_687UVector15orphanElementAtEi_nid_postfix(&vector, 0) == values + 1);
    Check(deleted.size() == 1 && vector.count == 1);
    Check(_ZN6icu_687UVector13removeElementEPv_nid_postfix(&vector, values + 2));
    Check(deleted.back() == values + 2 && vector.count == 0);
    Check(!_ZN6icu_687UVector13removeElementEPv_nid_postfix(&vector, values + 2));
    Check(_ZN6icu_687UVector15orphanElementAtEi_nid_postfix(&vector, 100) == nullptr);
    _ZN6icu_687UVector15removeElementAtEi_nid_postfix(&vector, -1);
    Check(_ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(&vector, nullptr) == Delete);
    Check(_ZN6icu_687UVector11setComparerEPFa8UElementS1_E_nid_postfix(&vector, Equal) == nullptr);
    _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&vector, 12, &status);
    Element elements[1]{};
    elements[0].integer = 15;
    Vector other{nullptr, 1, 1, elements, nullptr, nullptr};
    Check(!_ZN6icu_687UVectoreqERKS0__nid_postfix(&vector, &other));
    Check(_ZN6icu_687UVector11setComparerEPFa8UElementS1_E_nid_postfix(&vector, nullptr) == Equal);
    Check(_ZN6icu_687UVectoreqERKS0__nid_postfix(&vector, &other));
    Check(!_ZNK6icu_687UVector6equalsERKS0__nid_postfix(&vector, &other));
    elements[0].integer = 12;
    _ZN6icu_687UVector11setComparerEPFa8UElementS1_E_nid_postfix(&vector, IndirectEqual);
    Check(_ZNK6icu_687UVector6equalsERKS0__nid_postfix(&vector, &other));
    _ZN6icu_687UVector11setComparerEPFa8UElementS1_E_nid_postfix(&vector, Equal);
    Check(_ZNK6icu_687UVector11containsAllERKS0__nid_postfix(&vector, &other));
    Check(!_ZNK6icu_687UVector12containsNoneERKS0__nid_postfix(&vector, &other));
    _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&vector, 19, &status);
    Check(_ZN6icu_687UVector9retainAllERKS0__nid_postfix(&vector, &other));
    Check(vector.count == 1);
    _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&vector, 12, &status);
    Check(_ZN6icu_687UVector9removeAllERKS0__nid_postfix(&vector, &other) && vector.count == 1);
    Check(!_ZN6icu_687UVector9retainAllERKS0__nid_postfix(&vector, &vector));
    Check(_ZN6icu_687UVector9removeAllERKS0__nid_postfix(&vector, &vector) && vector.count == 0);
    _ZN6icu_687UVector6assignERKS0_PFvP8UElementS4_ER10UErrorCode_nid_postfix(&vector, &other, Assign, &status);
    Check(vector.count == 1 && vector.elements[0].integer == 12);
    status = 3;
    elements[0].integer = 22;
    _ZN6icu_687UVector6assignERKS0_PFvP8UElementS4_ER10UErrorCode_nid_postfix(&vector, &other, Assign, &status);
    Check(vector.elements[0].integer == 12);
    status = 0;
    Invalid([&] { _ZN6icu_687UVector6assignERKS0_PFvP8UElementS4_ER10UErrorCode_nid_postfix(&vector, &other, nullptr, &status); });
    std::free(vector.elements);
}

void Sorting() {
    Vector vector{};
    std::int32_t status = 0;
    _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(&vector, 1, &status);
    for (int length : {0, 1, 2, 8, 9, 10, 31, 257}) {
        std::vector<std::int32_t> expected;
        for (int index = 0; index < length; ++index) expected.push_back((index * 17 + 9) % 23 - 12);
        for (int mode = 0; mode < 3; ++mode) {
            _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(&vector);
            for (auto value : expected) _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&vector, value, &status);
            if (mode == 0) _ZN6icu_687UVector5sortiER10UErrorCode_nid_postfix(&vector, &status);
            if (mode == 1) _ZN6icu_687UVector4sortEPFa8UElementS1_ER10UErrorCode_nid_postfix(&vector, Compare, &status);
            if (mode == 2) _ZN6icu_687UVector19sortWithUComparatorEPFiPKvS2_S2_ES2_R10UErrorCode_nid_postfix(&vector, StableCompare, &deleted, &status);
            auto sorted = expected;
            std::sort(sorted.begin(), sorted.end());
            Check(vector.count == length && status == 0);
            for (int index = 0; index < length; ++index) Check(vector.elements[index].integer == sorted[index]);
        }
    }
    status = 3;
    _ZN6icu_687UVector4sortEPFa8UElementS1_ER10UErrorCode_nid_postfix(&vector, nullptr, &status);
    Check(status == 3);
    status = 0;
    _ZN6icu_687UVector19sortWithUComparatorEPFiPKvS2_S2_ES2_R10UErrorCode_nid_postfix(&vector, nullptr, nullptr, &status);
    Check(status == 1);
    status = 0;
    _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(&vector);
    for (int value : {7, 1, 4, 1}) {
        Element element{nullptr};
        element.integer = value;
        _ZN6icu_687UVector12sortedInsertE8UElementPFaS1_S1_ER10UErrorCode_nid_postfix(&vector, element, Compare, &status);
    }
    Check(vector.count == 4 && vector.elements[0].integer == 1 && vector.elements[3].integer == 7);
    _ZN6icu_687UVector12sortedInsertEPvPFa8UElementS2_ER10UErrorCode_nid_postfix(&vector, nullptr, Compare, &status);
    Check(vector.count == 5 && vector.elements[0].integer == 0);
    Invalid([&] { _ZN6icu_687UVector12sortedInsertEPvPFa8UElementS2_ER10UErrorCode_nid_postfix(&vector, nullptr, nullptr, &status); });
    std::free(vector.elements);
}

void Stacks() {
    Stack stack{};
    std::int32_t status = 0;
    _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(&stack, 1, &status);
    Check(_ZN6icu_686UStack3popEv_nid_postfix(&stack) == nullptr);
    Check(_ZN6icu_686UStack4popiEv_nid_postfix(&stack) == 0);
    int values[2]{};
    _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(&stack, values, &status);
    _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(&stack, values + 1, &status);
    _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(&stack, values, &status);
    Check(_ZNK6icu_686UStack6searchEPv_nid_postfix(&stack, values) == 3);
    Check(_ZNK6icu_686UStack6searchEPv_nid_postfix(&stack, nullptr) == -1);
    _ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(&stack, Delete);
    deleted.clear();
    Check(_ZN6icu_686UStack3popEv_nid_postfix(&stack) == values && deleted.back() == values);
    _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(&stack);
    Check(deleted.size() == 3);
    _ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(&stack, nullptr);
    _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(&stack, -100, &status);
    Check(_ZN6icu_686UStack4popiEv_nid_postfix(&stack) == -100);
    Check(_ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix(&stack) == _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix());
    Check(_ZNK6icu_686UStack17getDynamicClassIDEv_nid_postfix(&stack) == _ZN6icu_686UStack16getStaticClassIDEv_nid_postfix());
    Check(_ZN6icu_687UVector16getStaticClassIDEv_nid_postfix() != _ZN6icu_686UStack16getStaticClassIDEv_nid_postfix());
    std::free(stack.elements);
}

}

int main() {
    Basics();
    Callbacks();
    Sorting();
    Stacks();
    std::puts("ICU vector tests passed");
}
