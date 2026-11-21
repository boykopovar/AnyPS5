#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

union UElement {
    void* pointer;
    std::int32_t integer;
};

using UHashTok = UElement;

struct UHashElement {
    std::int32_t hashcode;
    UHashTok value;
    UHashTok key;
};

using UHashFunction = std::int32_t (APS5_VABI*)(const UHashTok);
using UElementsAreEqual = std::int8_t (APS5_VABI*)(const UHashTok, const UHashTok);
using UKeyComparator = UElementsAreEqual;
using UValueComparator = UElementsAreEqual;
using UObjectDeleter = void (APS5_VABI*)(void*);

struct UHashtable {
    UHashElement* elements;
    UHashFunction keyHasher;
    UKeyComparator keyComparator;
    UValueComparator valueComparator;
    UObjectDeleter keyDeleter;
    UObjectDeleter valueDeleter;
    std::int32_t count;
    std::int32_t length;
    std::int32_t highWaterMark;
    std::int32_t lowWaterMark;
    float highWaterRatio;
    float lowWaterRatio;
    std::int8_t primeIndex;
    std::int8_t allocated;
};

UHashtable* APS5_VABI uhash_open_nid_postfix(UHashFunction keyHash, UKeyComparator keyComparator,
                                             UValueComparator valueComparator, std::int32_t* status);
UHashtable* APS5_VABI uhash_openSize_nid_postfix(UHashFunction keyHash, UKeyComparator keyComparator,
                                                 UValueComparator valueComparator, std::int32_t size,
                                                 std::int32_t* status);
UHashtable* APS5_VABI uhash_init_nid_postfix(UHashtable* result, UHashFunction keyHash, UKeyComparator keyComparator,
                                             UValueComparator valueComparator, std::int32_t* status);
UHashtable* APS5_VABI uhash_initSize_nid_postfix(UHashtable* result, UHashFunction keyHash,
                                                 UKeyComparator keyComparator, UValueComparator valueComparator,
                                                 std::int32_t size, std::int32_t* status);
void APS5_VABI uhash_close_nid_postfix(UHashtable* hash);
void APS5_VABI uhash_deleteHashtable_nid_postfix(void* object);
UHashFunction APS5_VABI uhash_setKeyHasher_nid_postfix(UHashtable* hash, UHashFunction function);
UKeyComparator APS5_VABI uhash_setKeyComparator_nid_postfix(UHashtable* hash, UKeyComparator function);
UValueComparator APS5_VABI uhash_setValueComparator_nid_postfix(UHashtable* hash, UValueComparator function);
UObjectDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(UHashtable* hash, UObjectDeleter function);
UObjectDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(UHashtable* hash, UObjectDeleter function);
void APS5_VABI uhash_setResizePolicy_nid_postfix(UHashtable* hash, std::int32_t policy);
std::int32_t APS5_VABI uhash_count_nid_postfix(const UHashtable* hash);
void* APS5_VABI uhash_get_nid_postfix(const UHashtable* hash, const void* key);
void* APS5_VABI uhash_iget_nid_postfix(const UHashtable* hash, std::int32_t key);
std::int32_t APS5_VABI uhash_geti_nid_postfix(const UHashtable* hash, const void* key);
std::int32_t APS5_VABI uhash_igeti_nid_postfix(const UHashtable* hash, std::int32_t key);
void* APS5_VABI uhash_put_nid_postfix(UHashtable* hash, void* key, void* value, std::int32_t* status);
void* APS5_VABI uhash_iput_nid_postfix(UHashtable* hash, std::int32_t key, void* value, std::int32_t* status);
std::int32_t APS5_VABI uhash_puti_nid_postfix(UHashtable* hash, void* key, std::int32_t value, std::int32_t* status);
std::int32_t APS5_VABI uhash_iputi_nid_postfix(UHashtable* hash, std::int32_t key, std::int32_t value,
                                               std::int32_t* status);
void* APS5_VABI uhash_remove_nid_postfix(UHashtable* hash, const void* key);
void* APS5_VABI uhash_iremove_nid_postfix(UHashtable* hash, std::int32_t key);
std::int32_t APS5_VABI uhash_removei_nid_postfix(UHashtable* hash, const void* key);
std::int32_t APS5_VABI uhash_iremovei_nid_postfix(UHashtable* hash, std::int32_t key);
void APS5_VABI uhash_removeAll_nid_postfix(UHashtable* hash);
const UHashElement* APS5_VABI uhash_find_nid_postfix(const UHashtable* hash, const void* key);
const UHashElement* APS5_VABI uhash_nextElement_nid_postfix(const UHashtable* hash, std::int32_t* position);
void* APS5_VABI uhash_removeElement_nid_postfix(UHashtable* hash, const UHashElement* element);
std::int8_t APS5_VABI uhash_equals_nid_postfix(const UHashtable* first, const UHashtable* second);
std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(const UHashTok key);
std::int32_t APS5_VABI uhash_hashChars_nid_postfix(const UHashTok key);
std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(const UHashTok key);
std::int32_t APS5_VABI uhash_hashLong_nid_postfix(const UHashTok key);
std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(const UHashTok first, const UHashTok second);
std::int8_t APS5_VABI uhash_compareChars_nid_postfix(const UHashTok first, const UHashTok second);
std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(const UHashTok first, const UHashTok second);
std::int8_t APS5_VABI uhash_compareLong_nid_postfix(const UHashTok first, const UHashTok second);

}
