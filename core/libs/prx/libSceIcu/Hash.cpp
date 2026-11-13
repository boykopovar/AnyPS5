// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <bit>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

using namespace Icu;

constexpr std::array<std::int32_t, 29> Primes{
    7, 13, 31, 61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749,
    65521, 131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593,
    16777213, 33554393, 67108859, 134217689, 268435399, 536870909, 1073741789, 2147483647};
constexpr std::int32_t Deleted = std::numeric_limits<std::int32_t>::min();
constexpr std::int32_t Empty = Deleted + 1;

template<typename TValue>
TValue Require(TValue value) {
    if (!value) throw std::invalid_argument("ICU hash: null pointer");
    return value;
}

void* Allocate(std::size_t bytes) {
    return std::malloc(bytes);
}

UHashTok Pointer(const void* value) {
    return UHashTok{.pointer = const_cast<void*>(value)};
}

UHashTok Integer(std::int32_t value) {
    UHashTok token{};
    token.integer = value;
    return token;
}

std::int32_t WaterMark(std::int32_t length, float ratio) {
    const float mark = length * ratio;
    if (mark >= 2147483648.0f) throw std::invalid_argument("ICU hash: water mark overflow");
    return static_cast<std::int32_t>(mark);
}

void AllocateElements(UHashtable* hash, std::int32_t prime, std::int32_t* status) {
    if (*status > 0) return;
    hash->primeIndex = static_cast<std::int8_t>(prime);
    hash->length = Primes[prime];
    hash->elements = static_cast<UHashElement*>(Allocate(sizeof(UHashElement) * hash->length));
    if (!hash->elements) {
        *status = 7;
        return;
    }
    for (std::int32_t i = 0; i < hash->length; ++i) hash->elements[i] = {Empty, {}, {}};
    hash->count = 0;
    hash->lowWaterMark = WaterMark(hash->length, hash->lowWaterRatio);
    hash->highWaterMark = WaterMark(hash->length, hash->highWaterRatio);
}

std::int32_t PrimeIndex(std::int32_t size) {
    std::int32_t index = 0;
    while (index < 28 && Primes[index] < size) ++index;
    return index;
}

UHashtable* Init(UHashtable* hash, HashFunction hasher, HashComparator compare, HashComparator values,
                 std::int32_t prime, std::int32_t* status) {
    if (*Require(status) > 0) return nullptr;
    Require(hash);
    hash->keyHasher = hasher;
    hash->keyComparator = compare;
    hash->valueComparator = values;
    hash->keyDeleter = nullptr;
    hash->valueDeleter = nullptr;
    hash->allocated = 0;
    hash->lowWaterRatio = 0;
    hash->highWaterRatio = 0.5f;
    AllocateElements(hash, prime, status);
    return *status > 0 ? nullptr : hash;
}

UHashtable* Open(HashFunction hasher, HashComparator compare, HashComparator values,
                 std::int32_t prime, std::int32_t* status) {
    if (*Require(status) > 0) return nullptr;
    auto* hash = static_cast<UHashtable*>(Allocate(sizeof(UHashtable)));
    if (!hash) {
        *status = 7;
        return nullptr;
    }
    Init(hash, hasher, compare, values, prime, status);
    hash->allocated = 1;
    if (*status <= 0) return hash;
    std::free(hash);
    return nullptr;
}

UHashElement* Find(const UHashtable* hash, UHashTok key, std::int32_t hashcode) {
    Require(hash);
    Require(hash->elements);
    if (hash->length <= 1) throw std::invalid_argument("ICU hash: invalid table length");
    hashcode &= 0x7fffffff;
    const auto start = (hashcode ^ 0x4000000) % hash->length;
    auto position = start;
    std::int32_t firstDeleted = -1;
    std::int32_t jump = 0;
    do {
        auto& element = hash->elements[position];
        if (element.hashcode == hashcode) {
            if (Require(hash->keyComparator)(key, element.key)) return &element;
        } else if (element.hashcode == Empty) {
            return &hash->elements[firstDeleted < 0 ? position : firstDeleted];
        } else if (element.hashcode < 0 && firstDeleted < 0) {
            firstDeleted = position;
        }
        if (!jump) jump = hashcode % (hash->length - 1) + 1;
        if (position > std::numeric_limits<std::int32_t>::max() - jump)
            throw std::invalid_argument("ICU hash: probe overflow");
        position = (position + jump) % hash->length;
    } while (position != start);
    if (firstDeleted >= 0) return &hash->elements[firstDeleted];
    throw std::invalid_argument("ICU hash: full table");
}

