#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestList.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>
#include <vector>
#include <type_traits>
#include <ft2build.h>
#include FT_LIST_H

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType list check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

constexpr std::size_t Count = 16;
template<typename TNode>
using Nodes = std::array<TNode, Count>;

template<typename TNode>
static int Index(TNode* node, const Nodes<TNode>& nodes) {
    if (!node) return -1;
    for (std::size_t i = 0; i < nodes.size(); ++i) if (node == &nodes[i]) return static_cast<int>(i);
    Require(false);
    return -2;
}

struct Lists {
    GuestListRec guest{};
    FT_ListRec native{};
    Nodes<GuestListNodeRec> actual{};
    Nodes<FT_ListNodeRec> expected{};
    std::array<int, 4> values{11, 22, 33, 44};

    Lists() {
        for (std::size_t i = 0; i < Count; ++i) actual[i].data = expected[i].data = i % 5 == 0 ? nullptr : &values[i % values.size()];
    }

    void Match() const {
        Require(Index(guest.head, actual) == Index(native.head, expected));
        Require(Index(guest.tail, actual) == Index(native.tail, expected));
        for (std::size_t i = 0; i < Count; ++i) {
            Require(Index(actual[i].prev, actual) == Index(expected[i].prev, expected));
            Require(Index(actual[i].next, actual) == Index(expected[i].next, expected));
            Require(actual[i].data == expected[i].data);
        }
    }

    void Fill() {
        for (std::size_t i = 0; i < Count; ++i) {
            FT_List_Add_nid_postfix(&guest, &actual[i]);

            FT_List_Add(&native, &expected[i]);
        }
        Match();
    }
};

static void Mutations() {
    Lists lists;
    std::array<bool, Count> active{};
    std::array<unsigned, 4> operations{};
    std::mt19937 random(0x7157);
    for (unsigned step = 0; step < 2048; ++step) {
        const auto i = random() % Count;
        const bool choice = (random() & 1u) != 0;
        if (active[i]) {
            if (choice) {
                FT_List_Up_nid_postfix(&lists.guest, &lists.actual[i]);

                FT_List_Up(&lists.native, &lists.expected[i]);
                ++operations[3];
            } else {
                FT_List_Remove_nid_postfix(&lists.guest, &lists.actual[i]);

                FT_List_Remove(&lists.native, &lists.expected[i]);
                active[i] = false;
                ++operations[2];
            }
        } else {
            if (choice) {
                FT_List_Insert_nid_postfix(&lists.guest, &lists.actual[i]);

                FT_List_Insert(&lists.native, &lists.expected[i]);
                ++operations[1];
            } else {
                FT_List_Add_nid_postfix(&lists.guest, &lists.actual[i]);

                FT_List_Add(&lists.native, &lists.expected[i]);
                ++operations[0];
            }
            active[i] = true;
        }
        lists.Match();
        for (auto& value : lists.values) {
            Require(Index(FT_List_Find_nid_postfix(&lists.guest, &value), lists.actual) == Index(FT_List_Find(&lists.native, &value), lists.expected));
        }
        Require(Index(FT_List_Find_nid_postfix(&lists.guest, nullptr), lists.actual) == Index(FT_List_Find(&lists.native, nullptr), lists.expected));
        int absent = 123;
        Require(FT_List_Find_nid_postfix(&lists.guest, &absent) == nullptr);
    }
    for (const auto count : operations) Require(count != 0);
    for (auto operation : {FT_List_Add_nid_postfix, FT_List_Insert_nid_postfix, FT_List_Remove_nid_postfix, FT_List_Up_nid_postfix}) {
        operation(nullptr, &lists.actual[0]);
        operation(&lists.guest, nullptr);
        operation(nullptr, nullptr);
        lists.Match();
    }
    Require(FT_List_Find_nid_postfix(nullptr, nullptr) == nullptr);
    std::printf("list mutations: 2048 states compared with native FreeType\n");
}

template<typename TNode>
struct Walk {
    std::conditional_t<std::is_same_v<TNode, GuestListNodeRec>, GuestListRec, FT_ListRec>* list;
    Nodes<TNode>* nodes;
    bool guest;
    bool remove;
    std::size_t stop;
    int error;
    std::vector<int> visits;
};

template<typename TNode>
static int Visit(TNode* node, void* user, bool guest) {
    auto& state = *static_cast<Walk<TNode>*>(user);
    Require(state.guest == guest);
    state.visits.push_back(Index(node, *state.nodes));
    if (state.remove) {
        if constexpr (std::is_same_v<TNode, GuestListNodeRec>) FT_List_Remove_nid_postfix(state.list, node);
        else FT_List_Remove(state.list, node);
        node->prev = nullptr;
        node->next = nullptr;
    }
    return state.visits.size() == state.stop ? state.error : 0;
}

static int NativeIterator(FT_ListNode node, void* user) { return Visit(node, user, false); }
static int APS5_VABI GuestIterator(GuestListNodeRec* node, void* user) { return Visit(node, user, true); }

