// Derived from icu4c/source/common/uhash.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the open
// addressing hash table, its public API and the built-in key hash and comparator functions below.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uhash.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.

#include "prx/libSceIcu/UHash.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

static_assert(sizeof(UHashElement) == 24);
static_assert(offsetof(UHashElement, hashcode) == 0);
static_assert(offsetof(UHashElement, value) == 8);
static_assert(offsetof(UHashElement, key) == 16);
static_assert(sizeof(UHashtable) == 80);
static_assert(offsetof(UHashtable, elements) == 0);
static_assert(offsetof(UHashtable, keyHasher) == 8);
static_assert(offsetof(UHashtable, keyComparator) == 16);
static_assert(offsetof(UHashtable, valueComparator) == 24);
static_assert(offsetof(UHashtable, keyDeleter) == 32);
static_assert(offsetof(UHashtable, valueDeleter) == 40);
static_assert(offsetof(UHashtable, count) == 48);
static_assert(offsetof(UHashtable, length) == 52);
static_assert(offsetof(UHashtable, highWaterMark) == 56);
static_assert(offsetof(UHashtable, lowWaterMark) == 60);
static_assert(offsetof(UHashtable, highWaterRatio) == 64);
static_assert(offsetof(UHashtable, lowWaterRatio) == 68);
static_assert(offsetof(UHashtable, primeIndex) == 72);
static_assert(offsetof(UHashtable, allocated) == 73);

namespace {

constexpr std::int32_t Primes[] = {
    7, 13, 31, 61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749, 65521, 131071, 262139, 524287, 1048573,
    2097143, 4194301, 8388593, 16777213, 33554393, 67108859, 134217689, 268435399, 536870909, 1073741789, 2147483647,
};
constexpr std::int32_t PrimesLength = static_cast<std::int32_t>(sizeof(Primes) / sizeof(Primes[0]));
constexpr std::int32_t DefaultPrimeIndex = 4;
constexpr float ResizePolicyRatio[] = {0.0F, 0.5F, 0.1F, 0.5F, 0.0F, 1.0F};
constexpr std::int32_t HashDeleted = static_cast<std::int32_t>(0x80000000u);
constexpr std::int32_t HashEmpty = HashDeleted + 1;
constexpr std::int32_t HintKeyPointer = 1;
constexpr std::int32_t HintValuePointer = 2;
constexpr std::int32_t Grow = 0;
constexpr std::int32_t MemoryAllocation = 7;

bool Failed(const std::int32_t* status) {
    return status == nullptr || *status > 0;
}

bool EmptyOrDeleted(std::int32_t hashcode) {
    return hashcode < 0;
}

UHashTok EmptyToken() {
    UHashTok token;
    token.pointer = nullptr;
    return token;
}

std::size_t StringLength(const char* string) {
    std::size_t length = 0;
    while (string[length] != 0) ++length;
    return length;
}

std::size_t WideLength(const char16_t* string) {
    std::size_t length = 0;
    while (string[length] != 0) ++length;
    return length;
}

char LowerAscii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + 0x20) : value;
}

std::int32_t HashBytes(const char* string, std::int32_t length, bool foldCase) {
    std::uint32_t hash = 0;
    if (string != nullptr) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(string);
        const std::int32_t step = ((length - 32) / 32) + 1;
        for (std::int32_t index = 0; index < length; index += step) {
            const std::uint8_t value = bytes[index];
            hash = hash * 37 + (foldCase ? static_cast<std::uint8_t>(LowerAscii(static_cast<char>(value))) : value);
        }
    }
    return static_cast<std::int32_t>(hash);
}

std::int32_t HashWide(const char16_t* string, std::int32_t length) {
    std::uint32_t hash = 0;
    if (string != nullptr) {
        const std::int32_t step = ((length - 32) / 32) + 1;
        for (std::int32_t index = 0; index < length; index += step) {
            hash = hash * 37 + static_cast<std::uint16_t>(string[index]);
        }
    }
    return static_cast<std::int32_t>(hash);
}