UHashElement* Find(const UHashtable* hash, UHashTok key) {
    return Find(hash, key, Require(Require(hash)->keyHasher)(key));
}

void Rehash(UHashtable* hash, std::int32_t* status) {
    auto prime = static_cast<std::int32_t>(hash->primeIndex);
    if (hash->count > hash->highWaterMark) {
        if (++prime >= 29) return;
    } else if (hash->count < hash->lowWaterMark) {
        if (--prime < 0) return;
    } else return;
    auto* old = hash->elements;
    const auto length = hash->length;
    AllocateElements(hash, prime, status);
    if (*status > 0) {
        hash->elements = old;
        hash->length = length;
        return;
    }
    for (auto i = length; i-- > 0;) {
        if (old[i].hashcode < 0) continue;
        *Find(hash, old[i].key, old[i].hashcode) = old[i];
        ++hash->count;
    }
    std::free(old);
}

void Delete(UHashtable* hash, UHashTok key, UHashTok value) {
    if (hash->keyDeleter && key.pointer) hash->keyDeleter(key.pointer);
    if (hash->valueDeleter && value.pointer) hash->valueDeleter(value.pointer);
}

UHashTok Set(UHashtable* hash, UHashElement* element, std::int32_t code, UHashTok key, UHashTok value) {
    auto previous = element->value;
    if (hash->keyDeleter && element->key.pointer && element->key.pointer != key.pointer)
        hash->keyDeleter(element->key.pointer);
    if (hash->valueDeleter) {
        if (previous.pointer && previous.pointer != value.pointer) hash->valueDeleter(previous.pointer);
        previous.pointer = nullptr;
    }
    element->key = key;
    element->value = value;
    element->hashcode = code;
    return previous;
}

UHashTok RemoveElement(UHashtable* hash, UHashElement* element) {
    --hash->count;
    return Set(hash, element, Deleted, {}, {});
}

UHashTok Remove(UHashtable* hash, UHashTok key) {
    auto* element = Find(hash, key);
    if (element->hashcode < 0) return {};
    auto result = RemoveElement(hash, element);
    if (hash->count < hash->lowWaterMark) {
        std::int32_t status = 0;
        Rehash(hash, &status);
    }
    return result;
}

UHashTok Put(UHashtable* hash, UHashTok key, UHashTok value, bool pointerValue, std::int32_t* status) {
    Require(status);
    Require(hash);
    if (*status <= 0) {
        if (pointerValue && !value.pointer) return Remove(hash, key);
        if (hash->count > hash->highWaterMark) Rehash(hash, status);
        if (*status <= 0) {
            const auto code = Require(hash->keyHasher)(key);
            auto* element = Find(hash, key, code);
            if (element->hashcode < 0) {
                ++hash->count;
                if (hash->count == hash->length) {
                    --hash->count;
                    *status = 7;
                }
            }
            if (*status <= 0) return Set(hash, element, code & 0x7fffffff, key, value);
        }
    }
    Delete(hash, key, value);
    return {};
}

