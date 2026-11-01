// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "StringsEnumeration.hpp"

namespace Icu {

struct UListNode {
    void* data;
    UListNode* next;
    UListNode* previous;
    std::int8_t forceDelete;
};

struct UList {
    UListNode* curr;
    UListNode* head;
    UListNode* tail;
    std::int32_t size;
};

static_assert(sizeof(UListNode) == 32 && alignof(UListNode) == 8);
static_assert(offsetof(UListNode, data) == 0 && offsetof(UListNode, next) == 8);
static_assert(offsetof(UListNode, previous) == 16 && offsetof(UListNode, forceDelete) == 24);
static_assert(sizeof(UList) == 32 && alignof(UList) == 8);
static_assert(offsetof(UList, curr) == 0 && offsetof(UList, head) == 8);
static_assert(offsetof(UList, tail) == 16 && offsetof(UList, size) == 24);

}

extern "C" {
Icu::UList* APS5_VABI ulist_createEmptyList_nid_postfix(std::int32_t*);
void APS5_VABI ulist_addItemEndList_nid_postfix(Icu::UList*, const void*, std::int8_t, std::int32_t*);
void APS5_VABI ulist_addItemBeginList_nid_postfix(Icu::UList*, const void*, std::int8_t, std::int32_t*);
std::int8_t APS5_VABI ulist_containsString_nid_postfix(const Icu::UList*, const char*, std::int32_t);
std::int8_t APS5_VABI ulist_removeString_nid_postfix(Icu::UList*, const char*);
void* APS5_VABI ulist_getNext_nid_postfix(Icu::UList*);
std::int32_t APS5_VABI ulist_getListSize_nid_postfix(const Icu::UList*);
void APS5_VABI ulist_resetList_nid_postfix(Icu::UList*);
void APS5_VABI ulist_deleteList_nid_postfix(Icu::UList*);
void APS5_VABI ulist_close_keyword_values_iterator_nid_postfix(Icu::UEnumeration*);
std::int32_t APS5_VABI ulist_count_keyword_values_nid_postfix(Icu::UEnumeration*, std::int32_t*);
const char* APS5_VABI ulist_next_keyword_value_nid_postfix(Icu::UEnumeration*, std::int32_t*, std::int32_t*);
void APS5_VABI ulist_reset_keyword_values_iterator_nid_postfix(Icu::UEnumeration*, std::int32_t*);
Icu::UList* APS5_VABI ulist_getListFromEnum_nid_postfix(Icu::UEnumeration*);
}
