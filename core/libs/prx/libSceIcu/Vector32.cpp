// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {

using namespace Icu::Abi;
constexpr auto MaximumCapacity = std::numeric_limits<std::int32_t>::max() / static_cast<std::int32_t>(sizeof(std::int32_t));

template<typename TValue>
TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU vector32: null pointer");
    return *value;
}

std::int32_t& At(const Vector32& vector, std::int32_t index) {
    if (index < 0 || !vector.elements) throw std::invalid_argument("ICU vector32: invalid element access");
    return vector.elements[index];
}

bool Expand(Vector32& vector, std::int32_t minimum, std::int32_t& status) {
    if (status > 0) return false;
    if (minimum < 0) {
        status = 1;
        return false;
    }
    if (vector.capacity >= minimum) return true;
    if (vector.maxCapacity > 0 && minimum > vector.maxCapacity) {
        status = 15;
        return false;
    }
    if (vector.capacity > (std::numeric_limits<std::int32_t>::max() - 1) / 2) {
        status = 1;
        return false;
    }
    auto capacity = std::max(vector.capacity * 2, minimum);
    if (vector.maxCapacity > 0) capacity = std::min(capacity, vector.maxCapacity);
    if (capacity > MaximumCapacity) {
        status = 1;
        return false;
    }
    auto* elements = static_cast<std::int32_t*>(Storage::Resize(vector.elements, sizeof(std::int32_t) * capacity));
    if (!elements) {
        status = 7;
        return false;
    }
    vector.elements = elements;
    vector.capacity = capacity;
    return true;
}

bool Ensure(Vector32& vector, std::int32_t minimum, std::int32_t& status) {
    return minimum >= 0 && vector.capacity >= minimum ? true : Expand(vector, minimum, status);
}

std::int32_t Find(const Vector32& vector, std::int32_t key, std::int32_t start = 0) {
    for (auto index = start; index < vector.count; ++index)
        if (At(vector, index) == key) return index;
    return -1;
}

void Remove(Vector32& vector, std::int32_t index) {
    if (index < 0) return;
    if (vector.count == std::numeric_limits<std::int32_t>::min()) throw std::invalid_argument("ICU vector32: count underflow");
    for (auto position = index; position < vector.count - 1; ++position) At(vector, position) = At(vector, position + 1);
    --vector.count;
}

void SetSize(Vector32& vector, std::int32_t size) {
    if (size < 0) return;
    if (size > vector.count) {
        std::int32_t status = 0;
        if (!Ensure(vector, size, status)) return;
        for (auto index = vector.count; index < size; ++index) At(vector, index) = 0;
    }
    vector.count = size;
}

void Insert(Vector32& vector, std::int32_t value, std::int32_t index, std::int32_t& status) {
    if (index < 0 || index > vector.count) return;
    if (vector.count == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU vector32: count overflow");
    if (!Ensure(vector, vector.count + 1, status)) return;
    for (auto position = vector.count; position > index; --position) At(vector, position) = At(vector, position - 1);
    At(vector, index) = value;
    ++vector.count;
}

}