std::uint8_t Lower(std::uint8_t value) {
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

template<typename TChar>
std::int32_t Hash(const TChar* text, std::int32_t length, bool fold = false) {
    if (!text) return 0;
    if (length < std::numeric_limits<std::int32_t>::min() + 32)
        throw std::invalid_argument("ICU hash: length overflow");
    const auto step = (length - 32) / 32 + 1;
    std::uint32_t hash = 0;
    for (std::int64_t i = 0; i < length; i += step) {
        const auto value = static_cast<std::uint32_t>(text[i]);
        hash = hash * 37 + (fold ? Lower(value) : value);
    }
    return std::bit_cast<std::int32_t>(hash);
}

template<typename TChar>
std::int32_t Length(const TChar* text) {
    std::int32_t length = 0;
    if (!text) return 0;
    while (text[length]) {
        if (length == std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument("ICU hash: string length overflow");
        ++length;
    }
    return length;
}

template<typename TChar>
std::int8_t Equal(UHashTok left, UHashTok right, bool fold = false) {
    auto* first = static_cast<const TChar*>(left.pointer);
    auto* second = static_cast<const TChar*>(right.pointer);
    if (first == second) return 1;
    if (!first || !second) return 0;
    while (*first && (fold ? Lower(*first) == Lower(*second) : *first == *second)) {
        ++first;
        ++second;
    }
    return *first == *second;
}

}

extern "C" {

Icu::UHashtable* APS5_VABI uhash_open_nid_postfix(Icu::HashFunction hasher, Icu::HashComparator compare, Icu::HashComparator values, std::int32_t* status) {
    return Open(hasher, compare, values, 4, status);
}

Icu::UHashtable* APS5_VABI uhash_openSize_nid_postfix(Icu::HashFunction hasher, Icu::HashComparator compare, Icu::HashComparator values, std::int32_t size, std::int32_t* status) {
    return Open(hasher, compare, values, PrimeIndex(size), status);
}

Icu::UHashtable* APS5_VABI uhash_init_nid_postfix(Icu::UHashtable* hash, Icu::HashFunction hasher, Icu::HashComparator compare, Icu::HashComparator values, std::int32_t* status) {
    return Init(hash, hasher, compare, values, 4, status);
}

Icu::UHashtable* APS5_VABI uhash_initSize_nid_postfix(Icu::UHashtable* hash, Icu::HashFunction hasher, Icu::HashComparator compare, Icu::HashComparator values, std::int32_t size, std::int32_t* status) {
    return Init(hash, hasher, compare, values, PrimeIndex(size), status);
}

void APS5_VABI uhash_close_nid_postfix(Icu::UHashtable* hash) {
    if (!hash) return;
    if (hash->elements) {
        if (hash->keyDeleter || hash->valueDeleter) {
            std::int32_t position = -1;
            while (const auto* element = uhash_nextElement_nid_postfix(hash, &position)) Delete(hash, element->key, element->value);
        }
        std::free(hash->elements);
        hash->elements = nullptr;
    }
    if (hash->allocated) std::free(hash);
}

Icu::HashFunction APS5_VABI uhash_setKeyHasher_nid_postfix(Icu::UHashtable* hash, Icu::HashFunction function) {
    return std::exchange(Require(hash)->keyHasher, function);
}

Icu::HashComparator APS5_VABI uhash_setKeyComparator_nid_postfix(Icu::UHashtable* hash, Icu::HashComparator function) {
    return std::exchange(Require(hash)->keyComparator, function);
}

Icu::HashComparator APS5_VABI uhash_setValueComparator_nid_postfix(Icu::UHashtable* hash, Icu::HashComparator function) {
    return std::exchange(Require(hash)->valueComparator, function);
}

Icu::ObjectDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(Icu::UHashtable* hash, Icu::ObjectDeleter function) {
    return std::exchange(Require(hash)->keyDeleter, function);
}

Icu::ObjectDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(Icu::UHashtable* hash, Icu::ObjectDeleter function) {
    return std::exchange(Require(hash)->valueDeleter, function);
}

void APS5_VABI uhash_setResizePolicy_nid_postfix(Icu::UHashtable* hash, std::int32_t policy) {
    Require(hash);
    if (policy < 0 || policy > 2) throw std::invalid_argument("ICU hash: invalid resize policy");
    hash->lowWaterRatio = policy == 1 ? 0.1f : 0;
    hash->highWaterRatio = policy == 2 ? 1 : 0.5f;
    hash->lowWaterMark = WaterMark(hash->length, hash->lowWaterRatio);
    hash->highWaterMark = WaterMark(hash->length, hash->highWaterRatio);
    std::int32_t status = 0;
    Rehash(hash, &status);
}

std::int32_t APS5_VABI uhash_count_nid_postfix(const Icu::UHashtable* hash) {
    return Require(hash)->count;
}

void* APS5_VABI uhash_get_nid_postfix(const Icu::UHashtable* hash, const void* key) {
    return Find(hash, Pointer(key))->value.pointer;
}

void* APS5_VABI uhash_iget_nid_postfix(const Icu::UHashtable* hash, std::int32_t key) {
    return Find(hash, Integer(key))->value.pointer;
}

std::int32_t APS5_VABI uhash_geti_nid_postfix(const Icu::UHashtable* hash, const void* key) {
    return Find(hash, Pointer(key))->value.integer;
}

std::int32_t APS5_VABI uhash_igeti_nid_postfix(const Icu::UHashtable* hash, std::int32_t key) {
    return Find(hash, Integer(key))->value.integer;
}

void* APS5_VABI uhash_put_nid_postfix(Icu::UHashtable* hash, void* key, void* value, std::int32_t* status) {
    return Put(hash, Pointer(key), Pointer(value), true, status).pointer;
}

void* APS5_VABI uhash_iput_nid_postfix(Icu::UHashtable* hash, std::int32_t key, void* value, std::int32_t* status) {
    return Put(hash, Integer(key), Pointer(value), true, status).pointer;
}

std::int32_t APS5_VABI uhash_puti_nid_postfix(Icu::UHashtable* hash, void* key, std::int32_t value, std::int32_t* status) {
    return Put(hash, Pointer(key), Integer(value), false, status).integer;
}

std::int32_t APS5_VABI uhash_iputi_nid_postfix(Icu::UHashtable* hash, std::int32_t key, std::int32_t value, std::int32_t* status) {
    return Put(hash, Integer(key), Integer(value), false, status).integer;
}

void* APS5_VABI uhash_remove_nid_postfix(Icu::UHashtable* hash, const void* key) {
    return Remove(hash, Pointer(key)).pointer;
}

void* APS5_VABI uhash_iremove_nid_postfix(Icu::UHashtable* hash, std::int32_t key) {
    return Remove(hash, Integer(key)).pointer;
}

std::int32_t APS5_VABI uhash_removei_nid_postfix(Icu::UHashtable* hash, const void* key) {
    return Remove(hash, Pointer(key)).integer;
}

std::int32_t APS5_VABI uhash_iremovei_nid_postfix(Icu::UHashtable* hash, std::int32_t key) {
    return Remove(hash, Integer(key)).integer;
}

void APS5_VABI uhash_removeAll_nid_postfix(Icu::UHashtable* hash) {
    if (!Require(hash)->count) return;
    std::int32_t position = -1;
    while (const auto* element = uhash_nextElement_nid_postfix(hash, &position)) uhash_removeElement_nid_postfix(hash, element);
}

const Icu::UHashElement* APS5_VABI uhash_find_nid_postfix(const Icu::UHashtable* hash, const void* key) {
    const auto* element = Find(hash, Pointer(key));
    return element->hashcode < 0 ? nullptr : element;
}

const Icu::UHashElement* APS5_VABI uhash_nextElement_nid_postfix(const Icu::UHashtable* hash, std::int32_t* position) {
    Require(hash);
    Require(position);
    if (*position < -1 || *position == std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("ICU hash: invalid iterator position");
    for (auto i = *position + 1; i < hash->length; ++i) {
        const auto* element = &Require(hash->elements)[i];
        if (element->hashcode < 0) continue;
        *position = i;
        return element;
    }
    return nullptr;
}

void* APS5_VABI uhash_removeElement_nid_postfix(Icu::UHashtable* hash, const Icu::UHashElement* element) {
    Require(hash);
    return Require(element)->hashcode < 0 ? nullptr : RemoveElement(hash, const_cast<Icu::UHashElement*>(element)).pointer;
}

std::int8_t APS5_VABI uhash_equals_nid_postfix(const Icu::UHashtable* first, const Icu::UHashtable* second) {
    if (first == second) return 1;
    if (!first || !second || first->keyComparator != second->keyComparator ||
        first->valueComparator != second->valueComparator || !first->valueComparator || first->count != second->count) return 0;
    std::int32_t position = -1;
    for (std::int32_t i = 0; i < first->count; ++i) {
        const auto* element = Require(uhash_nextElement_nid_postfix(first, &position));
        if (!first->valueComparator(element->value, Find(second, element->key)->value)) return 0;
    }
    return 1;
}

std::int32_t APS5_VABI uhash_hashLong_nid_postfix(Icu::UHashTok key) {
    return key.integer;
}

std::int8_t APS5_VABI uhash_compareLong_nid_postfix(Icu::UHashTok left, Icu::UHashTok right) {
    return left.integer == right.integer;
}

std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(Icu::UHashTok key) {
    auto* text = static_cast<const char16_t*>(key.pointer);
    return Hash(text, Length(text));
}

std::int32_t APS5_VABI uhash_hashChars_nid_postfix(Icu::UHashTok key) {
    auto* text = static_cast<const std::uint8_t*>(key.pointer);
    return Hash(text, Length(text));
}

std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(Icu::UHashTok key) {
    auto* text = static_cast<const std::uint8_t*>(key.pointer);
    return Hash(text, Length(text), true);
}

std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(Icu::UHashTok left, Icu::UHashTok right) {
    return Equal<char16_t>(left, right);
}

std::int8_t APS5_VABI uhash_compareChars_nid_postfix(Icu::UHashTok left, Icu::UHashTok right) {
    return Equal<char>(left, right);
}

std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(Icu::UHashTok left, Icu::UHashTok right) {
    return Equal<char>(left, right, true);
}

std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t* text, std::int32_t length) {
    return Hash(text, length);
}

std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* text, std::int32_t length) {
    return Hash(reinterpret_cast<const std::uint8_t*>(text), length);
}

std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char* text, std::int32_t length) {
    return Hash(reinterpret_cast<const std::uint8_t*>(text), length, true);
}

}
