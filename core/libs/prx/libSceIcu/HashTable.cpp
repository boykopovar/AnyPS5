// Derived from icu4c/source/common/uhash.cpp, uhash_us.cpp and ustring.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uhash.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uhash_us.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ustring.cpp
//
// The upstream files' notices, verbatim (uhash.cpp):
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.
// uhash_us.cpp:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2011, International Business Machines
//   Corporation and others.  All Rights Reserved.
// ustring.cpp:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1998-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.
// uhash.h (signatures and layouts):
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2015, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstring>
#include <iterator>

namespace {

constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t Primes[] = {
    7, 13, 31, 61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749,
    65521, 131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593,
    16777213, 33554393, 67108859, 134217689, 268435399, 536870909,
    1073741789, 2147483647};
constexpr std::int32_t PrimesLength = static_cast<std::int32_t>(std::size(Primes));
constexpr std::int32_t DefaultPrimeIndex = 4;
constexpr float ResizePolicyRatios[6] = {0.0F, 0.5F, 0.1F, 0.5F, 0.0F, 1.0F};
constexpr std::int32_t HashDeleted = static_cast<std::int32_t>(0x80000000u);
constexpr std::int32_t HashEmpty = HashDeleted + 1;
constexpr std::int8_t HintKeyPointer = 1;
constexpr std::int8_t HintValuePointer = 2;

bool EmptyOrDeleted(std::int32_t hashcode) {
    return hashcode < 0;
}

UHashTok PointerToken(const void* pointer) {
    UHashTok token{};
    token.pointer = const_cast<void*>(pointer);
    return token;
}

UHashTok IntegerToken(std::int32_t integer) {
    UHashTok token{};
    token.integer = integer;
    return token;
}

template <typename T>
T* Required(T* pointer) {
    if (!pointer) APS5_INVALID_ARG_EX;
    return pointer;
}

std::int32_t HashOf(const UHashtable* hash, UHashTok key) {
    return Required(hash->keyHasher)(key);
}

void DeleteKeyValue(const UHashtable* hash, void* key, void* value) {
    if (hash->keyDeleter && key) hash->keyDeleter(key);
    if (hash->valueDeleter && value) hash->valueDeleter(value);
}

UHashTok SetElement(UHashtable* hash, UHashElement* element, std::int32_t hashcode, UHashTok key, UHashTok value, std::int8_t hint) {
    UHashTok oldValue = element->value;
    if (hash->keyDeleter && element->key.pointer && element->key.pointer != key.pointer) hash->keyDeleter(element->key.pointer);
    if (hash->valueDeleter) {
        if (oldValue.pointer && oldValue.pointer != value.pointer) hash->valueDeleter(oldValue.pointer);
        oldValue.pointer = nullptr;
    }
    if (hint & HintKeyPointer) element->key.pointer = key.pointer;
    else element->key = key;
    if (hint & HintValuePointer) element->value.pointer = value.pointer;
    else element->value = value;
    element->hashcode = hashcode;
    return oldValue;
}

UHashTok RemoveElement(UHashtable* hash, UHashElement* element) {
    --hash->count;
    return SetElement(hash, element, HashDeleted, PointerToken(nullptr), PointerToken(nullptr), 0);
}

void SetResizePolicy(UHashtable* hash, std::int32_t policy) {
    if (policy < 0 || policy > 2) APS5_INVALID_ARG_EX;
    hash->lowWaterRatio = ResizePolicyRatios[policy * 2];
    hash->highWaterRatio = ResizePolicyRatios[policy * 2 + 1];
}

void Allocate(UHashtable* hash, std::int32_t primeIndex, std::int32_t* status) {
    hash->primeIndex = static_cast<std::int8_t>(primeIndex);
    hash->length = Primes[primeIndex];
    hash->elements = static_cast<UHashElement*>(uprv_malloc_nid_postfix(sizeof(UHashElement) * static_cast<std::size_t>(hash->length)));
    if (!hash->elements) {
        *status = MemoryAllocationError;
        return;
    }
    for (std::int32_t index = 0; index < hash->length; ++index) {
        hash->elements[index].key = PointerToken(nullptr);
        hash->elements[index].value = PointerToken(nullptr);
        hash->elements[index].hashcode = HashEmpty;
    }
    hash->count = 0;
    hash->lowWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->lowWaterRatio);
    hash->highWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->highWaterRatio);
}

