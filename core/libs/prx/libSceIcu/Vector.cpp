// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

using namespace Icu::Abi;

template<typename TValue>
TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU vector: null pointer");
    return *value;
}

Element& At(const Vector& vector, std::int32_t index) {
    if (index < 0 || !vector.elements) throw std::invalid_argument("ICU vector: invalid element access");
    return vector.elements[index];
}

std::int32_t NextCount(const Vector& vector) {
    if (vector.count == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU vector: count overflow");
    return vector.count + 1;
}

bool Grow(Vector& vector, std::int32_t minimum, std::int32_t& status) {
    if (minimum < 0) {
        status = 1;
        return false;
    }
    if (vector.capacity >= minimum) return true;
    if (vector.capacity > (std::numeric_limits<std::int32_t>::max() - 1) / 2) {
        status = 1;
        return false;
    }
    const auto capacity = std::max(vector.capacity * 2, minimum);
    if (capacity > std::numeric_limits<std::int32_t>::max() / static_cast<std::int32_t>(sizeof(Element))) {
        status = 1;
        return false;
    }
    auto* elements = static_cast<Element*>(Storage::Resize(vector.elements, sizeof(Element) * capacity));
    if (!elements) {
        status = 7;
        return false;
    }
    vector.elements = elements;
    vector.capacity = capacity;
    return true;
}

std::int32_t Find(const Vector& vector, Element key, std::int32_t start, std::int8_t hint) {
    for (auto index = start; index < vector.count; ++index) {
        const auto element = At(vector, index);
        if (vector.comparer ? vector.comparer(key, element) != 0
                            : (hint & 1) ? key.pointer == element.pointer : key.integer == element.integer) return index;
    }
    return -1;
}

void* Orphan(Vector& vector, std::int32_t index) {
    if (index < 0 || index >= vector.count) return nullptr;
    auto* result = At(vector, index).pointer;
    for (auto position = index; position < vector.count - 1; ++position) At(vector, position) = At(vector, position + 1);
    --vector.count;
    return result;
}

void Remove(Vector& vector, std::int32_t index) {
    auto* value = Orphan(vector, index);
    if (value && vector.deleter) vector.deleter(value);
}

void Insert(Vector& vector, Element value, std::int32_t index, std::int32_t& status) {
    if (index < 0 || index > vector.count || !Grow(vector, NextCount(vector), status)) return;
    for (auto position = vector.count; position > index; --position) At(vector, position) = At(vector, position - 1);
    At(vector, index) = value;
    ++vector.count;
}

void SetSize(Vector& vector, std::int32_t size, std::int32_t& status) {
    if (size < 0) return;
    if (size > vector.count) {
        if (!Grow(vector, size, status)) return;
        for (auto index = vector.count; index < size; ++index) At(vector, index).pointer = nullptr;
    } else {
        for (auto index = vector.count - 1; index >= size; --index) Remove(vector, index);
    }
    vector.count = size;
}

void Set(Vector& vector, Element value, std::int32_t index) {
    if (index < 0 || index >= vector.count) return;
    auto& element = At(vector, index);
    if (element.pointer && vector.deleter) vector.deleter(element.pointer);
    element = value;
}

void SortedInsert(Vector& vector, Element value, ElementComparator compare, std::int32_t& status) {
    std::int32_t start = 0;
    auto limit = vector.count;
    while (start != limit) {
        if (!compare) throw std::invalid_argument("ICU vector: null comparator");
        const auto middle = static_cast<std::int64_t>(start) + limit;
        if (middle > std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU vector: search overflow");
        const auto index = static_cast<std::int32_t>(middle / 2);
        if (compare(At(vector, index), value) > 0) limit = index;
        else start = index + 1;
    }
    Insert(vector, value, start, status);
}

template<typename TCompare>
void InsertionSort(Element* array, std::int32_t length, TCompare compare, Element& scratch) {
    for (std::int32_t index = 1; index < length; ++index) {
        std::int32_t start = 0;
        auto limit = index;
        while (limit - start >= 9) {
            const auto middle = (start + limit) / 2;
            const auto order = compare(array + index, array + middle);
            if (order < 0) limit = middle;
            else start = middle + (order == 0);
        }
        while (start < limit && compare(array + index, array + start) >= 0) ++start;
        if (start < index) {
            scratch = array[index];
            for (auto position = index; position > start; --position) array[position] = array[position - 1];
            array[start] = scratch;
        }
    }
}

template<typename TCompare>
void QuickSort(Element* array, std::int32_t start, std::int32_t limit, TCompare compare, Element& pivot) {
    do {
        if (limit - start <= 9) {
            InsertionSort(array + start, limit - start, compare, pivot);
            return;
        }
        auto left = start;
        auto right = limit;
        pivot = array[(start + limit) / 2];
        do {
            while (compare(array + left, &pivot) < 0) {
                if (++left >= limit) throw std::invalid_argument("ICU vector: inconsistent comparator");
            }
            while (compare(&pivot, array + right - 1) < 0) {
                if (--right <= start) throw std::invalid_argument("ICU vector: inconsistent comparator");
            }
            if (left < right) {
                --right;
                if (left < right) std::swap(array[left], array[right]);
                ++left;
            }
        } while (left < right);
        if (right - start < limit - left) {
            if (start < right - 1) QuickSort(array, start, right, compare, pivot);
            start = left;
        } else {
            if (left < limit - 1) QuickSort(array, left, limit, compare, pivot);
            limit = right;
        }
    } while (start < limit - 1);
}

template<typename TCompare>
void Sort(Vector& vector, TCompare compare, bool stable, std::int32_t& status) {
    if (status > 0) return;
    if (vector.count < 0 || (vector.count > 0 && !vector.elements)) {
        status = 1;
        return;
    }
    if (vector.count <= 1) return;
    alignas(std::max_align_t) Element scratch;
    if (stable || vector.count < 9) InsertionSort(vector.elements, vector.count, compare, scratch);
    else QuickSort(vector.elements, 0, vector.count, compare, scratch);
}

}

extern "C" {

void APS5_VABI _ZN6icu_687UVector5_initEiR10UErrorCode_nid_postfix(Icu::Abi::Vector* self, std::int32_t capacity, std::int32_t* status) {
    if (Require(status) > 0) return;
    auto& vector = Require(self);
    if (capacity < 1 || capacity > std::numeric_limits<std::int32_t>::max() / static_cast<std::int32_t>(sizeof(Element))) capacity = 8;
    vector.elements = static_cast<Element*>(Storage::Resize(nullptr, sizeof(Element) * capacity));
    if (!vector.elements) *status = 7;
    else vector.capacity = capacity;
}

std::int8_t APS5_VABI _ZN6icu_687UVector14ensureCapacityEiR10UErrorCode_nid_postfix(Vector* self, std::int32_t minimum, std::int32_t* status) {
    return Grow(Require(self), minimum, Require(status));
}

void APS5_VABI _ZN6icu_687UVector10addElementEPvR10UErrorCode_nid_postfix(Vector* self, void* value, std::int32_t* status) {
    auto& vector = Require(self);
    if (Grow(vector, NextCount(vector), Require(status))) At(vector, vector.count++).pointer = value;
}

void APS5_VABI _ZN6icu_687UVector10addElementEiR10UErrorCode_nid_postfix(Vector* self, std::int32_t value, std::int32_t* status) {
    auto& vector = Require(self);
    if (Grow(vector, NextCount(vector), Require(status))) {
        auto& element = At(vector, vector.count++);
        element.pointer = nullptr;
        element.integer = value;
    }
}

void APS5_VABI _ZN6icu_687UVector12setElementAtEPvi_nid_postfix(Vector* self, void* value, std::int32_t index) {
    Set(Require(self), Element{value}, index);
}

void APS5_VABI _ZN6icu_687UVector12setElementAtEii_nid_postfix(Vector* self, std::int32_t value, std::int32_t index) {
    Element element{nullptr};
    element.integer = value;
    Set(Require(self), element, index);
}

void APS5_VABI _ZN6icu_687UVector15insertElementAtEPviR10UErrorCode_nid_postfix(Vector* self, void* value, std::int32_t index, std::int32_t* status) {
    Insert(Require(self), Element{value}, index, Require(status));
}

void APS5_VABI _ZN6icu_687UVector15insertElementAtEiiR10UErrorCode_nid_postfix(Vector* self, std::int32_t value, std::int32_t index, std::int32_t* status) {
    Element element{nullptr};
    element.integer = value;
    Insert(Require(self), element, index, Require(status));
}

void* APS5_VABI _ZNK6icu_687UVector9elementAtEi_nid_postfix(const Vector* self, std::int32_t index) {
    const auto& vector = Require(self);
    return index >= 0 && index < vector.count ? At(vector, index).pointer : nullptr;
}

std::int32_t APS5_VABI _ZNK6icu_687UVector10elementAtiEi_nid_postfix(const Vector* self, std::int32_t index) {
    const auto& vector = Require(self);
    return index >= 0 && index < vector.count ? At(vector, index).integer : 0;
}

std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfE8UElementia_nid_postfix(const Vector* self, Element key, std::int32_t start, std::int8_t hint) {
    return Find(Require(self), key, start, hint);
}

std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfEPvi_nid_postfix(const Vector* self, void* key, std::int32_t start) {
    return Find(Require(self), Element{key}, start, 1);
}

std::int32_t APS5_VABI _ZNK6icu_687UVector7indexOfEii_nid_postfix(const Vector* self, std::int32_t key, std::int32_t start) {
    Element element{nullptr};
    element.integer = key;
    return Find(Require(self), element, start, 2);
}

std::int8_t APS5_VABI _ZN6icu_687UVectoreqERKS0__nid_postfix(Vector* self, const Vector* other) {
    const auto& vector = Require(self);
    const auto& source = Require(other);
    if (vector.count != source.count) return 0;
    if (vector.comparer) {
        for (std::int32_t index = 0; index < vector.count; ++index)
            if (!vector.comparer(At(vector, index), At(source, index))) return 0;
    }
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_687UVector6equalsERKS0__nid_postfix(const Vector* self, const Vector* other) {
    const auto& vector = Require(self);
    const auto& source = Require(other);
    if (vector.count != source.count) return 0;
    for (std::int32_t index = 0; index < vector.count; ++index) {
        if (vector.comparer) {
            if (!vector.comparer(Element{&At(source, index)}, At(vector, index))) return 0;
        } else if (At(vector, index).pointer != At(source, index).pointer) return 0;
    }
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_687UVector11containsAllERKS0__nid_postfix(const Vector* self, const Vector* other) {
    const auto& vector = Require(self);
    const auto& source = Require(other);
    for (std::int32_t index = 0; index < source.count; ++index)
        if (Find(vector, At(source, index), 0, 0) < 0) return 0;
    return 1;
}

std::int8_t APS5_VABI _ZNK6icu_687UVector12containsNoneERKS0__nid_postfix(const Vector* self, const Vector* other) {
    const auto& vector = Require(self);
    const auto& source = Require(other);
    for (std::int32_t index = 0; index < source.count; ++index)
        if (Find(vector, At(source, index), 0, 0) >= 0) return 0;
    return 1;
}

void* APS5_VABI _ZN6icu_687UVector15orphanElementAtEi_nid_postfix(Vector* self, std::int32_t index) {
    return Orphan(Require(self), index);
}

void APS5_VABI _ZN6icu_687UVector15removeElementAtEi_nid_postfix(Vector* self, std::int32_t index) {
    Remove(Require(self), index);
}

std::int8_t APS5_VABI _ZN6icu_687UVector13removeElementEPv_nid_postfix(Vector* self, void* value) {
    auto& vector = Require(self);
    const auto index = Find(vector, Element{value}, 0, 1);
    if (index < 0) return 0;
    Remove(vector, index);
    return 1;
}

void APS5_VABI _ZN6icu_687UVector17removeAllElementsEv_nid_postfix(Vector* self) {
    auto& vector = Require(self);
    if (vector.deleter) {
        for (std::int32_t index = 0; index < vector.count; ++index) {
            auto* value = At(vector, index).pointer;
            if (value) vector.deleter(value);
        }
    }
    vector.count = 0;
}

std::int8_t APS5_VABI _ZN6icu_687UVector9removeAllERKS0__nid_postfix(Vector* self, const Vector* other) {
    auto& vector = Require(self);
    const auto& source = Require(other);
    bool changed = false;
    for (std::int32_t index = 0; index < source.count; ++index) {
        const auto found = Find(vector, At(source, index), 0, 0);
        if (found < 0) continue;
        Remove(vector, found);
        changed = true;
    }
    return changed;
}

std::int8_t APS5_VABI _ZN6icu_687UVector9retainAllERKS0__nid_postfix(Vector* self, const Vector* other) {
    auto& vector = Require(self);
    const auto& source = Require(other);
    bool changed = false;
    for (auto index = vector.count - 1; index >= 0; --index) {
        if (Find(source, At(vector, index), 0, 0) >= 0) continue;
        Remove(vector, index);
        changed = true;
    }
    return changed;
}

void APS5_VABI _ZN6icu_687UVector7setSizeEiR10UErrorCode_nid_postfix(Vector* self, std::int32_t size, std::int32_t* status) {
    SetSize(Require(self), size, Require(status));
}

void APS5_VABI _ZN6icu_687UVector6assignERKS0_PFvP8UElementS4_ER10UErrorCode_nid_postfix(Vector* self, const Vector* other, ElementAssigner assign, std::int32_t* status) {
    auto& vector = Require(self);
    const auto& source = Require(other);
    auto& error = Require(status);
    if (!Grow(vector, source.count, error)) return;
    SetSize(vector, source.count, error);
    if (error > 0) return;
    for (std::int32_t index = 0; index < source.count; ++index) {
        auto& element = At(vector, index);
        if (element.pointer && vector.deleter) vector.deleter(element.pointer);
        if (!assign) throw std::invalid_argument("ICU vector: null assigner");
        assign(&element, &At(source, index));
    }
}

void** APS5_VABI _ZNK6icu_687UVector7toArrayEPPv_nid_postfix(const Vector* self, void** result) {
    const auto& vector = Require(self);
    for (std::int32_t index = 0; index < vector.count; ++index) {
        if (!result) throw std::invalid_argument("ICU vector: null destination");
        result[index] = At(vector, index).pointer;
    }
    return result;
}

ElementDeleter APS5_VABI _ZN6icu_687UVector10setDeleterEPFvPvE_nid_postfix(Vector* self, ElementDeleter deleter) {
    return std::exchange(Require(self).deleter, deleter);
}

ElementsEqual APS5_VABI _ZN6icu_687UVector11setComparerEPFa8UElementS1_E_nid_postfix(Vector* self, ElementsEqual comparer) {
    return std::exchange(Require(self).comparer, comparer);
}

void APS5_VABI _ZN6icu_687UVector12sortedInsertE8UElementPFaS1_S1_ER10UErrorCode_nid_postfix(Vector* self, Element value, ElementComparator compare, std::int32_t* status) {
    SortedInsert(Require(self), value, compare, Require(status));
}

void APS5_VABI _ZN6icu_687UVector12sortedInsertEPvPFa8UElementS2_ER10UErrorCode_nid_postfix(Vector* self, void* value, ElementComparator compare, std::int32_t* status) {
    SortedInsert(Require(self), Element{value}, compare, Require(status));
}

void APS5_VABI _ZN6icu_687UVector12sortedInsertEiPFa8UElementS1_ER10UErrorCode_nid_postfix(Vector* self, std::int32_t value, ElementComparator compare, std::int32_t* status) {
    Element element{nullptr};
    element.integer = value;
    SortedInsert(Require(self), element, compare, Require(status));
}

void APS5_VABI _ZN6icu_687UVector5sortiER10UErrorCode_nid_postfix(Vector* self, std::int32_t* status) {
    Sort(Require(self), [](const Element* left, const Element* right) {
        return (left->integer > right->integer) - (left->integer < right->integer);
    }, false, Require(status));
}

void APS5_VABI _ZN6icu_687UVector4sortEPFa8UElementS1_ER10UErrorCode_nid_postfix(Vector* self, ElementComparator compare, std::int32_t* status) {
    Sort(Require(self), [compare](const Element* left, const Element* right) {
        if (!compare) throw std::invalid_argument("ICU vector: null comparator");
        return compare(*left, *right);
    }, false, Require(status));
}

void APS5_VABI _ZN6icu_687UVector19sortWithUComparatorEPFiPKvS2_S2_ES2_R10UErrorCode_nid_postfix(Vector* self, ArrayComparator compare, const void* context, std::int32_t* status) {
    auto& error = Require(status);
    if (error > 0) return;
    if (!compare) {
        error = 1;
        return;
    }
    Sort(Require(self), [compare, context](const Element* left, const Element* right) {
        return compare(context, left, right);
    }, true, error);
}

void* APS5_VABI _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix() {
    static char identifier;
    return &identifier;
}

void* APS5_VABI _ZNK6icu_687UVector17getDynamicClassIDEv_nid_postfix(const Vector*) {
    return _ZN6icu_687UVector16getStaticClassIDEv_nid_postfix();
}

void* APS5_VABI _ZN6icu_686UStack16getStaticClassIDEv_nid_postfix() {
    static char identifier;
    return &identifier;
}

void* APS5_VABI _ZNK6icu_686UStack17getDynamicClassIDEv_nid_postfix(const Stack*) {
    return _ZN6icu_686UStack16getStaticClassIDEv_nid_postfix();
}

void* APS5_VABI _ZN6icu_686UStack3popEv_nid_postfix(Stack* self) {
    auto& vector = Require(self);
    if (vector.count <= 0) return nullptr;
    auto* result = At(vector, vector.count - 1).pointer;
    Remove(vector, vector.count - 1);
    return result;
}

std::int32_t APS5_VABI _ZN6icu_686UStack4popiEv_nid_postfix(Stack* self) {
    auto& vector = Require(self);
    if (vector.count <= 0) return 0;
    const auto result = At(vector, vector.count - 1).integer;
    Remove(vector, vector.count - 1);
    return result;
}

std::int32_t APS5_VABI _ZNK6icu_686UStack6searchEPv_nid_postfix(const Stack* self, void* value) {
    const auto& vector = Require(self);
    const auto index = Find(vector, Element{value}, 0, 1);
    return index < 0 ? index : vector.count - index;
}

}
