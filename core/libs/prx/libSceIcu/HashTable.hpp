#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

union UHashTok {
    void* pointer;
    std::int32_t integer;
};

struct UHashElement {
    std::int32_t hashcode;
    UHashTok value;
    UHashTok key;
};

using UHashFunction = std::int32_t (APS5_VABI*)(UHashTok);
using UKeyComparator = std::int8_t (APS5_VABI*)(UHashTok, UHashTok);
using UValueComparator = UKeyComparator;
using GuestUObjectDeleter = void (APS5_VABI*)(void*);

struct UHashtable {
    UHashElement* elements;
    UHashFunction keyHasher;
    UKeyComparator keyComparator;
    UValueComparator valueComparator;
    GuestUObjectDeleter keyDeleter;
    GuestUObjectDeleter valueDeleter;
    std::int32_t count;
    std::int32_t length;
    std::int32_t highWaterMark;
    std::int32_t lowWaterMark;
    float highWaterRatio;
    float lowWaterRatio;
    std::int8_t primeIndex;
    std::int8_t allocated;
};

struct UHashtableObject {
    UHashtable* hash;
    UHashtable hashObj;
};

static_assert(sizeof(UHashTok) == 8);
static_assert(sizeof(UHashElement) == 24);
static_assert(sizeof(UHashtable) == 80);
static_assert(sizeof(UHashtableObject) == 88);

extern "C" {
UHashtable* APS5_VABI uhash_open_nid_postfix(UHashFunction, UKeyComparator, UValueComparator, std::int32_t*);
UHashtable* APS5_VABI uhash_openSize_nid_postfix(UHashFunction, UKeyComparator, UValueComparator, std::int32_t, std::int32_t*);
UHashtable* APS5_VABI uhash_init_nid_postfix(UHashtable*, UHashFunction, UKeyComparator, UValueComparator, std::int32_t*);
UHashtable* APS5_VABI uhash_initSize_nid_postfix(UHashtable*, UHashFunction, UKeyComparator, UValueComparator, std::int32_t, std::int32_t*);
void APS5_VABI uhash_close_nid_postfix(UHashtable*);
UHashFunction APS5_VABI uhash_setKeyHasher_nid_postfix(UHashtable*, UHashFunction);
UKeyComparator APS5_VABI uhash_setKeyComparator_nid_postfix(UHashtable*, UKeyComparator);
UValueComparator APS5_VABI uhash_setValueComparator_nid_postfix(UHashtable*, UValueComparator);
GuestUObjectDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(UHashtable*, GuestUObjectDeleter);
GuestUObjectDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(UHashtable*, GuestUObjectDeleter);
void APS5_VABI uhash_setResizePolicy_nid_postfix(UHashtable*, std::int32_t);
std::int32_t APS5_VABI uhash_count_nid_postfix(const UHashtable*);
void* APS5_VABI uhash_get_nid_postfix(const UHashtable*, const void*);
void* APS5_VABI uhash_iget_nid_postfix(const UHashtable*, std::int32_t);
std::int32_t APS5_VABI uhash_geti_nid_postfix(const UHashtable*, const void*);
std::int32_t APS5_VABI uhash_igeti_nid_postfix(const UHashtable*, std::int32_t);
void* APS5_VABI uhash_put_nid_postfix(UHashtable*, void*, void*, std::int32_t*);
void* APS5_VABI uhash_iput_nid_postfix(UHashtable*, std::int32_t, void*, std::int32_t*);
std::int32_t APS5_VABI uhash_puti_nid_postfix(UHashtable*, void*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI uhash_iputi_nid_postfix(UHashtable*, std::int32_t, std::int32_t, std::int32_t*);
void* APS5_VABI uhash_remove_nid_postfix(UHashtable*, const void*);
void* APS5_VABI uhash_iremove_nid_postfix(UHashtable*, std::int32_t);
std::int32_t APS5_VABI uhash_removei_nid_postfix(UHashtable*, const void*);
std::int32_t APS5_VABI uhash_iremovei_nid_postfix(UHashtable*, std::int32_t);
void APS5_VABI uhash_removeAll_nid_postfix(UHashtable*);
const UHashElement* APS5_VABI uhash_find_nid_postfix(const UHashtable*, const void*);
const UHashElement* APS5_VABI uhash_nextElement_nid_postfix(const UHashtable*, std::int32_t*);
void* APS5_VABI uhash_removeElement_nid_postfix(UHashtable*, const UHashElement*);
std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(UHashTok);
std::int32_t APS5_VABI uhash_hashChars_nid_postfix(UHashTok);
std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(UHashTok);
std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(UHashTok, UHashTok);
std::int8_t APS5_VABI uhash_compareChars_nid_postfix(UHashTok, UHashTok);
std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(UHashTok, UHashTok);
std::int32_t APS5_VABI uhash_hashLong_nid_postfix(UHashTok);
std::int8_t APS5_VABI uhash_compareLong_nid_postfix(UHashTok, UHashTok);
void APS5_VABI uhash_deleteHashtable_nid_postfix(void*);
std::int8_t APS5_VABI uhash_equals_nid_postfix(const UHashtable*, const UHashtable*);
}