UHashtable* Init(UHashtable* result, UHashFunction keyHash, UKeyComparator keyComp, UValueComparator valueComp, std::int32_t primeIndex, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    Required(result);
    result->keyHasher = keyHash;
    result->keyComparator = keyComp;
    result->valueComparator = valueComp;
    result->keyDeleter = nullptr;
    result->valueDeleter = nullptr;
    result->allocated = 0;
    SetResizePolicy(result, 0);
    Allocate(result, primeIndex, status);
    if (Icu::Failure(status)) return nullptr;
    return result;
}

UHashtable* Create(UHashFunction keyHash, UKeyComparator keyComp, UValueComparator valueComp, std::int32_t primeIndex, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    auto* result = static_cast<UHashtable*>(uprv_malloc_nid_postfix(sizeof(UHashtable)));
    if (!result) {
        *status = MemoryAllocationError;
        return nullptr;
    }
    Init(result, keyHash, keyComp, valueComp, primeIndex, status);
    result->allocated = 1;
    if (Icu::Failure(status)) {
        IcuReleaseMemory_nid_no_patch(result);
        return nullptr;
    }
    return result;
}

std::int32_t PrimeIndexFor(std::int32_t size) {
    std::int32_t index = 0;
    while (index < PrimesLength - 1 && Primes[index] < size) ++index;
    return index;
}

UHashElement* Find(const UHashtable* hash, UHashTok key, std::int32_t hashcode) {
    UHashElement* elements = Required(hash->elements);
    std::int32_t firstDeleted = -1;
    std::int32_t jump = 0;
    std::int32_t tableHash = 0;
    hashcode &= 0x7FFFFFFF;
    std::int32_t startIndex = (hashcode ^ 0x4000000) % hash->length;
    std::int32_t index = startIndex;
    do {
        tableHash = elements[index].hashcode;
        if (tableHash == hashcode) {
            if (Required(hash->keyComparator)(key, elements[index].key)) return &elements[index];
        } else if (tableHash == HashEmpty) {
            break;
        } else if (EmptyOrDeleted(tableHash) && firstDeleted < 0) {
            firstDeleted = index;
        }
        if (jump == 0) jump = (hashcode % (hash->length - 1)) + 1;
        index = (index + jump) % hash->length;
    } while (index != startIndex);
    if (firstDeleted >= 0) index = firstDeleted;
    else if (tableHash != HashEmpty) APS5_INVALID_ARG_EX;
    return &elements[index];
}

void Rehash(UHashtable* hash, std::int32_t* status) {
    UHashElement* old = hash->elements;
    std::int32_t oldLength = hash->length;
    std::int32_t newPrimeIndex = hash->primeIndex;
    if (hash->count > hash->highWaterMark) {
        if (++newPrimeIndex >= PrimesLength) return;
    } else if (hash->count < hash->lowWaterMark) {
        if (--newPrimeIndex < 0) return;
    } else {
        return;
    }
    Allocate(hash, newPrimeIndex, status);
    if (Icu::Failure(status)) {
        hash->elements = old;
        hash->length = oldLength;
        return;
    }
    for (std::int32_t index = oldLength - 1; index >= 0; --index) {
        if (EmptyOrDeleted(old[index].hashcode)) continue;
        UHashElement* element = Find(hash, old[index].key, old[index].hashcode);
        element->key = old[index].key;
        element->value = old[index].value;
        element->hashcode = old[index].hashcode;
        ++hash->count;
    }
    IcuReleaseMemory_nid_no_patch(old);
}

