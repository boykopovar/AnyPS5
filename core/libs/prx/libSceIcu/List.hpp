#pragma once
#include "Enumeration.hpp"
#include <cstdint>

struct UListNode {
    void* data;
    UListNode* next;
    UListNode* previous;
    std::int8_t forceDelete;
};

struct UList {
    UListNode* current;
    UListNode* head;
    UListNode* tail;
    std::int32_t size;
};

static_assert(sizeof(UListNode) == 32);
static_assert(sizeof(UList) == 32);

extern "C" {
UList* APS5_VABI ulist_createEmptyList_nid_postfix(std::int32_t*);
void APS5_VABI ulist_addItemEndList_nid_postfix(UList*, const void*, std::int8_t, std::int32_t*);
void APS5_VABI ulist_addItemBeginList_nid_postfix(UList*, const void*, std::int8_t, std::int32_t*);
std::int8_t APS5_VABI ulist_containsString_nid_postfix(const UList*, const char*, std::int32_t);
std::int8_t APS5_VABI ulist_removeString_nid_postfix(UList*, const char*);
void* APS5_VABI ulist_getNext_nid_postfix(UList*);
std::int32_t APS5_VABI ulist_getListSize_nid_postfix(const UList*);
void APS5_VABI ulist_resetList_nid_postfix(UList*);
void APS5_VABI ulist_deleteList_nid_postfix(UList*);
void APS5_VABI ulist_close_keyword_values_iterator_nid_postfix(UEnumeration*);
std::int32_t APS5_VABI ulist_count_keyword_values_nid_postfix(UEnumeration*, std::int32_t*);
const char* APS5_VABI ulist_next_keyword_value_nid_postfix(UEnumeration*, std::int32_t*, std::int32_t*);
void APS5_VABI ulist_reset_keyword_values_iterator_nid_postfix(UEnumeration*, std::int32_t*);
UList* APS5_VABI ulist_getListFromEnum_nid_postfix(UEnumeration*);
}
