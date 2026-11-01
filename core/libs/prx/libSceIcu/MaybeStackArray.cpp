// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace {

using Array = Icu::Abi::MaybeStackArray40;

template<typename TValue>
TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("MaybeStackArray: null pointer");
    return *value;
}

void Reset(Array* array) {
    Require(array).pointer = array->stack;
    array->capacity = 40;
    array->needToRelease = 0;
}

char* At(const Array* array, std::int64_t offset) {
    auto* pointer = Require(array).pointer;
    if (!pointer) throw std::invalid_argument("MaybeStackArray: null array");
    return pointer + offset;
}

}

namespace {
void Release(Icu::Abi::MaybeStackArray40* array) {
    if (!array) throw std::invalid_argument("MaybeStackArray: null object");
    if (array->needToRelease) Icu::Abi::Free_nid_no_patch(array->pointer);
}
void Move(Icu::Abi::MaybeStackArray40* array, Icu::Abi::MaybeStackArray40* source) {
    if (!array || !source) throw std::invalid_argument("MaybeStackArray: null object");
    array->capacity = source->capacity;
    array->needToRelease = source->needToRelease;
    if (source->pointer == source->stack) {
        array->pointer = array->stack;
        if (source->capacity < 0 || source->capacity > 40) throw std::invalid_argument("MaybeStackArray: invalid stack capacity");
        if (array != source) std::memcpy(array->stack, source->stack, source->capacity);
    } else { array->pointer = source->pointer; Reset(source); }
}
}
extern "C" {

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(Array* array) {
    Reset(array);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ev_nid_postfix(Array* array) {
    Reset(array);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE17resetToStackArrayEv_nid_postfix(Array* array) {
    Reset(array);
}

std::int32_t APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE11getCapacityEv_nid_postfix(const Array* array) {
    return Require(array).capacity;
}

char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE8getAliasEv_nid_postfix(const Array* array) {
    return Require(array).pointer;
}

char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EE13getArrayLimitEv_nid_postfix(const Array* array) {
    return At(array, Require(array).capacity);
}

char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(Array* array, std::int64_t offset) {
    return At(array, offset);
}

const char* APS5_VABI _ZNK6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(const Array* array, std::int64_t offset) {
    return At(array, offset);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EED1Ev_nid_postfix(Icu::Abi::MaybeStackArray40* array) {
    Release(array);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EED2Ev_nid_postfix(Icu::Abi::MaybeStackArray40* array) {
    Release(array);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE12releaseArrayEv_nid_postfix(Icu::Abi::MaybeStackArray40* array) {
    Release(array);
}

char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(Icu::Abi::MaybeStackArray40* array, std::int32_t capacity, std::int32_t length) {
    Require(array);
    if (capacity <= 0) return nullptr;
    auto* allocated = static_cast<char*>(Icu::Abi::Allocate_nid_no_patch(capacity));
    if (!allocated) return nullptr;
    if (length > 0) {
        length = std::min({length, array->capacity, capacity});
        if (length > 0 && !array->pointer) { Icu::Abi::Free_nid_no_patch(allocated); throw std::invalid_argument("MaybeStackArray: null source"); }
        std::memcpy(allocated, array->pointer, static_cast<std::size_t>(length));
    }
    Release(array);
    array->pointer = allocated;
    array->capacity = capacity;
    array->needToRelease = 1;
    return allocated;
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE12aliasInsteadEPci_nid_postfix(Icu::Abi::MaybeStackArray40* array, char* source, std::int32_t capacity) {
    if (source && capacity > 0) { Release(array); array->pointer = source; array->capacity = capacity; array->needToRelease = 0; }
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ei10UErrorCode_nid_postfix(Icu::Abi::MaybeStackArray40* array, std::int32_t capacity, std::int32_t status) {
    Reset(array);
    if (status <= 0 && capacity > 40) _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(array, capacity, 0);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC1EOS1__nid_postfix(Icu::Abi::MaybeStackArray40* array, Icu::Abi::MaybeStackArray40* source) {
    Move(array, source);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ei10UErrorCode_nid_postfix(Icu::Abi::MaybeStackArray40* array, std::int32_t capacity, std::int32_t status) {
    Reset(array);
    if (status <= 0 && capacity > 40) _ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(array, capacity, 0);
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEC2EOS1__nid_postfix(Icu::Abi::MaybeStackArray40* array, Icu::Abi::MaybeStackArray40* source) {
    Move(array, source);
}

Icu::Abi::MaybeStackArray40* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EEaSEOS1__nid_postfix(Icu::Abi::MaybeStackArray40* array, Icu::Abi::MaybeStackArray40* source) {
    if (array == source) throw std::invalid_argument("MaybeStackArray: self move assignment");
    Release(array);
    Move(array, source);
    return array;
}

void APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE8copyFromERKS1_R10UErrorCode_nid_postfix(Icu::Abi::MaybeStackArray40* array, const Icu::Abi::MaybeStackArray40* source, std::int32_t* status) {
    if (Require(status) > 0) return;
    Require(source);
    if (!_ZN6icu_6815MaybeStackArrayIcLi40EE6resizeEii_nid_postfix(array, source->capacity, 0)) { *status = 7; return; }
    if (array->capacity > 0 && !source->pointer) throw std::invalid_argument("MaybeStackArray: null source");
    if (array != source) std::memcpy(array->pointer, source->pointer, array->capacity);
}

}