UHashTok Remove(UHashtable* hash, UHashTok key) {
    Required(hash);
    UHashElement* element = Find(hash, key, HashOf(hash, key));
    UHashTok result = PointerToken(nullptr);
    if (!EmptyOrDeleted(element->hashcode)) {
        result = RemoveElement(hash, element);
        if (hash->count < hash->lowWaterMark) {
            std::int32_t status = 0;
            Rehash(hash, &status);
        }
    }
    return result;
}

UHashTok Put(UHashtable* hash, UHashTok key, UHashTok value, std::int8_t hint, std::int32_t* status) {
    Required(hash);
    if (!Icu::Failure(status)) {
        if ((hint & HintValuePointer) && !value.pointer) return Remove(hash, key);
        if (hash->count > hash->highWaterMark) Rehash(hash, status);
        if (!Icu::Failure(status)) {
            std::int32_t hashcode = HashOf(hash, key);
            UHashElement* element = Find(hash, key, hashcode);
            if (EmptyOrDeleted(element->hashcode)) {
                ++hash->count;
                if (hash->count == hash->length) {
                    --hash->count;
                    *status = MemoryAllocationError;
                }
            }
            if (!Icu::Failure(status)) return SetElement(hash, element, hashcode & 0x7FFFFFFF, key, value, hint);
        }
    }
    DeleteKeyValue(hash, key.pointer, value.pointer);
    return PointerToken(nullptr);
}

const UHashElement* Lookup(const UHashtable* hash, UHashTok key) {
    Required(hash);
    return Find(hash, key, HashOf(hash, key));
}

char AsciiLower(char c) {
    if (0x41 <= c && c <= 0x5a) c = static_cast<char>(c + 0x20);
    return c;
}

template <typename Character, typename Equal>
std::int8_t CompareStrings(UHashTok key1, UHashTok key2, Equal equal) {
    const auto* p1 = static_cast<const Character*>(key1.pointer);
    const auto* p2 = static_cast<const Character*>(key2.pointer);
    if (p1 == p2) return 1;
    if (!p1 || !p2) return 0;
    while (*p1 != 0 && equal(*p1, *p2)) {
        ++p1;
        ++p2;
    }
    return *p1 == *p2;
}

}

extern "C" {

UHashtable* APS5_VABI uhash_open_nid_postfix(UHashFunction keyHash, UKeyComparator keyComp, UValueComparator valueComp, std::int32_t* status) {
    return Create(keyHash, keyComp, valueComp, DefaultPrimeIndex, status);
}

UHashtable* APS5_VABI uhash_openSize_nid_postfix(UHashFunction keyHash, UKeyComparator keyComp, UValueComparator valueComp, std::int32_t size, std::int32_t* status) {
    return Create(keyHash, keyComp, valueComp, PrimeIndexFor(size), status);
}

UHashtable* APS5_VABI uhash_init_nid_postfix(UHashtable* fillinResult, UHashFunction keyHash, UKeyComparator keyComp, UValueComparator valueComp, std::int32_t* status) {
    return Init(fillinResult, keyHash, keyComp, valueComp, DefaultPrimeIndex, status);
}

UHashtable* APS5_VABI uhash_initSize_nid_postfix(UHashtable* fillinResult, UHashFunction keyHash, UKeyComparator keyComp, UValueComparator valueComp, std::int32_t size, std::int32_t* status) {
    return Init(fillinResult, keyHash, keyComp, valueComp, PrimeIndexFor(size), status);
}

void APS5_VABI uhash_close_nid_postfix(UHashtable* hash) {
    if (!hash) return;
    if (hash->elements) {
        if (hash->keyDeleter || hash->valueDeleter) {
            std::int32_t position = -1;
            while (const UHashElement* element = uhash_nextElement_nid_postfix(hash, &position)) DeleteKeyValue(hash, element->key.pointer, element->value.pointer);
        }
        IcuReleaseMemory_nid_no_patch(hash->elements);
        hash->elements = nullptr;
    }
    if (hash->allocated) IcuReleaseMemory_nid_no_patch(hash);
}

UHashFunction APS5_VABI uhash_setKeyHasher_nid_postfix(UHashtable* hash, UHashFunction function) {
    UHashFunction result = Required(hash)->keyHasher;
    hash->keyHasher = function;
    return result;
}

UKeyComparator APS5_VABI uhash_setKeyComparator_nid_postfix(UHashtable* hash, UKeyComparator function) {
    UKeyComparator result = Required(hash)->keyComparator;
    hash->keyComparator = function;
    return result;
}

UValueComparator APS5_VABI uhash_setValueComparator_nid_postfix(UHashtable* hash, UValueComparator function) {
    UValueComparator result = Required(hash)->valueComparator;
    hash->valueComparator = function;
    return result;
}

GuestUObjectDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(UHashtable* hash, GuestUObjectDeleter function) {
    GuestUObjectDeleter result = Required(hash)->keyDeleter;
    hash->keyDeleter = function;
    return result;
}

GuestUObjectDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(UHashtable* hash, GuestUObjectDeleter function) {
    GuestUObjectDeleter result = Required(hash)->valueDeleter;
    hash->valueDeleter = function;
    return result;
}

void APS5_VABI uhash_setResizePolicy_nid_postfix(UHashtable* hash, std::int32_t policy) {
    std::int32_t status = 0;
    SetResizePolicy(Required(hash), policy);
    hash->lowWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->lowWaterRatio);
    hash->highWaterMark = static_cast<std::int32_t>(static_cast<float>(hash->length) * hash->highWaterRatio);
    Rehash(hash, &status);
}