void DeleteKeyValue(UHashtable* hash, void* key, void* value) {
    if (hash->keyDeleter != nullptr && key != nullptr) hash->keyDeleter(key);
    if (hash->valueDeleter != nullptr && value != nullptr) hash->valueDeleter(value);
}

UHashTok SetElement(UHashtable* hash, UHashElement* element, std::int32_t hashcode, UHashTok key, UHashTok value,
    std::int32_t hint) {
    UHashTok oldValue = element->value;
    if (hash->keyDeleter != nullptr && element->key.pointer != nullptr && element->key.pointer != key.pointer) {
        hash->keyDeleter(element->key.pointer);
    }
    if (hash->valueDeleter != nullptr) {
        if (oldValue.pointer != nullptr && oldValue.pointer != value.pointer) {
            hash->valueDeleter(oldValue.pointer);
        }
        oldValue.pointer = nullptr;
    }
    if ((hint & HintKeyPointer) != 0) {
        element->key.pointer = key.pointer;
    } else {
        element->key = key;
    }
    if ((hint & HintValuePointer) != 0) {
        element->value.pointer = value.pointer;
    } else {
        element->value = value;
    }
    element->hashcode = hashcode;
    return oldValue;
}

UHashTok InternalRemoveElement(UHashtable* hash, UHashElement* element) {
    --hash->count;
    const UHashTok empty = EmptyToken();
    return SetElement(hash, element, HashDeleted, empty, empty, 0);
}

void InternalSetResizePolicy(UHashtable* hash, std::int32_t policy) {
    hash->lowWaterRatio = ResizePolicyRatio[policy * 2];
    hash->highWaterRatio = ResizePolicyRatio[policy * 2 + 1];
}

void Allocate(UHashtable* hash, std::int32_t primeIndex, std::int32_t* status) {
    if (Failed(status)) return;
    hash->primeIndex = static_cast<std::int8_t>(primeIndex);
    hash->length = Primes[primeIndex];
    auto* elements = static_cast<UHashElement*>(
        uprv_malloc_nid_postfix(static_cast<std::size_t>(hash->length) * sizeof(UHashElement)));
    hash->elements = elements;
    if (elements == nullptr) {
        *status = MemoryAllocation;
        return;
    }
    const UHashTok empty = EmptyToken();
    for (std::int32_t index = 0; index < hash->length; ++index) {
        elements[index].key = empty;
        elements[index].value = empty;
        elements[index].hashcode = HashEmpty;
    }
    hash->count = 0;
    hash->lowWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->lowWaterRatio);
    hash->highWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->highWaterRatio);
}

UHashtable* Init(UHashtable* result, UHashFunction keyHash, UKeyComparator keyComparator,
    UValueComparator valueComparator, std::int32_t primeIndex, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (keyHash == nullptr || keyComparator == nullptr) {
        *status = Icu::IllegalArgument;
        return nullptr;
    }
    result->keyHasher = keyHash;
    result->keyComparator = keyComparator;
    result->valueComparator = valueComparator;
    result->keyDeleter = nullptr;
    result->valueDeleter = nullptr;
    result->allocated = 0;
    InternalSetResizePolicy(result, Grow);
    Allocate(result, primeIndex, status);
    if (Failed(status)) return nullptr;
    return result;
}

UHashtable* Create(UHashFunction keyHash, UKeyComparator keyComparator, UValueComparator valueComparator,
    std::int32_t primeIndex, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    auto* result = static_cast<UHashtable*>(uprv_malloc_nid_postfix(sizeof(UHashtable)));
    if (result == nullptr) {
        *status = MemoryAllocation;
        return nullptr;
    }
    Init(result, keyHash, keyComparator, valueComparator, primeIndex, status);
    result->allocated = 1;
    if (Failed(status)) {
        IcuReleaseMemory_nid_no_patch(result);
        return nullptr;
    }
    return result;
}

