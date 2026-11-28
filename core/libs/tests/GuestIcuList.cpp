#include "prx/libSceIcu/libSceIcu.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t Warning = -124;

struct Heap {
    std::vector<void*> live;
    std::int32_t allocations = 0;
    std::int32_t releases = 0;
    std::int32_t unknownReleases = 0;
    bool exhausted = false;
};

Heap heap;

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t size) {
    if (heap.exhausted) return nullptr;
    void* block = std::malloc(size);
    heap.live.push_back(block);
    ++heap.allocations;
    return block;
}

void* APS5_VABI Reallocate_nid_no_patch(const void*, void*, std::size_t) {
    std::abort();
}

void APS5_VABI Release_nid_no_patch(const void*, void* block) {
    ++heap.releases;
    const auto found = std::find(heap.live.begin(), heap.live.end(), block);
    if (found == heap.live.end()) {
        ++heap.unknownReleases;
        return;
    }
    heap.live.erase(found);
    std::free(block);
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU list: %s\n", message);
    std::abort();
}

template<class F>
bool Throws(F call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

bool Live(const void* block) {
    return std::find(heap.live.begin(), heap.live.end(), block) != heap.live.end();
}

char* Owned(const char* text) {
    char* copy = static_cast<char*>(uprv_malloc_nid_postfix(std::strlen(text) + 1));
    std::strcpy(copy, text);
    return copy;
}

UList* NewList() {
    std::int32_t status = Success;
    UList* list = ulist_createEmptyList_nid_postfix(&status);
    Require(list != nullptr && status == Success, "an empty list is created");
    return list;
}

void Add(UList* list, const char* text, bool atEnd = true) {
    std::int32_t status = Success;
    if (atEnd) ulist_addItemEndList_nid_postfix(list, text, 0, &status);
    else ulist_addItemBeginList_nid_postfix(list, text, 0, &status);
    Require(status == Success, "an item is added");
}

bool Next(UList* list, const char* expected) {
    const auto* value = static_cast<const char*>(ulist_getNext_nid_postfix(list));
    if (!expected) return value == nullptr;
    return value != nullptr && std::strcmp(value, expected) == 0;
}

void Order() {
    UList* list = NewList();
    Require(heap.allocations == 1 && Live(list), "the list is one block from the installed allocator");
    Require(ulist_getListSize_nid_postfix(list) == 0, "a new list is empty");
    ulist_resetList_nid_postfix(list);
    Require(Next(list, nullptr), "an empty list has no next item");
    Add(list, "b");
    Add(list, "c");
    Add(list, "a", false);
    Require(ulist_getListSize_nid_postfix(list) == 3 && heap.allocations == 4, "three items, one block each");
    Require(Next(list, nullptr), "iteration has no position before the first reset");
    ulist_resetList_nid_postfix(list);
    Require(Next(list, "a") && Next(list, "b") && Next(list, "c") && Next(list, nullptr), "items come back head to tail");
    Require(Next(list, nullptr), "the end is sticky");
    ulist_resetList_nid_postfix(list);
    Require(Next(list, "a"), "reset returns to the head");
    ulist_deleteList_nid_postfix(list);
    Require(heap.live.empty() && heap.releases == 4 && heap.unknownReleases == 0, "delete releases every node and the list");
}

void SingleItemAtBegin() {
    UList* list = NewList();
    Add(list, "only", false);
    Add(list, "last");
    Add(list, "first", false);
    ulist_resetList_nid_postfix(list);
    Require(Next(list, "first") && Next(list, "only") && Next(list, "last") && Next(list, nullptr), "a first item added at the beginning links both ways");
    ulist_deleteList_nid_postfix(list);
    Require(heap.live.empty(), "delete releases the list");
}

void Contains() {
    UList* list = NewList();
    Add(list, "alpha");
    Add(list, "be");
    Require(ulist_containsString_nid_postfix(list, "be", 2) == 1, "an item is found");
    Require(ulist_containsString_nid_postfix(list, "beta", 2) == 1, "only the given length is compared");
    Require(ulist_containsString_nid_postfix(list, "be", 1) == 0, "a shorter length matches no item of another length");
    Require(ulist_containsString_nid_postfix(list, "alphabet", 8) == 0, "a longer string is not found");
    Require(ulist_containsString_nid_postfix(list, "bx", 2) == 0, "same length, different bytes");
    Require(ulist_containsString_nid_postfix(list, "", 0) == 0, "no empty item yet");
    Require(ulist_containsString_nid_postfix(list, "be", -1) == 0, "a negative length matches nothing");
    Require(ulist_containsString_nid_postfix(list, nullptr, 3) == 0, "a null string is not read when no item has the length");
    Require(Throws([&] { ulist_containsString_nid_postfix(list, nullptr, 2); }), "a null string that would be compared");
    Add(list, "");
    Require(ulist_containsString_nid_postfix(list, nullptr, 0) == 1, "length zero matches an empty item without reading the string");
    Require(ulist_containsString_nid_postfix(nullptr, "be", 2) == 0, "a null list contains nothing");
    ulist_deleteList_nid_postfix(list);
}

void Removal() {
    UList* list = NewList();
    for (const char* text : {"a", "b", "a", "c"}) Add(list, text);
    Require(ulist_removeString_nid_postfix(list, "zz") == 0 && ulist_getListSize_nid_postfix(list) == 4, "an absent string removes nothing");
    const std::int32_t releases = heap.releases;
    Require(ulist_removeString_nid_postfix(list, "a") == 1 && ulist_getListSize_nid_postfix(list) == 3, "the head is removed");
    Require(heap.releases == releases + 1, "removal releases the node and not its borrowed data");
    ulist_resetList_nid_postfix(list);
    Require(Next(list, "b") && Next(list, "a") && Next(list, "c") && Next(list, nullptr), "only the first occurrence is removed");
    ulist_resetList_nid_postfix(list);
    Require(Next(list, "b"), "the position is on the second item");
    Require(ulist_removeString_nid_postfix(list, "a") == 1, "the item at the position is removed");
    Require(Next(list, "c") && Next(list, nullptr), "the position moves to the following item");
    Require(ulist_removeString_nid_postfix(list, "c") == 1, "the tail is removed");
    Add(list, "d");
    ulist_resetList_nid_postfix(list);
    Require(Next(list, "b") && Next(list, "d") && Next(list, nullptr), "the list links again after its tail was removed");
    Require(ulist_removeString_nid_postfix(list, "b") == 1 && ulist_removeString_nid_postfix(list, "d") == 1, "the last items are removed");
    Require(ulist_getListSize_nid_postfix(list) == 0, "the list is empty again");
    Require(ulist_removeString_nid_postfix(list, nullptr) == 0, "an empty list does not read the string");
    Add(list, "e");
    Add(list, "f", false);
    ulist_resetList_nid_postfix(list);
    Require(Next(list, "f") && Next(list, "e") && Next(list, nullptr), "an emptied list accepts items again");
    Require(Throws([&] { ulist_removeString_nid_postfix(list, nullptr); }), "a null string that would be compared");
    Require(ulist_removeString_nid_postfix(nullptr, "a") == 0, "a null list removes nothing");
    ulist_deleteList_nid_postfix(list);
    Require(heap.live.empty(), "delete releases the list");
}

void OwnedData() {
    UList* list = NewList();
    std::int32_t status = Success;
    char* first = Owned("first");
    char* second = Owned("second");
    ulist_addItemEndList_nid_postfix(list, first, 1, &status);
    ulist_addItemBeginList_nid_postfix(list, second, 1, &status);
    Add(list, "borrowed");
    Require(status == Success && ulist_getListSize_nid_postfix(list) == 3, "owned and borrowed items are added");
    Require(ulist_removeString_nid_postfix(list, "first") == 1 && !Live(first) && Live(second), "removal releases owned data");
    ulist_deleteList_nid_postfix(list);
    Require(heap.live.empty() && heap.unknownReleases == 0, "delete releases owned data and never borrowed data");

    list = NewList();
    void* empty = uprv_malloc_nid_postfix(0);
    Require(empty != nullptr && !Live(empty), "a zero-byte request is not an allocation");
    const std::int32_t releases = heap.releases;
    ulist_addItemEndList_nid_postfix(list, empty, 1, &status);
    ulist_addItemEndList_nid_postfix(nullptr, empty, 1, &status);
    Require(ulist_removeString_nid_postfix(list, "") == 1 && heap.releases == releases + 1, "an owned zero-byte block is not passed to the release function");
    ulist_addItemBeginList_nid_postfix(list, empty, 1, &status);
    ulist_deleteList_nid_postfix(list);
    Require(heap.live.empty() && heap.unknownReleases == 0 && heap.releases == releases + 3, "delete skips an owned zero-byte block");
}

void Refusals() {
    const std::int32_t allocations = heap.allocations;
    for (std::int32_t incoming : {IllegalArgument, MemoryAllocationError}) {
        std::int32_t status = incoming;
        Require(ulist_createEmptyList_nid_postfix(&status) == nullptr && status == incoming, "a set error creates nothing");
    }
    Require(heap.allocations == allocations, "a set error allocates nothing");
    std::int32_t status = Warning;
    UList* list = ulist_createEmptyList_nid_postfix(&status);
    Require(list != nullptr && status == Warning, "a warning does not stop creation");

    status = IllegalArgument;
    ulist_addItemEndList_nid_postfix(list, "kept", 0, &status);
    ulist_addItemBeginList_nid_postfix(list, "kept", 0, &status);
    Require(ulist_getListSize_nid_postfix(list) == 0 && status == IllegalArgument, "a set error adds nothing");
    char* owned = Owned("owned");
    ulist_addItemEndList_nid_postfix(list, owned, 1, &status);
    Require(!Live(owned), "a set error releases owned data at the end");
    owned = Owned("owned");
    ulist_addItemBeginList_nid_postfix(list, owned, 1, &status);
    Require(!Live(owned), "a set error releases owned data at the beginning");

    status = Success;
    ulist_addItemEndList_nid_postfix(list, nullptr, 0, &status);
    ulist_addItemBeginList_nid_postfix(list, nullptr, 0, &status);
    Require(ulist_getListSize_nid_postfix(list) == 0 && status == Success, "null data adds nothing");
    owned = Owned("owned");
    ulist_addItemEndList_nid_postfix(nullptr, owned, 1, &status);
    Require(!Live(owned) && status == Success, "a null list releases owned data at the end");
    owned = Owned("owned");
    ulist_addItemBeginList_nid_postfix(nullptr, owned, 1, &status);
    Require(!Live(owned) && status == Success, "a null list releases owned data at the beginning");

    Require(Throws([&] { ulist_createEmptyList_nid_postfix(nullptr); }), "create with a null status");
    Require(Throws([&] { ulist_addItemEndList_nid_postfix(list, "x", 0, nullptr); }), "add at the end with a null status");
    Require(Throws([&] { ulist_addItemBeginList_nid_postfix(list, "x", 0, nullptr); }), "add at the beginning with a null status");

    Require(ulist_getListSize_nid_postfix(nullptr) == -1, "a null list has size -1");
    Require(ulist_getNext_nid_postfix(nullptr) == nullptr, "a null list has no next item");
    ulist_resetList_nid_postfix(nullptr);
    ulist_deleteList_nid_postfix(nullptr);
    ulist_deleteList_nid_postfix(list);
    Require(heap.live.empty() && heap.unknownReleases == 0, "every block is released once");
}

void Exhaustion() {
    UList* list = NewList();
    char* owned = Owned("owned");
    heap.exhausted = true;
    std::int32_t status = Success;
    Require(ulist_createEmptyList_nid_postfix(&status) == nullptr && status == MemoryAllocationError, "a failed allocation reports a memory error");
    status = Success;
    ulist_addItemEndList_nid_postfix(list, "borrowed", 0, &status);
    Require(status == MemoryAllocationError && ulist_getListSize_nid_postfix(list) == 0, "a failed node allocation at the end adds nothing");
    status = Success;
    ulist_addItemBeginList_nid_postfix(list, owned, 1, &status);
    Require(status == MemoryAllocationError && !Live(owned), "a failed node allocation releases owned data");
    heap.exhausted = false;
    ulist_deleteList_nid_postfix(list);
    Require(heap.live.empty(), "delete releases the list");
}

UEnumeration* KeywordValues(UList* list) {
    auto* enumeration = static_cast<UEnumeration*>(uprv_malloc_nid_postfix(sizeof(UEnumeration)));
    *enumeration = UEnumeration{nullptr, list, ulist_close_keyword_values_iterator_nid_postfix, ulist_count_keyword_values_nid_postfix, nullptr,
                                ulist_next_keyword_value_nid_postfix, ulist_reset_keyword_values_iterator_nid_postfix};
    return enumeration;
}

void Enumeration() {
    UList* list = NewList();
    for (const char* text : {"gregorian", "", "buddhist"}) Add(list, text);
    UEnumeration* enumeration = KeywordValues(list);
    Require(ulist_getListFromEnum_nid_postfix(enumeration) == list, "the list is the enumeration context");

    std::int32_t status = Success;
    Require(uenum_count_nid_postfix(enumeration, &status) == 3 && status == Success, "count is the list size");
    std::int32_t length = -1;
    Require(uenum_next_nid_postfix(enumeration, &length, &status) == nullptr && length == -1, "no position before the first reset");
    enumeration->reset(enumeration, &status);
    const char* value = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(value && std::strcmp(value, "gregorian") == 0 && length == 9, "the first value and its length");
    value = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(value && *value == 0 && length == 0, "an empty value has length zero");
    length = 77;
    value = ulist_next_keyword_value_nid_postfix(enumeration, nullptr, &status);
    Require(value && std::strcmp(value, "buddhist") == 0 && length == 77, "a null length slot is not written");
    Require(uenum_next_nid_postfix(enumeration, &length, &status) == nullptr && length == 77 && status == Success, "the end leaves the length alone");

    enumeration->reset(enumeration, &status);
    Require(uenum_next_nid_postfix(enumeration, &length, &status) != nullptr && length == 9, "the first value again");
    status = IllegalArgument;
    length = 77;
    Require(ulist_next_keyword_value_nid_postfix(enumeration, &length, &status) == nullptr && length == 77, "next with an error set");
    status = Success;
    value = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(value && length == 0, "next with an error set did not move the position");
    Require(uenum_next_nid_postfix(enumeration, &length, &status) != nullptr && uenum_next_nid_postfix(enumeration, &length, &status) == nullptr, "the end again");
    status = IllegalArgument;
    ulist_reset_keyword_values_iterator_nid_postfix(enumeration, &status);
    Require(ulist_count_keyword_values_nid_postfix(enumeration, &status) == -1 && status == IllegalArgument, "count with an error set");
    status = Success;
    Require(uenum_next_nid_postfix(enumeration, &length, &status) == nullptr, "a reset with an error set did not move the position");
    ulist_reset_keyword_values_iterator_nid_postfix(enumeration, &status);
    Require(uenum_next_nid_postfix(enumeration, &length, &status) != nullptr && length == 9, "reset returns to the first value");

    Require(Throws([&] { ulist_count_keyword_values_nid_postfix(enumeration, nullptr); }), "count with a null status");
    Require(Throws([&] { ulist_next_keyword_value_nid_postfix(enumeration, &length, nullptr); }), "next with a null status");
    Require(Throws([&] { ulist_reset_keyword_values_iterator_nid_postfix(enumeration, nullptr); }), "reset with a null status");
    Require(Throws([&] { ulist_count_keyword_values_nid_postfix(nullptr, &status); }), "count with a null enumeration");
    Require(Throws([&] { ulist_next_keyword_value_nid_postfix(nullptr, &length, &status); }), "next with a null enumeration");
    Require(Throws([&] { ulist_reset_keyword_values_iterator_nid_postfix(nullptr, &status); }), "reset with a null enumeration");
    Require(Throws([&] { ulist_getListFromEnum_nid_postfix(nullptr); }), "list of a null enumeration");

    uenum_close_nid_postfix(enumeration);
    Require(heap.live.empty() && heap.unknownReleases == 0, "close releases the list, its nodes and the enumeration");
    ulist_close_keyword_values_iterator_nid_postfix(nullptr);

    enumeration = KeywordValues(nullptr);
    status = Success;
    Require(uenum_count_nid_postfix(enumeration, &status) == -1 && status == Success, "an enumeration without a list counts -1");
    Require(uenum_next_nid_postfix(enumeration, &length, &status) == nullptr, "an enumeration without a list has no values");
    uenum_close_nid_postfix(enumeration);
    Require(heap.live.empty() && heap.unknownReleases == 0, "close releases an enumeration without a list");
}

}

int main() {
    std::int32_t status = Success;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == Success, "the tracking allocator is installed");
    Order();
    SingleItemAtBegin();
    Contains();
    Removal();
    OwnedData();
    Refusals();
    Exhaustion();
    Enumeration();
    u_cleanup_nid_postfix();
    std::puts("ICU list tests passed");
    return 0;
}
