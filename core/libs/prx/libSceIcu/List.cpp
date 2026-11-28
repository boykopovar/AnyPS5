// Derived from icu4c/source/common/ulist.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ulist.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2009-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t MemoryAllocationError = 7;

UList* ListOf(UEnumeration* enumeration) {
    if (!enumeration) APS5_INVALID_ARG_EX;
    return static_cast<UList*>(enumeration->context);
}

void Remove(UList* list, UListNode* node) {
    if (node->previous) node->previous->next = node->next;
    else list->head = node->next;
    if (node->next) node->next->previous = node->previous;
    else list->tail = node->previous;
    if (node == list->current) list->current = node->next;
    --list->size;
    if (node->forceDelete) IcuReleaseMemory_nid_no_patch(node->data);
    IcuReleaseMemory_nid_no_patch(node);
}

UListNode* NewNode(UList* list, const void* data, std::int8_t forceDelete, std::int32_t* status) {
    if (Icu::Failure(status) || !list || !data) {
        if (forceDelete) IcuReleaseMemory_nid_no_patch(const_cast<void*>(data));
        return nullptr;
    }
    auto* node = static_cast<UListNode*>(uprv_malloc_nid_postfix(sizeof(UListNode)));
    if (!node) {
        if (forceDelete) IcuReleaseMemory_nid_no_patch(const_cast<void*>(data));
        *status = MemoryAllocationError;
        return nullptr;
    }
    node->data = const_cast<void*>(data);
    node->forceDelete = forceDelete;
    return node;
}

}

extern "C" {

UList* APS5_VABI ulist_createEmptyList_nid_postfix(std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    auto* list = static_cast<UList*>(uprv_malloc_nid_postfix(sizeof(UList)));
    if (!list) {
        *status = MemoryAllocationError;
        return nullptr;
    }
    list->current = nullptr;
    list->head = nullptr;
    list->tail = nullptr;
    list->size = 0;
    return list;
}

void APS5_VABI ulist_addItemEndList_nid_postfix(UList* list, const void* data, std::int8_t forceDelete, std::int32_t* status) {
    UListNode* node = NewNode(list, data, forceDelete, status);
    if (!node) return;
    node->next = nullptr;
    if (list->size == 0) {
        node->previous = nullptr;
        list->head = node;
    } else {
        node->previous = list->tail;
        list->tail->next = node;
    }
    list->tail = node;
    ++list->size;
}

void APS5_VABI ulist_addItemBeginList_nid_postfix(UList* list, const void* data, std::int8_t forceDelete, std::int32_t* status) {
    UListNode* node = NewNode(list, data, forceDelete, status);
    if (!node) return;
    node->previous = nullptr;
    if (list->size == 0) {
        node->next = nullptr;
        list->tail = node;
    } else {
        node->next = list->head;
        list->head->previous = node;
    }
    list->head = node;
    ++list->size;
}

std::int8_t APS5_VABI ulist_containsString_nid_postfix(const UList* list, const char* data, std::int32_t length) {
    if (!list) return 0;
    for (const UListNode* node = list->head; node; node = node->next) {
        if (length < 0 || static_cast<std::size_t>(length) != std::strlen(static_cast<const char*>(node->data))) continue;
        if (length == 0) return 1;
        if (!data) APS5_INVALID_ARG_EX;
        if (std::memcmp(data, node->data, static_cast<std::size_t>(length)) == 0) return 1;
    }
    return 0;
}

std::int8_t APS5_VABI ulist_removeString_nid_postfix(UList* list, const char* data) {
    if (!list) return 0;
    for (UListNode* node = list->head; node; node = node->next) {
        if (!data) APS5_INVALID_ARG_EX;
        if (std::strcmp(data, static_cast<const char*>(node->data)) != 0) continue;
        Remove(list, node);
        return 1;
    }
    return 0;
}

void* APS5_VABI ulist_getNext_nid_postfix(UList* list) {
    if (!list || !list->current) return nullptr;
    UListNode* node = list->current;
    list->current = node->next;
    return node->data;
}

std::int32_t APS5_VABI ulist_getListSize_nid_postfix(const UList* list) {
    return list ? list->size : -1;
}

void APS5_VABI ulist_resetList_nid_postfix(UList* list) {
    if (list) list->current = list->head;
}

void APS5_VABI ulist_deleteList_nid_postfix(UList* list) {
    if (!list) return;
    for (UListNode* node = list->head; node;) {
        UListNode* next = node->next;
        if (node->forceDelete) IcuReleaseMemory_nid_no_patch(node->data);
        IcuReleaseMemory_nid_no_patch(node);
        node = next;
    }
    IcuReleaseMemory_nid_no_patch(list);
}

void APS5_VABI ulist_close_keyword_values_iterator_nid_postfix(UEnumeration* enumeration) {
    if (!enumeration) return;
    ulist_deleteList_nid_postfix(static_cast<UList*>(enumeration->context));
    IcuReleaseMemory_nid_no_patch(enumeration);
}

std::int32_t APS5_VABI ulist_count_keyword_values_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    return ulist_getListSize_nid_postfix(ListOf(enumeration));
}

const char* APS5_VABI ulist_next_keyword_value_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    const auto* value = static_cast<const char*>(ulist_getNext_nid_postfix(ListOf(enumeration)));
    if (value && resultLength) *resultLength = static_cast<std::int32_t>(std::strlen(value));
    return value;
}

void APS5_VABI ulist_reset_keyword_values_iterator_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    ulist_resetList_nid_postfix(ListOf(enumeration));
}

UList* APS5_VABI ulist_getListFromEnum_nid_postfix(UEnumeration* enumeration) {
    return ListOf(enumeration);
}

}
