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
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

constexpr std::int32_t MemoryAllocationError = 7;
constexpr int StringTooLong = 63;

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    std::int32_t value = 0;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!readable) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t value, const char* caller) {
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!written) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

bool ReadBounded(const char* text, std::size_t maximum, std::string& copy, const char* caller) {
    const int savedErrno = errno;
    const int result = CopyGuestString_nid_no_patch(text, maximum, copy);
    errno = savedErrno;
    if (result == StringTooLong) return false;
    if (result != 0) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
    return true;
}

std::string ReadString(const char* text, const char* caller) {
    std::string copy;
    std::size_t maximum = 256;
    while (!ReadBounded(text, maximum, copy, caller)) maximum *= 2;
    return copy;
}

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

UListNode* NewNode(UList* list, const void* data, std::int8_t forceDelete, std::int32_t* status, const char* caller) {
    if (ReadStatus(status, caller) > 0 || !list || !data) {
        if (forceDelete) IcuReleaseMemory_nid_no_patch(const_cast<void*>(data));
        return nullptr;
    }
    auto* node = static_cast<UListNode*>(uprv_malloc_nid_postfix(sizeof(UListNode)));
    if (!node) {
        if (forceDelete) IcuReleaseMemory_nid_no_patch(const_cast<void*>(data));
        WriteStatus(status, MemoryAllocationError, caller);
        return nullptr;
    }
    node->data = const_cast<void*>(data);
    node->forceDelete = forceDelete;
    return node;
}

}

extern "C" {

UList* APS5_VABI ulist_createEmptyList_nid_postfix(std::int32_t* status) {
    if (ReadStatus(status, __func__) > 0) return nullptr;
    auto* list = static_cast<UList*>(uprv_malloc_nid_postfix(sizeof(UList)));
    if (!list) {
        WriteStatus(status, MemoryAllocationError, __func__);
        return nullptr;
    }
    list->current = nullptr;
    list->head = nullptr;
    list->tail = nullptr;
    list->size = 0;
    return list;
}

void APS5_VABI ulist_addItemEndList_nid_postfix(UList* list, const void* data, std::int8_t forceDelete, std::int32_t* status) {
    UListNode* node = NewNode(list, data, forceDelete, status, __func__);
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
    UListNode* node = NewNode(list, data, forceDelete, status, __func__);
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
    if (!list || length < 0) return 0;
    const auto count = static_cast<std::size_t>(length);
    std::string text;
    bool copied = false;
    for (const UListNode* node = list->head; node; node = node->next) {
        std::string item;
        if (!ReadBounded(static_cast<const char*>(node->data), count + 1, item, __func__) || item.size() != count) continue;
        if (count == 0) return 1;
        if (!data) APS5_INVALID_ARG_EX;
        if (!copied) {
            text.resize(count);
            const int savedErrno = errno;
            const bool readable = GuestCopyIn_nid_no_patch(text.data(), data, count);
            errno = savedErrno;
            if (!readable) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
            copied = true;
        }
        if (item == text) return 1;
    }
    return 0;
}

std::int8_t APS5_VABI ulist_removeString_nid_postfix(UList* list, const char* data) {
    if (!list || !list->head) return 0;
    if (!data) APS5_INVALID_ARG_EX;
    const std::string text = ReadString(data, __func__);
    for (UListNode* node = list->head; node; node = node->next) {
        std::string item;
        if (!ReadBounded(static_cast<const char*>(node->data), text.size() + 1, item, __func__) || item != text) continue;
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
    if (ReadStatus(status, __func__) > 0) return -1;
    return ulist_getListSize_nid_postfix(ListOf(enumeration));
}

const char* APS5_VABI ulist_next_keyword_value_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (ReadStatus(status, __func__) > 0) return nullptr;
    const auto* value = static_cast<const char*>(ulist_getNext_nid_postfix(ListOf(enumeration)));
    if (value && resultLength) {
        const auto length = static_cast<std::int32_t>(ReadString(value, __func__).size());
        const int savedErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(resultLength, &length, sizeof(length));
        errno = savedErrno;
        if (!written) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return value;
}

void APS5_VABI ulist_reset_keyword_values_iterator_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (ReadStatus(status, __func__) > 0) return;
    ulist_resetList_nid_postfix(ListOf(enumeration));
}

UList* APS5_VABI ulist_getListFromEnum_nid_postfix(UEnumeration* enumeration) {
    return ListOf(enumeration);
}

}
