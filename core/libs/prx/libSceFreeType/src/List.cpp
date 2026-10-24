#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <stdexcept>

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

struct Iteration {
    GuestListIterator iterator;
    void* user;
};

int Iterate(FT_ListNode node, void* user) {
    auto& call = *static_cast<Iteration*>(user);
    return call.iterator(node, call.user);
}

struct Finalization {
    GuestMemoryRec* memory;
    GuestListDestructor destroy;
    void* user;
};

void Destroy(FT_Memory memory, void* data, void*) {
    auto& call = *static_cast<Finalization*>(memory->user);
    call.destroy(call.memory, data, call.user);
}

void FreeNode(FT_Memory memory, void* block) {
    auto& call = *static_cast<Finalization*>(memory->user);
    call.memory->free(call.memory, block);
}

}

extern "C" {

GuestListNodeRec* APS5_VABI FT_List_Find_nid_postfix(GuestListRec* list, void* data) {
    return FT_List_Find(list, data);
}

void APS5_VABI FT_List_Add_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    const ListWrite write(list, {node, list->tail});
    FT_List_Add(list, node);
}

void APS5_VABI FT_List_Insert_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    const ListWrite write(list, {node, list->head});
    FT_List_Insert(list, node);
}

void APS5_VABI FT_List_Remove_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node) return;
    const ListWrite write(list, {node->prev, node->next});
    FT_List_Remove(list, node);
}

void APS5_VABI FT_List_Up_nid_postfix(GuestListRec* list, GuestListNodeRec* node) {
    if (!list || !node || !node->prev) return;
    const ListWrite write(list, {node, node->prev, node->next, list->head});
    FT_List_Up(list, node);
}

int APS5_VABI FT_List_Iterate_nid_postfix(GuestListRec* list, GuestListIterator iterator, void* user) {
    Iteration call{iterator, user};
    return FT_List_Iterate(list, iterator ? Iterate : nullptr, &call);
}

void APS5_VABI FT_List_Finalize_nid_postfix(GuestListRec* list, GuestListDestructor destroy, GuestMemoryRec* memory, void* user) {
    if (!list || !memory) return;
    const ListWrite write(list);
    Finalization call{memory, destroy, user};
    FT_MemoryRec_ native{};
    native.user = &call;
    native.free = FreeNode;
    FT_List_Finalize(list, destroy ? Destroy : nullptr, &native, user);
}

}
