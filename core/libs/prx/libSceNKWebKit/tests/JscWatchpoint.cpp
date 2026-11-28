#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

using NKWebKit::JscAbi::ClearWatchpoint;
using NKWebKit::JscAbi::DeferredWatchpointFire;
using NKWebKit::JscAbi::IsInvalidated;
using NKWebKit::JscAbi::IsWatched;
using NKWebKit::JscAbi::NodeNext;
using NKWebKit::JscAbi::NodePrev;
using NKWebKit::JscAbi::SentinelAddress;
using NKWebKit::JscAbi::SentinelNext;
using NKWebKit::JscAbi::SentinelPrev;
using NKWebKit::JscAbi::SetNodeNext;
using NKWebKit::JscAbi::SetNodePrev;
using NKWebKit::JscAbi::SetSentinelNext;
using NKWebKit::JscAbi::SetSentinelPrev;
using NKWebKit::JscAbi::WatchpointNode;
using NKWebKit::JscAbi::WatchpointSet;
using NKWebKit::JscAbi::WatchpointState;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "JscWatchpoint check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

using Constructor = void (APS5_VABI*)(WatchpointSet*, WatchpointState);
using Destructor = void (APS5_VABI*)(WatchpointSet*);
using FireAllSlowDeferred = void (APS5_VABI*)(WatchpointSet*, void*, DeferredWatchpointFire*);

static_assert(std::is_same_v<decltype(&_ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE), Constructor>);
static_assert(std::is_same_v<decltype(&_ZN3JSC13WatchpointSetC2ENS_15WatchpointStateE), Constructor>);
static_assert(std::is_same_v<decltype(&_ZN3JSC13WatchpointSetD1Ev), Destructor>);
static_assert(std::is_same_v<decltype(&_ZN3JSC13WatchpointSetD2Ev), Destructor>);
static_assert(std::is_same_v<decltype(&_ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE), FireAllSlowDeferred>);

void CheckSentinelSelfLinked(WatchpointSet& set) {
    Require(SentinelNext(set) == SentinelAddress(set));
    Require(SentinelPrev(set) == SentinelAddress(set));
}

WatchpointSet* Constructed(WatchpointState state) {
    auto* set = new WatchpointSet;
    std::memset(set, 0xAA, sizeof(WatchpointSet));
    _ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE(set, state);
    return set;
}

void TestLayout() {
    static_assert(sizeof(WatchpointSet) == 20);
    static_assert(alignof(WatchpointSet) == 4);
    static_assert(offsetof(WatchpointSet, refCount) == 0);
    static_assert(offsetof(WatchpointSet, state) == 4);
    static_assert(offsetof(WatchpointSet, setIsNotEmpty) == 5);
    static_assert(offsetof(WatchpointSet, sentinel) == 6);
    static_assert(sizeof(WatchpointNode) == 12);
    static_assert(alignof(WatchpointNode) == 1);
    static_assert(offsetof(WatchpointNode, next) == 0);
    static_assert(offsetof(WatchpointNode, prev) == 6);
    static_assert(sizeof(DeferredWatchpointFire) == 20);
    static_assert(alignof(DeferredWatchpointFire) == 4);
    static_assert(offsetof(DeferredWatchpointFire, watchpointsToFire) == 0);
    Require(ClearWatchpoint == 0);
    Require(IsWatched == 1);
    Require(IsInvalidated == 2);
}

void TestPackedLinks() {
    std::unique_ptr<WatchpointSet> set(Constructed(IsWatched));
    auto* base = reinterpret_cast<unsigned char*>(set.get());
    Require(SentinelAddress(*set) == base + 6);
    Require(SentinelNext(*set) == base + 6);
    Require(SentinelPrev(*set) == base + 6);
    std::uint64_t packed = 0;
    std::memcpy(&packed, base + 6, 6);
    Require(packed == ((reinterpret_cast<std::uintptr_t>(base) + 6) & 0xFFFFFFFFFFFFull));
    std::memcpy(&packed, base + 12, 6);
    Require(packed == ((reinterpret_cast<std::uintptr_t>(base) + 6) & 0xFFFFFFFFFFFFull));
    WatchpointNode node;
    std::memset(&node, 0, sizeof(node));
    void* high = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xFFFF800000001234ull));
    SetNodeNext(&node, high);
    Require(NodeNext(&node) == high);
    Require(node.next[5] == 0x80);
    Require(node.next[0] == 0x34);
    void* low = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x00007FFF00001234ull));
    SetNodePrev(&node, low);
    Require(NodePrev(&node) == low);
    Require(NodeNext(&node) == high);
}

