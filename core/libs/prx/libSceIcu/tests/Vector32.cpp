#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

using namespace Icu::Abi;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU vector32 check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Check(false);
}

void Capacity() {
    Vector32 vector{};
    std::int32_t status = 5;
    _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(&vector, -1, &status);
    Check(vector.capacity == 8 && status == 5 && vector.elements);
    Check(!_ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(&vector, 2, &status));
    Check(vector.capacity == 8 && status == 5);
    status = -1;
    Check(_ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(&vector, 9, &status));
    Check(vector.capacity == 16 && status == -1);
    _ZN6icu_689UVector327setSizeEi_nid_postfix(&vector, 12);
    for (int index = 0; index < 12; ++index) Check(vector.elements[index] == 0);
    _ZN6icu_689UVector3212setElementAtEii_nid_postfix(&vector, 99, 1);
    _ZN6icu_689UVector3212setElementAtEii_nid_postfix(&vector, 99, 12);
    _ZN6icu_689UVector3214setMaxCapacityEi_nid_postfix(&vector, 4);
    Check(vector.maxCapacity == 4 && vector.capacity == 4 && vector.count == 4 && vector.elements[1] == 99);
    status = 0;
    Check(!_ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(&vector, 5, &status));
    Check(status == 15 && vector.count == 4);
    _ZN6icu_689UVector327setSizeEi_nid_postfix(&vector, 5);
    Check(vector.count == 4);
    _ZN6icu_689UVector327setSizeEi_nid_postfix(&vector, -1);
    Check(vector.count == 4);
    _ZN6icu_689UVector3214setMaxCapacityEi_nid_postfix(&vector, -8);
    Check(vector.maxCapacity == 0 && vector.capacity == 4);
    status = 0;
    Check(!_ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(&vector, -1, &status) && status == 1);
    status = 0;
    Check(!_ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(&vector, std::numeric_limits<std::int32_t>::max(), &status));
    Check(status == 1 && vector.capacity == 4);
    _ZN6icu_689UVector3214setMaxCapacityEi_nid_postfix(&vector, std::numeric_limits<std::int32_t>::max());
    Check(vector.maxCapacity == 0 && vector.capacity == 4);
    Check(_ZNK6icu_689UVector3217getDynamicClassIDEv_nid_postfix(&vector) == _ZN6icu_689UVector3216getStaticClassIDEv_nid_postfix());
    Check(_ZN6icu_689UVector3216getStaticClassIDEv_nid_postfix() != _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix());
    std::free(vector.elements);
    Vector32 limited{};
    limited.maxCapacity = 3;
    _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(&limited, 10, &status);
    Check(limited.capacity == 3);
    std::free(limited.elements);
}

void Mutation() {
    Vector32 vector{};
    std::int32_t status = 0;
    _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(&vector, 1, &status);
    for (auto value : {7, -8, 1, 7, 0}) _ZN6icu_689UVector3212sortedInsertEiR10UErrorCode_nid_postfix(&vector, value, &status);
    Check(vector.count == 5 && vector.elements[0] == -8 && vector.elements[1] == 0 && vector.elements[4] == 7);
    Check(_ZNK6icu_689UVector327indexOfEii_nid_postfix(&vector, 7, 0) == 3);
    Check(_ZNK6icu_689UVector327indexOfEii_nid_postfix(&vector, 7, 4) == 4);
    Check(_ZNK6icu_689UVector327indexOfEii_nid_postfix(&vector, 7, 5) == -1);
    Invalid([&] { _ZNK6icu_689UVector327indexOfEii_nid_postfix(&vector, 7, -1); });
    _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(&vector, 90, 1, &status);
    Check(vector.count == 6 && vector.elements[1] == 90 && vector.elements[2] == 0);
    _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(&vector, 0, -1, &status);
    Check(vector.count == 6);
    _ZN6icu_689UVector3215removeElementAtEi_nid_postfix(&vector, 1);
    Check(vector.count == 5 && vector.elements[1] == 0);
    _ZN6icu_689UVector3215removeElementAtEi_nid_postfix(&vector, -1);
    Check(vector.count == 5);
    _ZN6icu_689UVector3215removeElementAtEi_nid_postfix(&vector, 999);
    Check(vector.count == 4);
    status = 3;
    _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(&vector, 42, 4, &status);
    Check(vector.count == 5 && status == 3 && vector.elements[4] == 42);
    _ZN6icu_689UVector3217removeAllElementsEv_nid_postfix(&vector);
    Check(vector.count == 0);
    _ZN6icu_689UVector3215removeElementAtEi_nid_postfix(&vector, 0);
    Check(vector.count == -1);
    _ZN6icu_689UVector3217removeAllElementsEv_nid_postfix(&vector);
    Invalid([] { _ZN6icu_689UVector327setSizeEi_nid_postfix(nullptr, 0); });
    std::free(vector.elements);
}

void Relations() {
    Vector32 vector{};
    Vector32 other{};
    std::int32_t status = 0;
    _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(&vector, 1, &status);
    _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(&other, 1, &status);
    for (auto value : {1, 2, 2, 3}) _ZN6icu_689UVector3212sortedInsertEiR10UErrorCode_nid_postfix(&vector, value, &status);
    _ZN6icu_689UVector3212sortedInsertEiR10UErrorCode_nid_postfix(&other, 2, &status);
    Check(_ZNK6icu_689UVector3211containsAllERKS0__nid_postfix(&vector, &other));
    Check(!_ZNK6icu_689UVector3212containsNoneERKS0__nid_postfix(&vector, &other));
    Check(!_ZNK6icu_689UVector326equalsERKS0__nid_postfix(&vector, &other));
    Check(_ZN6icu_689UVector329removeAllERKS0__nid_postfix(&vector, &other));
    Check(vector.count == 3 && vector.elements[1] == 2);
    Check(_ZN6icu_689UVector329retainAllERKS0__nid_postfix(&vector, &other));
    Check(vector.count == 1 && vector.elements[0] == 2);
    Check(_ZN6icu_689UVector32eqERKS0__nid_postfix(&vector, &other));
    Check(!_ZN6icu_689UVector329retainAllERKS0__nid_postfix(&vector, &vector));
    Check(_ZN6icu_689UVector329removeAllERKS0__nid_postfix(&vector, &vector) && vector.count == 0);
    Check(_ZNK6icu_689UVector3212containsNoneERKS0__nid_postfix(&vector, &other));
    Check(_ZNK6icu_689UVector3211containsAllERKS0__nid_postfix(&other, &vector));
    status = 5;
    _ZN6icu_689UVector326assignERKS0_R10UErrorCode_nid_postfix(&vector, &other, &status);
    Check(vector.count == 1 && vector.elements[0] == 2 && status == 5);
    _ZN6icu_689UVector326assignERKS0_R10UErrorCode_nid_postfix(&vector, &vector, &status);
    Check(vector.count == 1 && vector.elements[0] == 2);
    std::free(vector.elements);
    std::free(other.elements);
}

}

int main() {
    Capacity();
    Mutation();
    Relations();
    std::puts("ICU vector32 tests passed");
}