std::int32_t APS5_VABI uhash_count_nid_postfix(const UHashtable* hash) {
    return Required(hash)->count;
}

void* APS5_VABI uhash_get_nid_postfix(const UHashtable* hash, const void* key) {
    return Lookup(hash, PointerToken(key))->value.pointer;
}

void* APS5_VABI uhash_iget_nid_postfix(const UHashtable* hash, std::int32_t key) {
    return Lookup(hash, IntegerToken(key))->value.pointer;
}

std::int32_t APS5_VABI uhash_geti_nid_postfix(const UHashtable* hash, const void* key) {
    return Lookup(hash, PointerToken(key))->value.integer;
}

std::int32_t APS5_VABI uhash_igeti_nid_postfix(const UHashtable* hash, std::int32_t key) {
    return Lookup(hash, IntegerToken(key))->value.integer;
}

void* APS5_VABI uhash_put_nid_postfix(UHashtable* hash, void* key, void* value, std::int32_t* status) {
    return Put(hash, PointerToken(key), PointerToken(value), HintKeyPointer | HintValuePointer, status).pointer;
}

void* APS5_VABI uhash_iput_nid_postfix(UHashtable* hash, std::int32_t key, void* value, std::int32_t* status) {
    return Put(hash, IntegerToken(key), PointerToken(value), HintValuePointer, status).pointer;
}

std::int32_t APS5_VABI uhash_puti_nid_postfix(UHashtable* hash, void* key, std::int32_t value, std::int32_t* status) {
    return Put(hash, PointerToken(key), IntegerToken(value), HintKeyPointer, status).integer;
}

std::int32_t APS5_VABI uhash_iputi_nid_postfix(UHashtable* hash, std::int32_t key, std::int32_t value, std::int32_t* status) {
    return Put(hash, IntegerToken(key), IntegerToken(value), 0, status).integer;
}

void* APS5_VABI uhash_remove_nid_postfix(UHashtable* hash, const void* key) {
    return Remove(hash, PointerToken(key)).pointer;
}

void* APS5_VABI uhash_iremove_nid_postfix(UHashtable* hash, std::int32_t key) {
    return Remove(hash, IntegerToken(key)).pointer;
}

std::int32_t APS5_VABI uhash_removei_nid_postfix(UHashtable* hash, const void* key) {
    return Remove(hash, PointerToken(key)).integer;
}

std::int32_t APS5_VABI uhash_iremovei_nid_postfix(UHashtable* hash, std::int32_t key) {
    return Remove(hash, IntegerToken(key)).integer;
}

