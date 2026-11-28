#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <initializer_list>
#include <ft2build.h>
#include FT_LIST_H

using namespace FreeType;

namespace {

bool CopyInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

template <typename Value>
Value Load(const Value* source, const char* func) {
    Value value{};
    if (!CopyInPreservingErrno(&value, source, sizeof(value))) NotImplementedAt_nid_no_patch(func, __FILE__, __LINE__);
    return value;
}

template <typename Value>
void Store(Value* destination, const Value& value, const char* func) {
    if (!GuestCopyOut_nid_no_patch(destination, &value, sizeof(value))) NotImplementedAt_nid_no_patch(func, __FILE__, __LINE__);
}

void SetPrev(GuestListNodeRec* node, GuestListNodeRec* value, const char* func) {
    Store(&node->prev, value, func);
}

void SetNext(GuestListNodeRec* node, GuestListNodeRec* value, const char* func) {
    Store(&node->next, value, func);
}

void SetHead(GuestListRec* list, GuestListNodeRec* value, const char* func) {
    Store(&list->head, value, func);
}

void SetTail(GuestListRec* list, GuestListNodeRec* value, const char* func) {
    Store(&list->tail, value, func);
}

void RequireWritable(const char* func, GuestListRec* list, std::initializer_list<GuestListNodeRec*> nodes = {}) {
    bool writable = GuestWritable_nid_no_patch(list, sizeof(*list));
    for (auto* node : nodes) writable = writable && (!node || GuestWritable_nid_no_patch(node, sizeof(*node)));
    if (!writable) NotImplementedAt_nid_no_patch(func, __FILE__, __LINE__);
}

}

extern "C" {

GuestListNodeRec* APS5_VABI FT_List_Find_nid_postfix(GuestListRec* list, void* data) {
    if (!list) return nullptr;
    for (auto* node = Load(list, __func__).head; node;) {
        const GuestListNodeRec record = Load(node, __func__);
        if (record.data == data) return node;
        node = record.next;
    }
    return nullptr;
}

void APS5_VABI FT_List_Add_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    auto* tail = Load(list, __func__).tail;
    RequireWritable(__func__, list, {node, tail});
    SetNext(node, nullptr, __func__);
    SetPrev(node, tail, __func__);
    if (tail) SetNext(tail, node, __func__);
    else SetHead(list, node, __func__);
    SetTail(list, node, __func__);
}

void APS5_VABI FT_List_Insert_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    auto* head = Load(list, __func__).head;
    RequireWritable(__func__, list, {node, head});
    SetNext(node, head, __func__);
    SetPrev(node, nullptr, __func__);
    if (head) SetPrev(head, node, __func__);
    else SetTail(list, node, __func__);
    SetHead(list, node, __func__);
}

void APS5_VABI FT_List_Remove_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    const GuestListNodeRec record = Load(node, __func__);
    RequireWritable(__func__, list, {record.prev, record.next});
    if (record.prev) SetNext(record.prev, record.next, __func__);
    else SetHead(list, record.next, __func__);
    if (record.next) SetPrev(record.next, record.prev, __func__);
    else SetTail(list, record.prev, __func__);
}

void APS5_VABI FT_List_Up_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    const GuestListNodeRec record = Load(node, __func__);
    if (!record.prev) return;
    auto* head = Load(list, __func__).head;
    RequireWritable(__func__, list, {node, record.prev, record.next, head});
    SetNext(record.prev, record.next, __func__);
    if (record.next) SetPrev(record.next, record.prev, __func__);
    else SetTail(list, record.prev, __func__);
    SetPrev(node, nullptr, __func__);
    SetNext(node, head, __func__);
    SetPrev(head, node, __func__);
    SetHead(list, node, __func__);
}

int APS5_VABI FT_List_Iterate_nid_postfix(GuestListRec* list, GuestListIterator iterator, void* user) {
    if (!list || !iterator) return FT_Err_Invalid_Argument;
    auto* node = Load(list, __func__).head;
    while (node) {
        auto* next = Load(node, __func__).next;
        const int error = iterator(node, user);
        if (error) return error;
        node = next;
    }
    return 0;
}

void APS5_VABI FT_List_Finalize_nid_postfix(GuestListRec* list, GuestListDestructor destroy, GuestMemoryRec* memory, void* user) {
    if (!list || !memory) return;
    RequireWritable(__func__, list);
    auto* node = Load(list, __func__).head;
    while (node) {
        const GuestListNodeRec record = Load(node, __func__);
        if (destroy) destroy(memory, record.data, user);
        Load(memory, __func__).free(memory, node);
        node = record.next;
    }
    Store(list, GuestListRec{}, __func__);
}

}