void TestConstructor() {
    {
        std::unique_ptr<WatchpointSet> set(Constructed(ClearWatchpoint));
        Require(set->refCount == 1);
        Require(set->state == static_cast<std::int8_t>(ClearWatchpoint));
        Require(set->setIsNotEmpty == 0);
        CheckSentinelSelfLinked(*set);
    }
    {
        std::unique_ptr<WatchpointSet> set(Constructed(IsWatched));
        Require(set->refCount == 1);
        Require(set->state == static_cast<std::int8_t>(IsWatched));
        Require(set->setIsNotEmpty == 0);
        CheckSentinelSelfLinked(*set);
    }
    {
        std::unique_ptr<WatchpointSet> set(Constructed(IsInvalidated));
        Require(set->state == static_cast<std::int8_t>(IsInvalidated));
        CheckSentinelSelfLinked(*set);
    }
    {
        alignas(4) std::uint8_t storage[sizeof(WatchpointSet)];
        std::memset(storage, 0x55, sizeof(storage));
        auto* set = reinterpret_cast<WatchpointSet*>(storage);
        _ZN3JSC13WatchpointSetC2ENS_15WatchpointStateE(set, WatchpointState{200});
        Require(set->refCount == 1);
        Require(set->state == static_cast<std::int8_t>(200));
        Require(set->setIsNotEmpty == 0);
        CheckSentinelSelfLinked(*set);
        Require(storage[18] == 0x55);
        Require(storage[19] == 0x55);
    }
    {
        std::unique_ptr<WatchpointSet> set(Constructed(IsWatched));
        WatchpointNode node;
        std::memset(&node, 0, sizeof(node));
        SetSentinelNext(*set, &node);
        SetSentinelPrev(*set, &node);
        SetNodePrev(&node, SentinelAddress(*set));
        SetNodeNext(&node, SentinelAddress(*set));
        _ZN3JSC13WatchpointSetD1Ev(set.get());
        CheckSentinelSelfLinked(*set);
        Require(NodeNext(&node) == nullptr);
        Require(NodePrev(&node) == nullptr);
    }
}

void TestDestructorRemovesListedWatchpoints() {
    {
        std::unique_ptr<WatchpointSet> set(Constructed(IsWatched));
        WatchpointNode first;
        std::memset(&first, 0, sizeof(first));
        WatchpointNode second;
        std::memset(&second, 0, sizeof(second));
        SetSentinelNext(*set, &first);
        SetSentinelPrev(*set, &second);
        SetNodePrev(&first, SentinelAddress(*set));
        SetNodeNext(&first, &second);
        SetNodePrev(&second, &first);
        SetNodeNext(&second, SentinelAddress(*set));
        _ZN3JSC13WatchpointSetD2Ev(set.get());
        CheckSentinelSelfLinked(*set);
        Require(NodeNext(&first) == nullptr);
        Require(NodePrev(&first) == nullptr);
        Require(NodeNext(&second) == nullptr);
        Require(NodePrev(&second) == nullptr);
    }
    {
        std::unique_ptr<WatchpointSet> set(Constructed(ClearWatchpoint));
        WatchpointNode only;
        std::memset(&only, 0, sizeof(only));
        SetSentinelNext(*set, &only);
        SetSentinelPrev(*set, &only);
        SetNodePrev(&only, SentinelAddress(*set));
        SetNodeNext(&only, SentinelAddress(*set));
        _ZN3JSC13WatchpointSetD1Ev(set.get());
        CheckSentinelSelfLinked(*set);
        Require(NodeNext(&only) == nullptr);
        Require(NodePrev(&only) == nullptr);
    }
    {
        std::unique_ptr<WatchpointSet> set(Constructed(IsInvalidated));
        _ZN3JSC13WatchpointSetD2Ev(set.get());
        CheckSentinelSelfLinked(*set);
        Require(set->state == static_cast<std::int8_t>(IsInvalidated));
        Require(set->refCount == 1);
        Require(set->setIsNotEmpty == 0);
    }
}