UHashElement* Find(const UHashtable* hash, UHashTok key, std::int32_t hashcode) {
    std::int32_t firstDeleted = -1;
    std::int32_t jump = 0;
    UHashElement* elements = hash->elements;
    hashcode &= 0x7FFFFFFF;
    std::int32_t startIndex = (hashcode ^ 0x4000000) % hash->length;
    std::int32_t index = startIndex;
    std::int32_t tableHash = HashEmpty;
    do {
        tableHash = elements[index].hashcode;
        if (tableHash == hashcode) {
            // The comparators are equality predicates, as in ICU's uhash.cpp: a nonzero result means
            // "these keys are equal". A colliding slot whose key differs is skipped, not returned.
            if (hash->keyComparator != nullptr && hash->keyComparator(key, elements[index].key) != 0) {
                return &elements[index];
            }
        } else if (tableHash == HashEmpty) {
            break;
        } else if (tableHash == HashDeleted && firstDeleted < 0) {
            firstDeleted = index;
        }
        if (jump == 0) jump = (hashcode % (hash->length - 1)) + 1;
        index = (index + jump) % hash->length;
    } while (index != startIndex);
    if (firstDeleted >= 0) {
        index = firstDeleted;
    } else if (tableHash != HashEmpty) {
        return nullptr;
    }
    return &elements[index];
}

void Rehash(UHashtable* hash, std::int32_t* status) {
    UHashElement* old = hash->elements;
    const std::int32_t oldLength = hash->length;
    std::int32_t newPrimeIndex = hash->primeIndex;
    if (hash->count > hash->highWaterMark) {
        if (++newPrimeIndex >= PrimesLength) return;
    } else if (hash->count < hash->lowWaterMark) {
        if (--newPrimeIndex < 0) return;
    } else {
        return;
    }
    Allocate(hash, newPrimeIndex, status);
    if (Failed(status)) {
        hash->elements = old;
        hash->length = oldLength;
        return;
    }
    for (std::int32_t index = oldLength - 1; index >= 0; --index) {
        if (!EmptyOrDeleted(old[index].hashcode)) {
            UHashElement* element = Find(hash, old[index].key, old[index].hashcode);
            element->key = old[index].key;
            element->value = old[index].value;
            element->hashcode = old[index].hashcode;
            ++hash->count;
        }
    }
    IcuReleaseMemory_nid_no_patch(old);
}

UHashTok Remove(UHashtable* hash, UHashTok key) {
    UHashElement* element = Find(hash, key, hash->keyHasher(key));
    if (element != nullptr && !EmptyOrDeleted(element->hashcode)) {
        const UHashTok result = InternalRemoveElement(hash, element);
        if (hash->count < hash->lowWaterMark) {
            std::int32_t status = 0;
            Rehash(hash, &status);
        }
        return result;
    }
    return EmptyToken();
}

UHashTok Put(UHashtable* hash, UHashTok key, UHashTok value, std::int32_t hint, std::int32_t* status) {
    if (Failed(status)) {
        DeleteKeyValue(hash, key.pointer, value.pointer);
        return EmptyToken();
    }
    if ((hint & HintValuePointer) != 0 && value.pointer == nullptr) return Remove(hash, key);
    if (hash->count > hash->highWaterMark) {
        Rehash(hash, status);
        if (Failed(status)) {
            DeleteKeyValue(hash, key.pointer, value.pointer);
            return EmptyToken();
        }
    }
    const std::int32_t hashcode = hash->keyHasher(key);
    UHashElement* element = Find(hash, key, hashcode);
    if (element == nullptr) {
        *status = MemoryAllocation;
        DeleteKeyValue(hash, key.pointer, value.pointer);
        return EmptyToken();
    }
    if (EmptyOrDeleted(element->hashcode)) {
        ++hash->count;
        if (hash->count == hash->length) {
            --hash->count;
            *status = MemoryAllocation;
            DeleteKeyValue(hash, key.pointer, value.pointer);
            return EmptyToken();
        }
    }
    return SetElement(hash, element, hashcode & 0x7FFFFFFF, key, value, hint);
}

}

