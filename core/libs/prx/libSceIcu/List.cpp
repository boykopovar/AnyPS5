// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <bit>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

template<typename TValue>
TValue Require(TValue value) {
    if (!value) throw std::invalid_argument("ICU list: null pointer");
    return value;
}

void* Allocate(std::size_t size) {
    return std::malloc(size);
}

void Add(Icu::UList* list, const void* data, std::int8_t forceDelete, std::int32_t* status, bool begin) {
    if (*Require(status) > 0 || !list || !data) {
        if (forceDelete) std::free(const_cast<void*>(data));
        return;
    }
    if (list->size == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("ICU list: size overflow");
    auto* node = static_cast<Icu::UListNode*>(Allocate(sizeof(Icu::UListNode)));
    if (!node) {
        if (forceDelete) std::free(const_cast<void*>(data));
        *status = 7;
        return;
    }
    *node = {const_cast<void*>(data), nullptr, nullptr, forceDelete};
    if (!list->size) {
        list->head = list->tail = node;
    } else if (begin) {
        node->next = Require(list->head);
        list->head->previous = node;
        list->head = node;
    } else {
        node->previous = Require(list->tail);
        list->tail->next = node;
        list->tail = node;
    }
    ++list->size;
}

std::int32_t Length(const char* text) {
    const auto length = std::strlen(Require(text));
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(length));
}

}

extern "C" {

Icu::UList* APS5_VABI ulist_createEmptyList_nid_postfix(std::int32_t* status) {
    if (*Require(status) > 0) return nullptr;
    auto* list = static_cast<Icu::UList*>(Allocate(sizeof(Icu::UList)));
    if (list) *list = {};
    else *status = 7;
    return list;
}

void APS5_VABI ulist_addItemEndList_nid_postfix(Icu::UList* list, const void* data, std::int8_t forceDelete, std::int32_t* status) {
    Add(list, data, forceDelete, status, false);
}

void APS5_VABI ulist_addItemBeginList_nid_postfix(Icu::UList* list, const void* data, std::int8_t forceDelete, std::int32_t* status) {
    Add(list, data, forceDelete, status, true);
}

std::int8_t APS5_VABI ulist_containsString_nid_postfix(const Icu::UList* list, const char* data, std::int32_t length) {
    if (!list) return 0;
    for (auto* node = list->head; node; node = node->next) {
        if (Length(static_cast<const char*>(node->data)) != length) continue;
        if (length < 0) throw std::invalid_argument("ICU list: negative comparison length");
        Require(data);
        if (!std::memcmp(data, node->data, length)) return 1;
    }
    return 0;
}

std::int8_t APS5_VABI ulist_removeString_nid_postfix(Icu::UList* list, const char* data) {
    if (!list) return 0;
    for (auto* node = list->head; node; node = node->next) {
        if (std::strcmp(Require(data), Require(static_cast<const char*>(node->data)))) continue;
        if (node->previous) node->previous->next = node->next;
        else list->head = node->next;
        if (node->next) node->next->previous = node->previous;
        else list->tail = node->previous;
        if (list->curr == node) list->curr = node->next;
        if (list->size == std::numeric_limits<std::int32_t>::min()) throw std::invalid_argument("ICU list: size underflow");
        --list->size;
        if (node->forceDelete) std::free(node->data);
        std::free(node);
        return 1;
    }
    return 0;
}

void* APS5_VABI ulist_getNext_nid_postfix(Icu::UList* list) {
    if (!list || !list->curr) return nullptr;
    auto* node = list->curr;
    list->curr = node->next;
    return node->data;
}

std::int32_t APS5_VABI ulist_getListSize_nid_postfix(const Icu::UList* list) {
    return list ? list->size : -1;
}

void APS5_VABI ulist_resetList_nid_postfix(Icu::UList* list) {
    if (list) list->curr = list->head;
}

void APS5_VABI ulist_deleteList_nid_postfix(Icu::UList* list) {
    if (!list) return;
    auto* node = list->head;
    while (node) {
        auto* next = node->next;
        if (node->forceDelete) std::free(node->data);
        std::free(node);
        node = next;
    }
    std::free(list);
}

void APS5_VABI ulist_close_keyword_values_iterator_nid_postfix(Icu::UEnumeration* enumeration) {
    if (!enumeration) return;
    ulist_deleteList_nid_postfix(static_cast<Icu::UList*>(enumeration->context));
    std::free(enumeration);
}

std::int32_t APS5_VABI ulist_count_keyword_values_nid_postfix(Icu::UEnumeration* enumeration, std::int32_t* status) {
    if (*Require(status) > 0) return -1;
    return ulist_getListSize_nid_postfix(static_cast<Icu::UList*>(Require(enumeration)->context));
}

const char* APS5_VABI ulist_next_keyword_value_nid_postfix(Icu::UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (*Require(status) > 0) return nullptr;
    const auto* text = static_cast<const char*>(ulist_getNext_nid_postfix(static_cast<Icu::UList*>(Require(enumeration)->context)));
    if (text && resultLength) *resultLength = Length(text);
    return text;
}

void APS5_VABI ulist_reset_keyword_values_iterator_nid_postfix(Icu::UEnumeration* enumeration, std::int32_t* status) {
    if (*Require(status) > 0) return;
    ulist_resetList_nid_postfix(static_cast<Icu::UList*>(Require(enumeration)->context));
}

Icu::UList* APS5_VABI ulist_getListFromEnum_nid_postfix(Icu::UEnumeration* enumeration) {
    return static_cast<Icu::UList*>(Require(enumeration)->context);
}

}
