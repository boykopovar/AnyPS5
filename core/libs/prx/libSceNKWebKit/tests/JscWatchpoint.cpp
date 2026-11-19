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
using NKWebKit::JscAbi::SentinelAddress;
using NKWebKit::JscAbi::SentinelNext;
using NKWebKit::JscAbi::SentinelPrev;
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
    static_assert(sizeof(WatchpointSet) == 24);
    static_assert(alignof(WatchpointSet) == 4);
    static_assert(offsetof(WatchpointSet, refCount) == 0);
    static_assert(offsetof(WatchpointSet, state) == 4);
    static_assert(offsetof(WatchpointSet, setIsNotEmpty) == 5);
    static_assert(offsetof(WatchpointSet, sentinel) == 6);
    static_assert(sizeof(WatchpointNode) == 16);
    static_assert(offsetof(WatchpointNode, next) == 0);
    static_assert(offsetof(WatchpointNode, prev) == 8);
    static_assert(sizeof(DeferredWatchpointFire) == 24);
    static_assert(alignof(DeferredWatchpointFire) == 4);
    static_assert(offsetof(DeferredWatchpointFire, watchpointsToFire) == 0);
    Require(ClearWatchpoint == 0);
    Require(IsWatched == 1);
    Require(IsInvalidated == 2);
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
        Require(storage[22] == 0x55);
        Require(storage[23] == 0x55);
    }
    {
        std::unique_ptr<WatchpointSet> set(Constructed(IsWatched));
        WatchpointNode node{nullptr, nullptr};
        SetSentinelNext(*set, &node);
        SetSentinelPrev(*set, &node);
        node.prev = SentinelAddress(*set);
        node.next = SentinelAddress(*set);
        _ZN3JSC13WatchpointSetD1Ev(set.get());
        CheckSentinelSelfLinked(*set);
        Require(node.next == nullptr);
        Require(node.prev == nullptr);
    }
}

void TestDestructorRemovesListedWatchpoints() {
    {
        std::unique_ptr<WatchpointSet> set(Constructed(IsWatched));
        WatchpointNode first{nullptr, nullptr};
        WatchpointNode second{nullptr, nullptr};
        SetSentinelNext(*set, &first);
        SetSentinelPrev(*set, &second);
        first.prev = SentinelAddress(*set);
        first.next = &second;
        second.prev = &first;
        second.next = SentinelAddress(*set);
        _ZN3JSC13WatchpointSetD2Ev(set.get());
        CheckSentinelSelfLinked(*set);
        Require(first.next == nullptr);
        Require(first.prev == nullptr);
        Require(second.next == nullptr);
        Require(second.prev == nullptr);
    }
    {
        std::unique_ptr<WatchpointSet> set(Constructed(ClearWatchpoint));
        WatchpointNode only{nullptr, nullptr};
        SetSentinelNext(*set, &only);
        SetSentinelPrev(*set, &only);
        only.prev = SentinelAddress(*set);
        only.next = SentinelAddress(*set);
        _ZN3JSC13WatchpointSetD1Ev(set.get());
        CheckSentinelSelfLinked(*set);
        Require(only.next == nullptr);
        Require(only.prev == nullptr);
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
        WatchpointNode only{nullptr, nullptr};
        SetSentinelNext(*fired, &only);
        SetSentinelPrev(*fired, &only);
        only.prev = SentinelAddress(*fired);
        only.next = SentinelAddress(*fired);
        fired->setIsNotEmpty = 1;
        _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(fired.get(), nullptr, deferred.get());
        Require(SentinelNext(deferred->watchpointsToFire) == &only);
        Require(SentinelPrev(deferred->watchpointsToFire) == &only);
        Require(only.prev == SentinelAddress(deferred->watchpointsToFire));
        Require(only.next == SentinelAddress(deferred->watchpointsToFire));
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
        WatchpointNode kept{nullptr, nullptr};
        SetSentinelNext(deferred->watchpointsToFire, &kept);
        SetSentinelPrev(deferred->watchpointsToFire, &kept);
        kept.prev = SentinelAddress(deferred->watchpointsToFire);
        kept.next = SentinelAddress(deferred->watchpointsToFire);
        deferred->watchpointsToFire.setIsNotEmpty = 1;
        WatchpointNode only{nullptr, nullptr};
        SetSentinelNext(*fired, &only);
        SetSentinelPrev(*fired, &only);
        only.prev = SentinelAddress(*fired);
        only.next = SentinelAddress(*fired);
        fired->setIsNotEmpty = 1;
        _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(fired.get(), nullptr, deferred.get());
        Require(SentinelNext(deferred->watchpointsToFire) == &kept);
        Require(SentinelPrev(deferred->watchpointsToFire) == &only);
        Require(kept.next == &only);
        Require(kept.prev == SentinelAddress(deferred->watchpointsToFire));
        Require(only.prev == &kept);
        Require(only.next == SentinelAddress(deferred->watchpointsToFire));
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
        WatchpointNode only{nullptr, nullptr};
        SetSentinelNext(*fired, &only);
        SetSentinelPrev(*fired, &only);
        only.prev = SentinelAddress(*fired);
        only.next = SentinelAddress(*fired);
        _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(fired.get(), nullptr, deferred.get());
        Require(SentinelNext(deferred->watchpointsToFire) == &only);
        Require(SentinelPrev(deferred->watchpointsToFire) == &only);
        Require(only.prev == SentinelAddress(deferred->watchpointsToFire));
        Require(only.next == SentinelAddress(deferred->watchpointsToFire));
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
    TestConstructor();
    TestDestructorRemovesListedWatchpoints();
    TestFireAllSlowDeferred();
    TestNullArguments();
    return 0;
}