extern "C" {

UHashtable* APS5_VABI uhash_open_nid_postfix(UHashFunction keyHash, UKeyComparator keyComparator,
    UValueComparator valueComparator, std::int32_t* status) {
    return Create(keyHash, keyComparator, valueComparator, DefaultPrimeIndex, status);
}

UHashtable* APS5_VABI uhash_openSize_nid_postfix(UHashFunction keyHash, UKeyComparator keyComparator,
    UValueComparator valueComparator, std::int32_t size, std::int32_t* status) {
    std::int32_t index = 0;
    while (index < (PrimesLength - 1) && Primes[index] < size) ++index;
    return Create(keyHash, keyComparator, valueComparator, index, status);
}

UHashtable* APS5_VABI uhash_init_nid_postfix(UHashtable* result, UHashFunction keyHash, UKeyComparator keyComparator,
    UValueComparator valueComparator, std::int32_t* status) {
    return Init(result, keyHash, keyComparator, valueComparator, DefaultPrimeIndex, status);
}

UHashtable* APS5_VABI uhash_initSize_nid_postfix(UHashtable* result, UHashFunction keyHash,
    UKeyComparator keyComparator, UValueComparator valueComparator, std::int32_t size, std::int32_t* status) {
    std::int32_t index = 0;
    while (index < (PrimesLength - 1) && Primes[index] < size) ++index;
    return Init(result, keyHash, keyComparator, valueComparator, index, status);
}

void APS5_VABI uhash_close_nid_postfix(UHashtable* hash) {
    if (hash == nullptr) return;
    if (hash->elements != nullptr) {
        if (hash->keyDeleter != nullptr || hash->valueDeleter != nullptr) {
            std::int32_t position = -1;
            UHashElement* element = nullptr;
            while ((element = const_cast<UHashElement*>(uhash_nextElement_nid_postfix(hash, &position))) != nullptr) {
                DeleteKeyValue(hash, element->key.pointer, element->value.pointer);
            }
        }
        IcuReleaseMemory_nid_no_patch(hash->elements);
        hash->elements = nullptr;
    }
    if (hash->allocated != 0) IcuReleaseMemory_nid_no_patch(hash);
}

void APS5_VABI uhash_deleteHashtable_nid_postfix(void* object) {
    uhash_close_nid_postfix(static_cast<UHashtable*>(object));
}

UHashFunction APS5_VABI uhash_setKeyHasher_nid_postfix(UHashtable* hash, UHashFunction function) {
    UHashFunction previous = hash->keyHasher;
    hash->keyHasher = function;
    return previous;
}

UKeyComparator APS5_VABI uhash_setKeyComparator_nid_postfix(UHashtable* hash, UKeyComparator function) {
    UKeyComparator previous = hash->keyComparator;
    hash->keyComparator = function;
    return previous;
}

UValueComparator APS5_VABI uhash_setValueComparator_nid_postfix(UHashtable* hash, UValueComparator function) {
    UValueComparator previous = hash->valueComparator;
    hash->valueComparator = function;
    return previous;
}

UObjectDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(UHashtable* hash, UObjectDeleter function) {
    UObjectDeleter previous = hash->keyDeleter;
    hash->keyDeleter = function;
    return previous;
}

UObjectDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(UHashtable* hash, UObjectDeleter function) {
    UObjectDeleter previous = hash->valueDeleter;
    hash->valueDeleter = function;
    return previous;
}

void APS5_VABI uhash_setResizePolicy_nid_postfix(UHashtable* hash, std::int32_t policy) {
    if (policy < 0 || policy > 2) return;
    std::int32_t status = 0;
    InternalSetResizePolicy(hash, policy);
    hash->lowWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->lowWaterRatio);
    hash->highWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->highWaterRatio);
    Rehash(hash, &status);
}

std::int32_t APS5_VABI uhash_count_nid_postfix(const UHashtable* hash) {
    return hash->count;
}

void* APS5_VABI uhash_get_nid_postfix(const UHashtable* hash, const void* key) {
    UHashTok holder;
    holder.pointer = const_cast<void*>(key);
    const UHashElement* element = Find(hash, holder, hash->keyHasher(holder));
    return element == nullptr ? nullptr : element->value.pointer;
}

