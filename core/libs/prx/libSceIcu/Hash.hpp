// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

union UHashTok {
    void* pointer;
    std::int32_t integer;
};

struct UHashElement {
    std::int32_t hashcode;
    UHashTok value;
    UHashTok key;
};

using HashFunction = std::int32_t (APS5_VABI*)(UHashTok);
using HashComparator = std::int8_t (APS5_VABI*)(UHashTok, UHashTok);
using ObjectDeleter = void (APS5_VABI*)(void*);

struct UHashtable {
    UHashElement* elements;
    HashFunction keyHasher;
    HashComparator keyComparator;
    HashComparator valueComparator;
    ObjectDeleter keyDeleter;
    ObjectDeleter valueDeleter;
    std::int32_t count;
    std::int32_t length;
    std::int32_t highWaterMark;
    std::int32_t lowWaterMark;
    float highWaterRatio;
    float lowWaterRatio;
    std::int8_t primeIndex;
    std::int8_t allocated;
};

static_assert(sizeof(UHashTok) == 8 && alignof(UHashTok) == 8);
static_assert(offsetof(UHashTok, pointer) == 0 && offsetof(UHashTok, integer) == 0);
static_assert(sizeof(UHashElement) == 24 && alignof(UHashElement) == 8);
static_assert(offsetof(UHashElement, hashcode) == 0);
static_assert(offsetof(UHashElement, value) == 8 && offsetof(UHashElement, key) == 16);
static_assert(sizeof(UHashtable) == 80 && alignof(UHashtable) == 8);
static_assert(offsetof(UHashtable, elements) == 0 && offsetof(UHashtable, keyHasher) == 8);
static_assert(offsetof(UHashtable, keyComparator) == 16 && offsetof(UHashtable, valueComparator) == 24);
static_assert(offsetof(UHashtable, keyDeleter) == 32 && offsetof(UHashtable, valueDeleter) == 40);
static_assert(offsetof(UHashtable, count) == 48 && offsetof(UHashtable, length) == 52);
static_assert(offsetof(UHashtable, highWaterMark) == 56 && offsetof(UHashtable, lowWaterMark) == 60);
static_assert(offsetof(UHashtable, highWaterRatio) == 64 && offsetof(UHashtable, lowWaterRatio) == 68);
static_assert(offsetof(UHashtable, primeIndex) == 72 && offsetof(UHashtable, allocated) == 73);

}

extern "C" {
Icu::UHashtable* APS5_VABI uhash_open_nid_postfix(Icu::HashFunction, Icu::HashComparator, Icu::HashComparator, std::int32_t*);
Icu::UHashtable* APS5_VABI uhash_openSize_nid_postfix(Icu::HashFunction, Icu::HashComparator, Icu::HashComparator, std::int32_t, std::int32_t*);
Icu::UHashtable* APS5_VABI uhash_init_nid_postfix(Icu::UHashtable*, Icu::HashFunction, Icu::HashComparator, Icu::HashComparator, std::int32_t*);
Icu::UHashtable* APS5_VABI uhash_initSize_nid_postfix(Icu::UHashtable*, Icu::HashFunction, Icu::HashComparator, Icu::HashComparator, std::int32_t, std::int32_t*);
void APS5_VABI uhash_close_nid_postfix(Icu::UHashtable*);
Icu::HashFunction APS5_VABI uhash_setKeyHasher_nid_postfix(Icu::UHashtable*, Icu::HashFunction);
Icu::HashComparator APS5_VABI uhash_setKeyComparator_nid_postfix(Icu::UHashtable*, Icu::HashComparator);
Icu::HashComparator APS5_VABI uhash_setValueComparator_nid_postfix(Icu::UHashtable*, Icu::HashComparator);
Icu::ObjectDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(Icu::UHashtable*, Icu::ObjectDeleter);
Icu::ObjectDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(Icu::UHashtable*, Icu::ObjectDeleter);
void APS5_VABI uhash_setResizePolicy_nid_postfix(Icu::UHashtable*, std::int32_t);
std::int32_t APS5_VABI uhash_count_nid_postfix(const Icu::UHashtable*);
void* APS5_VABI uhash_get_nid_postfix(const Icu::UHashtable*, const void*);
void* APS5_VABI uhash_iget_nid_postfix(const Icu::UHashtable*, std::int32_t);
std::int32_t APS5_VABI uhash_geti_nid_postfix(const Icu::UHashtable*, const void*);
std::int32_t APS5_VABI uhash_igeti_nid_postfix(const Icu::UHashtable*, std::int32_t);
void* APS5_VABI uhash_put_nid_postfix(Icu::UHashtable*, void*, void*, std::int32_t*);
void* APS5_VABI uhash_iput_nid_postfix(Icu::UHashtable*, std::int32_t, void*, std::int32_t*);
std::int32_t APS5_VABI uhash_puti_nid_postfix(Icu::UHashtable*, void*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI uhash_iputi_nid_postfix(Icu::UHashtable*, std::int32_t, std::int32_t, std::int32_t*);
void* APS5_VABI uhash_remove_nid_postfix(Icu::UHashtable*, const void*);
void* APS5_VABI uhash_iremove_nid_postfix(Icu::UHashtable*, std::int32_t);
std::int32_t APS5_VABI uhash_removei_nid_postfix(Icu::UHashtable*, const void*);
std::int32_t APS5_VABI uhash_iremovei_nid_postfix(Icu::UHashtable*, std::int32_t);
void APS5_VABI uhash_removeAll_nid_postfix(Icu::UHashtable*);
const Icu::UHashElement* APS5_VABI uhash_find_nid_postfix(const Icu::UHashtable*, const void*);
const Icu::UHashElement* APS5_VABI uhash_nextElement_nid_postfix(const Icu::UHashtable*, std::int32_t*);
void* APS5_VABI uhash_removeElement_nid_postfix(Icu::UHashtable*, const Icu::UHashElement*);
std::int8_t APS5_VABI uhash_equals_nid_postfix(const Icu::UHashtable*, const Icu::UHashtable*);
std::int32_t APS5_VABI uhash_hashLong_nid_postfix(Icu::UHashTok);
std::int8_t APS5_VABI uhash_compareLong_nid_postfix(Icu::UHashTok, Icu::UHashTok);
std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(Icu::UHashTok);
std::int32_t APS5_VABI uhash_hashChars_nid_postfix(Icu::UHashTok);
std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(Icu::UHashTok);
std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(Icu::UHashTok, Icu::UHashTok);
std::int8_t APS5_VABI uhash_compareChars_nid_postfix(Icu::UHashTok, Icu::UHashTok);
std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(Icu::UHashTok, Icu::UHashTok);
std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t*, std::int32_t);
std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char*, std::int32_t);
std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char*, std::int32_t);
}
