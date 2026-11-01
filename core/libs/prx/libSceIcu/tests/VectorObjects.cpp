// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdlib>

namespace {
std::int32_t deletions;
void Check(bool value) { if (!value) std::abort(); }
void APS5_VABI Delete(void*) { ++deletions; }
}
int main() {
    using namespace Icu::Abi;
    std::int32_t status = 3;
    Vector vector{};
    _ZN6icu_687UVectorC1ER10UErrorCode_nid_postfix(&vector, &status);
    Check(vector.count == 0 && vector.capacity == 0 && !vector.elements && vector.vtable);
    _ZN6icu_687UVectorD1Ev_nid_postfix(&vector);
    status = 0;
    Stack stack{};
    _ZN6icu_686UStackC2EPFvPvEPFa8UElementS4_EiR10UErrorCode_nid_postfix(&stack, Delete, nullptr, -1, &status);
    Check(stack.capacity == 8 && stack.deleter == Delete && stack.count == 0 && status == 0);
    _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(&stack, &status, &status);
    _ZN6icu_686UStackD1Ev_nid_postfix(&stack);
    Check(deletions == 1 && stack.count == 0 && stack.elements == nullptr);
    Vector32 integers{};
    status = 3;
    _ZN6icu_689UVector32C2EiR10UErrorCode_nid_postfix(&integers, 1, &status);
    Check(integers.capacity == 1 && integers.count == 0 && integers.maxCapacity == 0 && integers.elements && status == 3);
    _ZN6icu_689UVector32D2Ev_nid_postfix(&integers);
    Check(integers.elements == nullptr);
    return 0;
}