void* APS5_VABI uhash_iget_nid_postfix(const UHashtable* hash, std::int32_t key) {
    UHashTok holder;
    holder.integer = key;
    const UHashElement* element = Find(hash, holder, hash->keyHasher(holder));
    return element == nullptr ? nullptr : element->value.pointer;
}

std::int32_t APS5_VABI uhash_geti_nid_postfix(const UHashtable* hash, const void* key) {
    UHashTok holder;
    holder.pointer = const_cast<void*>(key);
    const UHashElement* element = Find(hash, holder, hash->keyHasher(holder));
    return element == nullptr ? 0 : element->value.integer;
}

std::int32_t APS5_VABI uhash_igeti_nid_postfix(const UHashtable* hash, std::int32_t key) {
    UHashTok holder;
    holder.integer = key;
    const UHashElement* element = Find(hash, holder, hash->keyHasher(holder));
    return element == nullptr ? 0 : element->value.integer;
}

void* APS5_VABI uhash_put_nid_postfix(UHashtable* hash, void* key, void* value, std::int32_t* status) {
    UHashTok keyHolder;
    UHashTok valueHolder;
    keyHolder.pointer = key;
    valueHolder.pointer = value;
    return Put(hash, keyHolder, valueHolder, HintKeyPointer | HintValuePointer, status).pointer;
}

void* APS5_VABI uhash_iput_nid_postfix(UHashtable* hash, std::int32_t key, void* value, std::int32_t* status) {
    UHashTok keyHolder;
    UHashTok valueHolder;
    keyHolder.integer = key;
    valueHolder.pointer = value;
    return Put(hash, keyHolder, valueHolder, HintValuePointer, status).pointer;
}

std::int32_t APS5_VABI uhash_puti_nid_postfix(UHashtable* hash, void* key, std::int32_t value, std::int32_t* status) {
    UHashTok keyHolder;
    UHashTok valueHolder;
    keyHolder.pointer = key;
    valueHolder.integer = value;
    return Put(hash, keyHolder, valueHolder, HintKeyPointer, status).integer;
}

std::int32_t APS5_VABI uhash_iputi_nid_postfix(UHashtable* hash, std::int32_t key, std::int32_t value,
    std::int32_t* status) {
    UHashTok keyHolder;
    UHashTok valueHolder;
    keyHolder.integer = key;
    valueHolder.integer = value;
    return Put(hash, keyHolder, valueHolder, 0, status).integer;
}

void* APS5_VABI uhash_remove_nid_postfix(UHashtable* hash, const void* key) {
    UHashTok holder;
    holder.pointer = const_cast<void*>(key);
    return Remove(hash, holder).pointer;
}

void* APS5_VABI uhash_iremove_nid_postfix(UHashtable* hash, std::int32_t key) {
    UHashTok holder;
    holder.integer = key;
    return Remove(hash, holder).pointer;
}

std::int32_t APS5_VABI uhash_removei_nid_postfix(UHashtable* hash, const void* key) {
    UHashTok holder;
    holder.pointer = const_cast<void*>(key);
    return Remove(hash, holder).integer;
}

std::int32_t APS5_VABI uhash_iremovei_nid_postfix(UHashtable* hash, std::int32_t key) {
    UHashTok holder;
    holder.integer = key;
    return Remove(hash, holder).integer;
}

void APS5_VABI uhash_removeAll_nid_postfix(UHashtable* hash) {
    std::int32_t position = -1;
    const UHashElement* element = nullptr;
    if (hash->count == 0) return;
    while ((element = uhash_nextElement_nid_postfix(hash, &position)) != nullptr) {
        uhash_removeElement_nid_postfix(hash, element);
    }
}

const UHashElement* APS5_VABI uhash_find_nid_postfix(const UHashtable* hash, const void* key) {
    UHashTok holder;
    holder.pointer = const_cast<void*>(key);
    const UHashElement* element = Find(hash, holder, hash->keyHasher(holder));
    return element == nullptr || EmptyOrDeleted(element->hashcode) ? nullptr : element;
}