void APS5_VABI uhash_removeAll_nid_postfix(UHashtable* hash) {
    if (Required(hash)->count == 0) return;
    std::int32_t position = -1;
    while (const UHashElement* element = uhash_nextElement_nid_postfix(hash, &position)) uhash_removeElement_nid_postfix(hash, element);
}

const UHashElement* APS5_VABI uhash_find_nid_postfix(const UHashtable* hash, const void* key) {
    const UHashElement* element = Lookup(hash, PointerToken(key));
    return EmptyOrDeleted(element->hashcode) ? nullptr : element;
}

const UHashElement* APS5_VABI uhash_nextElement_nid_postfix(const UHashtable* hash, std::int32_t* position) {
    Required(hash);
    for (std::int32_t index = *Required(position) + 1; index < hash->length; ++index) {
        if (EmptyOrDeleted(hash->elements[index].hashcode)) continue;
        *position = index;
        return &hash->elements[index];
    }
    return nullptr;
}

void* APS5_VABI uhash_removeElement_nid_postfix(UHashtable* hash, const UHashElement* element) {
    Required(hash);
    if (EmptyOrDeleted(Required(element)->hashcode)) return nullptr;
    return RemoveElement(hash, const_cast<UHashElement*>(element)).pointer;
}

std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(UHashTok key) {
    const auto* string = static_cast<const char16_t*>(key.pointer);
    if (!string) return 0;
    std::int32_t length = 0;
    while (string[length] != 0) ++length;
    return ustr_hashUCharsN_nid_postfix(reinterpret_cast<const std::uint16_t*>(string), length);
}

std::int32_t APS5_VABI uhash_hashChars_nid_postfix(UHashTok key) {
    const auto* string = static_cast<const char*>(key.pointer);
    return string ? ustr_hashCharsN_nid_postfix(string, static_cast<std::int32_t>(std::strlen(string))) : 0;
}

std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(UHashTok key) {
    const auto* string = static_cast<const char*>(key.pointer);
    return string ? ustr_hashICharsN_nid_postfix(string, static_cast<std::int32_t>(std::strlen(string))) : 0;
}

std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(UHashTok key1, UHashTok key2) {
    return CompareStrings<char16_t>(key1, key2, [](char16_t a, char16_t b) { return a == b; });
}

std::int8_t APS5_VABI uhash_compareChars_nid_postfix(UHashTok key1, UHashTok key2) {
    return CompareStrings<char>(key1, key2, [](char a, char b) { return a == b; });
}

std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(UHashTok key1, UHashTok key2) {
    return CompareStrings<char>(key1, key2, [](char a, char b) { return AsciiLower(a) == AsciiLower(b); });
}

std::int32_t APS5_VABI uhash_hashLong_nid_postfix(UHashTok key) {
    return key.integer;
}

std::int8_t APS5_VABI uhash_compareLong_nid_postfix(UHashTok key1, UHashTok key2) {
    return key1.integer == key2.integer;
}

void APS5_VABI uhash_deleteHashtable_nid_postfix(void* object) {
    if (!object) return;
    auto* hashtable = static_cast<UHashtableObject*>(object);
    if (hashtable->hash) uhash_close_nid_postfix(hashtable->hash);
    IcuReleaseMemory_nid_no_patch(hashtable);
}

std::int8_t APS5_VABI uhash_equals_nid_postfix(const UHashtable* hash1, const UHashtable* hash2) {
    if (hash1 == hash2) return 1;
    if (!hash1 || !hash2 || hash1->keyComparator != hash2->keyComparator || hash1->valueComparator != hash2->valueComparator || !hash1->valueComparator) return 0;
    std::int32_t count = hash1->count;
    if (count != hash2->count) return 0;
    std::int32_t position = -1;
    for (std::int32_t index = 0; index < count; ++index) {
        const UHashElement* element = uhash_nextElement_nid_postfix(hash1, &position);
        if (!hash1->valueComparator(element->value, Lookup(hash2, element->key)->value)) return 0;
    }
    return 1;
}

}
