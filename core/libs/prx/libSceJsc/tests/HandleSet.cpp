#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using Jsc::HandleAbi::HandleNode;
using Jsc::HandleAbi::HandleNodeList;
using Jsc::HandleAbi::HandleSet;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "HandleSet check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

constexpr std::uint64_t Empty = 0;
constexpr std::uint64_t Cell = 0x7f0012345670ull;
constexpr std::uint64_t OtherCell = 0x7f0012345680ull;
constexpr std::uint64_t Int32 = 0xfffe00000000002aull;
constexpr std::uint64_t Double = 0x0002000000000000ull;
constexpr std::uint64_t Undefined = 0xaull;

void Init(HandleNodeList& list) {
    list.headSentinel = {0, nullptr, &list.tailSentinel};
    list.tailSentinel = {0, &list.headSentinel, nullptr};
}

void Push(HandleNodeList& list, HandleNode* node) {
    node->prev = &list.headSentinel;
    node->next = list.headSentinel.next;
    list.headSentinel.next->prev = node;
    list.headSentinel.next = node;
}

bool Contains(HandleNodeList& list, const HandleNode* node) {
    for (HandleNode* it = list.headSentinel.next; it != &list.tailSentinel; it = it->next) {
        if (it->next->prev != it) return false;
        if (it == node) return true;
    }
    return false;
}

void Barrier(HandleSet& set, HandleNode& node, std::uint64_t value) {
    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node.value, &value);
    node.value = value;
}

template<typename F> bool Throws(F function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestMovesBetweenLists() {
    HandleSet set{};
    Init(set.strongList);
    Init(set.immediateList);
    HandleNode a{Empty, nullptr, nullptr};
    HandleNode b{Empty, nullptr, nullptr};
    Push(set.immediateList, &a);
    Push(set.immediateList, &b);

    Barrier(set, a, Cell);
    Require(Contains(set.strongList, &a) && !Contains(set.immediateList, &a));
    Require(Contains(set.immediateList, &b));
    Require(set.strongList.headSentinel.next == &a);

    HandleNode* prev = a.prev;
    HandleNode* next = a.next;
    Barrier(set, a, OtherCell);
    Require(a.prev == prev && a.next == next);

    Barrier(set, a, Int32);
    Require(Contains(set.immediateList, &a) && !Contains(set.strongList, &a));
    Require(set.immediateList.headSentinel.next == &a);

    Barrier(set, a, Double);
    Require(Contains(set.immediateList, &a));
    Barrier(set, a, Undefined);
    Require(Contains(set.immediateList, &a));

    Barrier(set, b, Cell);
    Barrier(set, a, Cell);
    Require(set.strongList.headSentinel.next == &a && a.next == &b && b.next == &set.strongList.tailSentinel);
    Require(set.strongList.tailSentinel.prev == &b);

    Barrier(set, a, Empty);
    Require(Contains(set.immediateList, &a) && Contains(set.strongList, &b) && !Contains(set.strongList, &a));
    Require(set.strongList.headSentinel.next == &b && b.prev == &set.strongList.headSentinel);
    Require(set.immediateList.headSentinel.next == &a && a.next == &set.immediateList.tailSentinel);
}

void TestEmptyToImmediate() {
    HandleSet set{};
    Init(set.strongList);
    Init(set.immediateList);
    HandleNode node{Empty, nullptr, nullptr};
    Push(set.immediateList, &node);
    Barrier(set, node, Int32);
    Require(Contains(set.immediateList, &node) && set.immediateList.headSentinel.next == &node);
}

void TestInvalidArguments() {
    HandleSet set{};
    Init(set.strongList);
    Init(set.immediateList);
    HandleNode node{Empty, nullptr, nullptr};
    const std::uint64_t value = Cell;
    Require(Throws([&] { _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(nullptr, &node.value, &value); }));
    Require(Throws([&] { _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, nullptr, &value); }));
    Require(Throws([&] { _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node.value, nullptr); }));
    Require(Throws([&] { _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node.value, &value); }));
    const std::uint64_t empty = Empty;
    _ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_(&set, &node.value, &empty);
    Require(!node.prev && !node.next);
}

} // namespace

int main() {
    TestMovesBetweenLists();
    TestEmptyToImmediate();
    TestInvalidArguments();
    std::puts("HandleSet tests passed");
    return 0;
}