const UHashElement* APS5_VABI uhash_nextElement_nid_postfix(const UHashtable* hash, std::int32_t* position) {
    for (std::int32_t index = *position + 1; index < hash->length; ++index) {
        if (!EmptyOrDeleted(hash->elements[index].hashcode)) {
            *position = index;
            return &hash->elements[index];
        }
    }
    return nullptr;
}

void* APS5_VABI uhash_removeElement_nid_postfix(UHashtable* hash, const UHashElement* element) {
    if (element != nullptr && !EmptyOrDeleted(element->hashcode)) {
        return InternalRemoveElement(hash, const_cast<UHashElement*>(element)).pointer;
    }
    return nullptr;
}

std::int8_t APS5_VABI uhash_equals_nid_postfix(const UHashtable* first, const UHashtable* second) {
    if (first == second) return 1;
    if (first == nullptr || second == nullptr || first->keyComparator != second->keyComparator ||
        first->valueComparator != second->valueComparator || first->valueComparator == nullptr) {
        return 0;
    }
    const std::int32_t count = uhash_count_nid_postfix(first);
    if (count != uhash_count_nid_postfix(second)) return 0;
    std::int32_t position = -1;
    for (std::int32_t index = 0; index < count; ++index) {
        const UHashElement* element = uhash_nextElement_nid_postfix(first, &position);
        const UHashTok key = element->key;
        const UHashTok value = element->value;
        const UHashElement* other = Find(second, key, second->keyHasher(key));
        if (other == nullptr || EmptyOrDeleted(other->hashcode)) return 0;
        // The value comparator is an equality predicate as well: zero means the values differ, which
        // is the same direction ICU's uhash_equals uses.
        if (first->valueComparator(value, other->value) == 0) return 0;
    }
    return 1;
}

std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(const UHashTok key) {
    const auto* string = static_cast<const char16_t*>(key.pointer);
    return string == nullptr ? 0 : HashWide(string, static_cast<std::int32_t>(WideLength(string)));
}

std::int32_t APS5_VABI uhash_hashChars_nid_postfix(const UHashTok key) {
    const auto* string = static_cast<const char*>(key.pointer);
    return string == nullptr ? 0 : HashBytes(string, static_cast<std::int32_t>(StringLength(string)), false);
}

std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(const UHashTok key) {
    const auto* string = static_cast<const char*>(key.pointer);
    return string == nullptr ? 0 : HashBytes(string, static_cast<std::int32_t>(StringLength(string)), true);
}

std::int32_t APS5_VABI uhash_hashLong_nid_postfix(const UHashTok key) {
    return key.integer;
}

std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(const UHashTok first, const UHashTok second) {
    const auto* left = static_cast<const char16_t*>(first.pointer);
    const auto* right = static_cast<const char16_t*>(second.pointer);
    if (left == right) return 1;
    if (left == nullptr || right == nullptr) return 0;
    while (*left != 0 && *left == *right) {
        ++left;
        ++right;
    }
    return static_cast<std::int8_t>(*left == *right);
}

std::int8_t APS5_VABI uhash_compareChars_nid_postfix(const UHashTok first, const UHashTok second) {
    const auto* left = static_cast<const char*>(first.pointer);
    const auto* right = static_cast<const char*>(second.pointer);
    if (left == right) return 1;
    if (left == nullptr || right == nullptr) return 0;
    while (*left != 0 && *left == *right) {
        ++left;
        ++right;
    }
    return static_cast<std::int8_t>(*left == *right);
}

std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(const UHashTok first, const UHashTok second) {
    const auto* left = static_cast<const char*>(first.pointer);
    const auto* right = static_cast<const char*>(second.pointer);
    if (left == right) return 1;
    if (left == nullptr || right == nullptr) return 0;
    while (*left != 0 && LowerAscii(*left) == LowerAscii(*right)) {
        ++left;
        ++right;
    }
    return static_cast<std::int8_t>(*left == *right);
}

std::int8_t APS5_VABI uhash_compareLong_nid_postfix(const UHashTok first, const UHashTok second) {
    return static_cast<std::int8_t>(first.integer == second.integer);
}

}
