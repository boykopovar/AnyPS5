#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <stdexcept>
#include <ft2build.h>
#include FT_LIST_H

using namespace FreeType;

namespace {

class ListWrite {
public:
    ListWrite(GuestListRec* list, std::array<GuestListNodeRec*, 4> nodes = {})
        : writes{GuestArena::HostWrite(list, sizeof(*list)),
                 GuestArena::HostWrite(nodes[0], nodes[0] ? sizeof(*nodes[0]) : 0),
                 GuestArena::HostWrite(nodes[1], nodes[1] ? sizeof(*nodes[1]) : 0),
                 GuestArena::HostWrite(nodes[2], nodes[2] ? sizeof(*nodes[2]) : 0),
                 GuestArena::HostWrite(nodes[3], nodes[3] ? sizeof(*nodes[3]) : 0)} {
        for (const auto& write : writes) {
            if (!write.Open()) throw std::runtime_error("FT_List: a destination is not writable");
        }
    }

private:
    std::array<GuestArena::HostWrite, 5> writes;
};

}

extern "C" {

GuestListNodeRec* APS5_VABI FT_List_Find_nid_postfix(GuestListRec* list, void* data) {
    if (!list) return nullptr;
    for (auto* node = list->head; node; node = node->next) {
        if (node->data == data) return node;
    }
    return nullptr;
}

void APS5_VABI FT_List_Add_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    const ListWrite write(list, {node, list->tail});
    auto* tail = list->tail;
    node->next = nullptr;
    node->prev = tail;
    if (tail) tail->next = node;
    else list->head = node;
    list->tail = node;
}

void APS5_VABI FT_List_Insert_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    const ListWrite write(list, {node, list->head});
    auto* head = list->head;
    node->next = head;
    node->prev = nullptr;
    if (head) head->prev = node;
    else list->tail = node;
    list->head = node;
}

void APS5_VABI FT_List_Remove_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    const ListWrite write(list, {node->prev, node->next});
    auto* previous = node->prev;
    auto* next = node->next;
    if (previous) previous->next = next;
    else list->head = next;
    if (next) next->prev = previous;
    else list->tail = previous;
}

void APS5_VABI FT_List_Up_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node || !node->prev) return;
    const ListWrite write(list, {node, node->prev, node->next, list->head});
    auto* previous = node->prev;
    auto* next = node->next;
    previous->next = next;
    if (next) next->prev = previous;
    else list->tail = previous;
    node->prev = nullptr;
    node->next = list->head;
    list->head->prev = node;
    list->head = node;
}

int APS5_VABI FT_List_Iterate_nid_postfix(GuestListRec* list, GuestListIterator iterator, void* user) {
    if (!list || !iterator) return FT_Err_Invalid_Argument;
    auto* node = list->head;
    while (node) {
        auto* next = node->next;
        const int error = iterator(node, user);
        if (error) return error;
        node = next;
    }
    return 0;
}

void APS5_VABI FT_List_Finalize_nid_postfix(GuestListRec* list, GuestListDestructor destroy, GuestMemoryRec* memory, void* user) {
    if (!list || !memory) return;
    const ListWrite write(list);
    auto* node = list->head;
    while (node) {
        auto* next = node->next;
        if (destroy) destroy(memory, node->data, user);
        memory->free(memory, node);
        node = next;
    }
    list->head = nullptr;
    list->tail = nullptr;
}

}