void TestFireAllSlowDeferred() {
    {
        std::unique_ptr<WatchpointSet> fired(Constructed(IsWatched));
        std::unique_ptr<DeferredWatchpointFire> deferred(new DeferredWatchpointFire);
        std::memset(deferred.get(), 0x77, sizeof(DeferredWatchpointFire));
        _ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE(&deferred->watchpointsToFire, ClearWatchpoint);
        _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(fired.get(), nullptr, deferred.get());
        Require(fired->state == static_cast<std::int8_t>(IsInvalidated));
        Require(fired->setIsNotEmpty == 0);
        CheckSentinelSelfLinked(*fired);
        Require(deferred->watchpointsToFire.state == static_cast<std::int8_t>(IsWatched));
        Require(deferred->watchpointsToFire.setIsNotEmpty == 0);
        CheckSentinelSelfLinked(deferred->watchpointsToFire);
    }
    {
        std::unique_ptr<WatchpointSet> fired(Constructed(IsWatched));
        std::unique_ptr<DeferredWatchpointFire> deferred(new DeferredWatchpointFire);
        std::memset(deferred.get(), 0x77, sizeof(DeferredWatchpointFire));
        _ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE(&deferred->watchpointsToFire, ClearWatchpoint);
        WatchpointNode only;
        std::memset(&only, 0, sizeof(only));
        SetSentinelNext(*fired, &only);
        SetSentinelPrev(*fired, &only);
        SetNodePrev(&only, SentinelAddress(*fired));
        SetNodeNext(&only, SentinelAddress(*fired));
        fired->setIsNotEmpty = 1;
        _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(fired.get(), nullptr, deferred.get());
        Require(SentinelNext(deferred->watchpointsToFire) == &only);
        Require(SentinelPrev(deferred->watchpointsToFire) == &only);
        Require(NodePrev(&only) == SentinelAddress(deferred->watchpointsToFire));
        Require(NodeNext(&only) == SentinelAddress(deferred->watchpointsToFire));
        CheckSentinelSelfLinked(*fired);
        Require(deferred->watchpointsToFire.state == static_cast<std::int8_t>(IsWatched));
        Require(deferred->watchpointsToFire.setIsNotEmpty == 1);
        Require(fired->state == static_cast<std::int8_t>(IsInvalidated));
        Require(fired->setIsNotEmpty == 0);
    }
    {
        std::unique_ptr<WatchpointSet> fired(Constructed(IsWatched));
        std::unique_ptr<DeferredWatchpointFire> deferred(new DeferredWatchpointFire);
        std::memset(deferred.get(), 0x77, sizeof(DeferredWatchpointFire));
        _ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE(&deferred->watchpointsToFire, ClearWatchpoint);
        WatchpointNode kept;
        std::memset(&kept, 0, sizeof(kept));
        SetSentinelNext(deferred->watchpointsToFire, &kept);
        SetSentinelPrev(deferred->watchpointsToFire, &kept);
        SetNodePrev(&kept, SentinelAddress(deferred->watchpointsToFire));
        SetNodeNext(&kept, SentinelAddress(deferred->watchpointsToFire));
        deferred->watchpointsToFire.setIsNotEmpty = 1;
        WatchpointNode only;
        std::memset(&only, 0, sizeof(only));
        SetSentinelNext(*fired, &only);
        SetSentinelPrev(*fired, &only);
        SetNodePrev(&only, SentinelAddress(*fired));
        SetNodeNext(&only, SentinelAddress(*fired));
        fired->setIsNotEmpty = 1;
        _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(fired.get(), nullptr, deferred.get());
        Require(SentinelNext(deferred->watchpointsToFire) == &kept);
        Require(SentinelPrev(deferred->watchpointsToFire) == &only);
        Require(NodeNext(&kept) == &only);
        Require(NodePrev(&kept) == SentinelAddress(deferred->watchpointsToFire));
        Require(NodePrev(&only) == &kept);
        Require(NodeNext(&only) == SentinelAddress(deferred->watchpointsToFire));
        Require(deferred->watchpointsToFire.state == static_cast<std::int8_t>(IsWatched));
        Require(deferred->watchpointsToFire.setIsNotEmpty == 1);
        CheckSentinelSelfLinked(*fired);
        Require(fired->state == static_cast<std::int8_t>(IsInvalidated));
        Require(fired->setIsNotEmpty == 0);
    }
    {
        std::unique_ptr<WatchpointSet> fired(Constructed(ClearWatchpoint));
        std::unique_ptr<DeferredWatchpointFire> deferred(new DeferredWatchpointFire);
        std::memset(deferred.get(), 0x77, sizeof(DeferredWatchpointFire));
        _ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE(&deferred->watchpointsToFire, ClearWatchpoint);
        WatchpointNode only;
        std::memset(&only, 0, sizeof(only));
        SetSentinelNext(*fired, &only);
        SetSentinelPrev(*fired, &only);
        SetNodePrev(&only, SentinelAddress(*fired));
        SetNodeNext(&only, SentinelAddress(*fired));
        _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(fired.get(), nullptr, deferred.get());
        Require(SentinelNext(deferred->watchpointsToFire) == &only);
        Require(SentinelPrev(deferred->watchpointsToFire) == &only);
        Require(NodePrev(&only) == SentinelAddress(deferred->watchpointsToFire));
        Require(NodeNext(&only) == SentinelAddress(deferred->watchpointsToFire));
        Require(deferred->watchpointsToFire.state == static_cast<std::int8_t>(ClearWatchpoint));
        Require(deferred->watchpointsToFire.setIsNotEmpty == 0);
        CheckSentinelSelfLinked(*fired);
        Require(fired->state == static_cast<std::int8_t>(IsInvalidated));
        Require(fired->setIsNotEmpty == 0);
    }
    {
        std::unique_ptr<WatchpointSet> fired(Constructed(IsWatched));
        RequireThrows([&] {
            _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(nullptr, nullptr, nullptr);
        });
        RequireThrows([&] {
            _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(fired.get(), nullptr, nullptr);
        });
    }
}

void TestNullArguments() {
    RequireThrows([] {
        _ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE(nullptr, ClearWatchpoint);
    });
    RequireThrows([] {
        _ZN3JSC13WatchpointSetC2ENS_15WatchpointStateE(nullptr, IsWatched);
    });
    RequireThrows([] {
        _ZN3JSC13WatchpointSetD1Ev(nullptr);
    });
    RequireThrows([] {
        _ZN3JSC13WatchpointSetD2Ev(nullptr);
    });
}

}

int main() {
    TestLayout();
    TestPackedLinks();
    TestConstructor();
    TestDestructorRemovesListedWatchpoints();
    TestFireAllSlowDeferred();
    TestNullArguments();
    return 0;
}