extern "C" {

void APS5_VABI _ZN6icu_689UVector325_initEiR10UErrorCode_nid_postfix(Vector32* self, std::int32_t capacity, std::int32_t* status) {
    auto& vector = Require(self);
    auto& error = Require(status);
    if (capacity < 1) capacity = 8;
    if (vector.maxCapacity > 0) capacity = std::min(capacity, vector.maxCapacity);
    if (capacity > MaximumCapacity) capacity = std::min(8, vector.maxCapacity);
    if (capacity < 0) throw std::invalid_argument("ICU vector32: negative allocation size");
    vector.elements = static_cast<std::int32_t*>(Storage::Resize(nullptr, sizeof(std::int32_t) * capacity));
    if (!vector.elements) error = 7;
    else vector.capacity = capacity;
}

std::int8_t APS5_VABI _ZN6icu_689UVector3214expandCapacityEiR10UErrorCode_nid_postfix(Vector32* self, std::int32_t minimum, std::int32_t* status) {
    return Expand(Require(self), minimum, Require(status));
}

void APS5_VABI _ZN6icu_689UVector3214setMaxCapacityEi_nid_postfix(Vector32* self, std::int32_t limit) {
    auto& vector = Require(self);
    limit = std::max(limit, 0);
    if (limit > MaximumCapacity) return;
    vector.maxCapacity = limit;
    if (vector.capacity <= limit || limit == 0) return;
    auto* elements = static_cast<std::int32_t*>(Storage::Resize(vector.elements, sizeof(std::int32_t) * limit));
    if (!elements) return;
    vector.elements = elements;
    vector.capacity = limit;
    vector.count = std::min(vector.count, limit);
}

void APS5_VABI _ZN6icu_689UVector3212setElementAtEii_nid_postfix(Vector32* self, std::int32_t value, std::int32_t index) {
    auto& vector = Require(self);
    if (index >= 0 && index < vector.count) At(vector, index) = value;
}

void APS5_VABI _ZN6icu_689UVector3215insertElementAtEiiR10UErrorCode_nid_postfix(Vector32* self, std::int32_t value, std::int32_t index, std::int32_t* status) {
    Insert(Require(self), value, index, Require(status));
}

void APS5_VABI _ZN6icu_689UVector3215removeElementAtEi_nid_postfix(Vector32* self, std::int32_t index) {
    Remove(Require(self), index);
}

void APS5_VABI _ZN6icu_689UVector3217removeAllElementsEv_nid_postfix(Vector32* self) {
    Require(self).count = 0;
}

void APS5_VABI _ZN6icu_689UVector327setSizeEi_nid_postfix(Vector32* self, std::int32_t size) {
    SetSize(Require(self), size);
}

std::int32_t APS5_VABI _ZNK6icu_689UVector327indexOfEii_nid_postfix(const Vector32* self, std::int32_t value, std::int32_t start) {
    return Find(Require(self), value, start);
}

std::int8_t APS5_VABI _ZNK6icu_689UVector326equalsERKS0__nid_postfix(const Vector32* self, const Vector32* other) {
    const auto& vector = Require(self);
    const auto& source = Require(other);
    if (vector.count != source.count) return 0;
    for (std::int32_t index = 0; index < vector.count; ++index)
        if (At(vector, index) != At(source, index)) return 0;
    return 1;
}

std::int8_t APS5_VABI _ZN6icu_689UVector32eqERKS0__nid_postfix(Vector32* self, const Vector32* other) {
    return _ZNK6icu_689UVector326equalsERKS0__nid_postfix(self, other);
}

std::int8_t APS5_VABI _ZNK6icu_689UVector3211containsAllERKS0__nid_postfix(const Vector32* self, const Vector32* other) {
    const auto& vector = Require(self);
    const auto& source = Require(other);
    for (std::int32_t index = 0; index < source.count; ++index)
        if (Find(vector, At(source, index)) < 0) return 0;
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_689UVector3212containsNoneERKS0__nid_postfix(const Vector32* self, const Vector32* other) {
    const auto& vector = Require(self);
    const auto& source = Require(other);
    for (std::int32_t index = 0; index < source.count; ++index)
        if (Find(vector, At(source, index)) >= 0) return 0;
    return 1;
}

std::int8_t APS5_VABI _ZN6icu_689UVector329removeAllERKS0__nid_postfix(Vector32* self, const Vector32* other) {
    auto& vector = Require(self);
    const auto& source = Require(other);
    bool changed = false;
    for (std::int32_t index = 0; index < source.count; ++index) {
        const auto found = Find(vector, At(source, index));
        if (found < 0) continue;
        Remove(vector, found);
        changed = true;
    }
    return changed;
}

std::int8_t APS5_VABI _ZN6icu_689UVector329retainAllERKS0__nid_postfix(Vector32* self, const Vector32* other) {
    auto& vector = Require(self);
    const auto& source = Require(other);
    bool changed = false;
    for (auto index = vector.count - 1; index >= 0; --index) {
        if (Find(source, At(vector, index)) >= 0) continue;
        Remove(vector, index);
        changed = true;
    }
    return changed;
}

void APS5_VABI _ZN6icu_689UVector326assignERKS0_R10UErrorCode_nid_postfix(Vector32* self, const Vector32* other, std::int32_t* status) {
    auto& vector = Require(self);
    const auto& source = Require(other);
    if (!Ensure(vector, source.count, Require(status))) return;
    SetSize(vector, source.count);
    for (std::int32_t index = 0; index < source.count; ++index) At(vector, index) = At(source, index);
}

void APS5_VABI _ZN6icu_689UVector3212sortedInsertEiR10UErrorCode_nid_postfix(Vector32* self, std::int32_t value, std::int32_t* status) {
    auto& vector = Require(self);
    std::int32_t start = 0;
    auto limit = vector.count;
    while (start != limit) {
        const auto sum = static_cast<std::int64_t>(start) + limit;
        if (sum > std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU vector32: search overflow");
        const auto middle = static_cast<std::int32_t>(sum / 2);
        if (At(vector, middle) > value) limit = middle;
        else start = middle + 1;
    }
    Insert(vector, value, start, Require(status));
}

void* APS5_VABI _ZN6icu_689UVector3216getStaticClassIDEv_nid_postfix() {
    static char identifier;
    return &identifier;
}

void* APS5_VABI _ZNK6icu_689UVector3217getDynamicClassIDEv_nid_postfix(const Vector32*) {
    return _ZN6icu_689UVector3216getStaticClassIDEv_nid_postfix();
}

}