static void IterationCases() {
    for (const bool remove : {false, true}) {
        for (const std::size_t stop : {0u, 1u, 3u, 16u}) {
            Lists lists;
            lists.Fill();
            const int error = stop == 3 ? -29 : 73;
            Walk guest{&lists.guest, &lists.actual, true, remove, stop, error, {}};
            Walk native{&lists.native, &lists.expected, false, remove, stop, error, {}};
            const int result = FT_List_Iterate_nid_postfix(&lists.guest, GuestIterator, &guest);
            Require(result == FT_List_Iterate(&lists.native, NativeIterator, &native));
            Require(result == (stop == 0 ? 0 : error));
            Require(guest.visits == native.visits && guest.visits.size() == (stop == 0 ? Count : stop));
            lists.Match();
        }
    }
    Lists empty;
    Walk guest{&empty.guest, &empty.actual, true, false, 0, 0, {}};
    Walk native{&empty.native, &empty.expected, false, false, 0, 0, {}};
    Require(FT_List_Iterate_nid_postfix(&empty.guest, GuestIterator, &guest) == 0 && guest.visits.empty());
    Require(FT_List_Iterate_nid_postfix(nullptr, GuestIterator, &guest) == FT_List_Iterate(nullptr, NativeIterator, &native));
    Require(FT_List_Iterate_nid_postfix(&empty.guest, nullptr, &guest) == FT_List_Iterate(&empty.native, nullptr, &native));
    Require(FT_List_Iterate_nid_postfix(nullptr, nullptr, nullptr) == FT_List_Iterate(nullptr, nullptr, nullptr));
    std::printf("list iteration: 8 callback/order/error/removal cases compared\n");
}

struct Event {
    int kind;
    int id;
    bool cleared;
    bool operator==(const Event&) const = default;
};

template<typename TList>
struct Disposal {
    TList* list;
    FT_Memory native;
    GuestMemoryRec* guest;
    void* user;
    std::map<void*, int> live;
    std::vector<Event> events;
};

template<typename TList>
static void Release(Disposal<TList>& state, void* block) {
    const auto found = state.live.find(block);
    Require(found != state.live.end());
    state.events.push_back({2, found->second, state.list->head == nullptr || state.list->tail == nullptr});
    state.live.erase(found);
    using Node = std::conditional_t<std::is_same_v<TList, GuestListRec>, GuestListNodeRec, FT_ListNodeRec>;
    delete static_cast<Node*>(block);
}

static void NativeFree(FT_Memory memory, void* block) {
    auto& state = *static_cast<Disposal<FT_ListRec>*>(memory->user);
    Require(state.native == memory);
    Release(state, block);
}

static void APS5_VABI GuestFree(GuestMemoryRec* memory, void* block) {
    auto& state = *static_cast<Disposal<GuestListRec>*>(memory->user);
    Require(state.guest == memory);
    Release(state, block);
}

static void NativeDestroy(FT_Memory memory, void* data, void* user) {
    auto& state = *static_cast<Disposal<FT_ListRec>*>(memory->user);
    Require(state.native == memory && state.user == user);
    state.events.push_back({1, data ? *static_cast<int*>(data) : -1, state.list->head == nullptr || state.list->tail == nullptr});
}

static void APS5_VABI GuestDestroy(GuestMemoryRec* memory, void* data, void* user) {
    auto& state = *static_cast<Disposal<GuestListRec>*>(memory->user);
    Require(state.guest == memory && state.user == user);
    state.events.push_back({1, data ? *static_cast<int*>(data) : -1, state.list->head == nullptr || state.list->tail == nullptr});
}

static void FinalizationCases() {
    for (const bool destroy : {false, true}) {
        for (const unsigned count : {0u, 1u, 8u}) {
            GuestListRec guestList{};
            FT_ListRec nativeList{};
            std::array<int, 8> ids{0, 1, 2, 3, 4, 5, 6, 7};
            int token = 9;
            Disposal guest{&guestList, nullptr, nullptr, &token, {}, {}};
            Disposal native{&nativeList, nullptr, nullptr, &token, {}, {}};
            GuestMemoryRec guestMemory{&guest, nullptr, GuestFree, nullptr};
            FT_MemoryRec_ nativeMemory{&native, nullptr, NativeFree, nullptr};
            guest.guest = &guestMemory;
            native.native = &nativeMemory;
            for (unsigned i = 0; i < count; ++i) {
                auto* a = new GuestListNodeRec{};
                auto* b = new FT_ListNodeRec{};
                a->data = b->data = i == 0 ? nullptr : &ids[i];
                guest.live.emplace(a, i);
                native.live.emplace(b, i);
                FT_List_Add_nid_postfix(&guestList, a);

                FT_List_Add(&nativeList, b);
            }
            const auto savedHead = guestList.head;
            FT_List_Finalize_nid_postfix(&guestList, GuestDestroy, nullptr, &token);

            Require(guestList.head == savedHead && guest.events.empty());
            FT_List_Finalize_nid_postfix(nullptr, GuestDestroy, &guestMemory, &token);

            Require(guest.events.empty());
            FT_List_Finalize_nid_postfix(&guestList, destroy ? GuestDestroy : nullptr, &guestMemory, &token);

            FT_List_Finalize(&nativeList, destroy ? NativeDestroy : nullptr, &nativeMemory, &token);
            Require(guestList.head == nullptr && guestList.tail == nullptr && nativeList.head == nullptr && nativeList.tail == nullptr);
            Require(guest.live.empty() && native.live.empty());
            Require(guest.events == native.events && guest.events.size() == count * (destroy ? 2u : 1u));
            for (const auto& event : guest.events) Require(!event.cleared);
        }
    }
    std::printf("list finalization: 6 allocator/destructor/order cases compared\n");
}

int main() {
    Mutations();
    IterationCases();
    FinalizationCases();
}
