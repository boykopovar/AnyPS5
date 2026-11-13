#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace {

using NKWebKit::InspectorAbi::AgentRegistry;
using NKWebKit::InspectorAbi::BackendDispatcher;
using NKWebKit::InspectorAbi::DisconnectReason;
using NKWebKit::InspectorAbi::FrontendRouter;
using NKWebKit::InspectorAbi::InspectorAgentBase;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Inspector::AgentRegistry check failed at line %u\n", location.line());
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

using Constructor = void (APS5_VABI*)(AgentRegistry*);
using Destructor = void (APS5_VABI*)(AgentRegistry*);
using DiscardValues = void (APS5_VABI*)(AgentRegistry*);
using DidCreateFrontendAndBackend = void (APS5_VABI*)(AgentRegistry*, FrontendRouter*, BackendDispatcher*);
using WillDestroyFrontendAndBackend = void (APS5_VABI*)(AgentRegistry*, DisconnectReason);
static_assert(std::is_same_v<decltype(&_ZN9Inspector13AgentRegistryC2Ev), Constructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector13AgentRegistryD1Ev), Destructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector13AgentRegistryD2Ev), Destructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector13AgentRegistry13discardValuesEv), DiscardValues>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector13AgentRegistry27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE), DidCreateFrontendAndBackend>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector13AgentRegistry29willDestroyFrontendAndBackendENS_16DisconnectReasonE), WillDestroyFrontendAndBackend>);

enum class CallSlot { DidCreate, WillDestroy, DiscardValues, DiscardAgent, Delete };

struct Call {
    CallSlot slot;
    int agent;
    FrontendRouter* router;
    BackendDispatcher* dispatcher;
    DisconnectReason reason;
};

constexpr int MaximumCalls = 64;
Call calls[MaximumCalls];
int callCount = 0;

void Record(CallSlot slot, int agent, DisconnectReason reason = DisconnectReason::InspectedTargetDestroyed) {
    Require(callCount < MaximumCalls);
    calls[callCount] = Call{slot, agent, nullptr, nullptr, reason};
    ++callCount;
}

struct FakeAgent {
    InspectorAgentBase base;
    int id;
};

void APS5_VABI FakeDestroyingDestructor(InspectorAgentBase*) {}

void APS5_VABI FakeDeletingDestructor(InspectorAgentBase* self) {
    auto* agent = reinterpret_cast<FakeAgent*>(self);
    Record(CallSlot::Delete, agent->id);
    _ZN3WTF8fastFreeEPv(agent);
}

void APS5_VABI FakeDidCreateFrontendAndBackend(InspectorAgentBase* self, FrontendRouter* router, BackendDispatcher* dispatcher) {
    auto* agent = reinterpret_cast<FakeAgent*>(self);
    Record(CallSlot::DidCreate, agent->id);
    calls[callCount - 1].router = router;
    calls[callCount - 1].dispatcher = dispatcher;
}

void APS5_VABI FakeWillDestroyFrontendAndBackend(InspectorAgentBase* self, DisconnectReason reason) {
    auto* agent = reinterpret_cast<FakeAgent*>(self);
    Record(CallSlot::WillDestroy, agent->id, reason);
}

void APS5_VABI FakeDiscardValues(InspectorAgentBase* self) {
    auto* agent = reinterpret_cast<FakeAgent*>(self);
    Record(CallSlot::DiscardValues, agent->id);
}

void APS5_VABI FakeDiscardAgent(InspectorAgentBase* self) {
    auto* agent = reinterpret_cast<FakeAgent*>(self);
    Record(CallSlot::DiscardAgent, agent->id);
}

void* const FakeVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&FakeDestroyingDestructor),
    reinterpret_cast<void*>(&FakeDeletingDestructor),
    reinterpret_cast<void*>(&FakeDidCreateFrontendAndBackend),
    reinterpret_cast<void*>(&FakeWillDestroyFrontendAndBackend),
    reinterpret_cast<void*>(&FakeDiscardValues),
    reinterpret_cast<void*>(&FakeDiscardAgent),
};

FakeAgent* MakeAgent(int id) {
    void* storage = _ZN3WTF10fastMallocEm(sizeof(FakeAgent));
    return new (storage) FakeAgent{{&FakeVtable[2], NKWebKit::WtfAbi::String()}, id};
}

AgentRegistry MakeRegistry(FakeAgent* const* agents, unsigned count) {
    auto** buffer = static_cast<InspectorAgentBase**>(_ZN3WTF10fastMallocEm(count * sizeof(InspectorAgentBase*)));
    for (unsigned i = 0; i < count; ++i) buffer[i] = &agents[i]->base;
    return AgentRegistry{buffer, count, count};
}

AgentRegistry MakeNullRegistry(FakeAgent* agent) {
    auto** buffer = static_cast<InspectorAgentBase**>(_ZN3WTF10fastMallocEm(2 * sizeof(InspectorAgentBase*)));
    buffer[0] = &agent->base;
    buffer[1] = nullptr;
    return AgentRegistry{buffer, 2, 2};
}

void TestConstructor() {
    alignas(8) unsigned char storage[sizeof(AgentRegistry)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* registry = reinterpret_cast<AgentRegistry*>(storage);
    _ZN9Inspector13AgentRegistryC2Ev(registry);
    Require(registry->buffer == nullptr);
    Require(registry->capacity == 0);
    Require(registry->size == 0);
    std::printf("Inspector::AgentRegistry constructor: null buffer, zero capacity and size passed\n");
}

void TestDispatch() {
    FakeAgent* agents[3] = {MakeAgent(1), MakeAgent(2), MakeAgent(3)};
    AgentRegistry registry = MakeRegistry(agents, 3);
    auto* router = reinterpret_cast<FrontendRouter*>(0x1234);
    auto* dispatcher = reinterpret_cast<BackendDispatcher*>(0x5678);
    callCount = 0;
    _ZN9Inspector13AgentRegistry27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE(&registry, router, dispatcher);
    Require(callCount == 3);
    for (int i = 0; i < 3; ++i) {
        Require(calls[i].slot == CallSlot::DidCreate);
        Require(calls[i].agent == i + 1);
        Require(calls[i].router == router);
        Require(calls[i].dispatcher == dispatcher);
    }
    callCount = 0;
    _ZN9Inspector13AgentRegistry27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE(&registry, nullptr, nullptr);
    Require(callCount == 3);
    for (int i = 0; i < 3; ++i) {
        Require(calls[i].slot == CallSlot::DidCreate);
        Require(calls[i].router == nullptr);
        Require(calls[i].dispatcher == nullptr);
    }
    callCount = 0;
    _ZN9Inspector13AgentRegistry29willDestroyFrontendAndBackendENS_16DisconnectReasonE(&registry, DisconnectReason::InspectorDestroyed);
    Require(callCount == 3);
    for (int i = 0; i < 3; ++i) {
        Require(calls[i].slot == CallSlot::WillDestroy);
        Require(calls[i].agent == i + 1);
        Require(calls[i].reason == DisconnectReason::InspectorDestroyed);
        Require(static_cast<int>(calls[i].reason) == 1);
    }
    callCount = 0;
    _ZN9Inspector13AgentRegistry13discardValuesEv(&registry);
    Require(callCount == 3);
    for (int i = 0; i < 3; ++i) {
        Require(calls[i].slot == CallSlot::DiscardValues);
        Require(calls[i].agent == i + 1);
    }
    callCount = 0;
    _ZN9Inspector13AgentRegistryD1Ev(&registry);
    Require(callCount == 6);
    Require(registry.buffer == nullptr);
    Require(registry.capacity == 0);
    std::printf("Inspector::AgentRegistry dispatch: order, pointers, reason and destruction passed\n");
}

void TestDestruction() {
    {
        FakeAgent* agents[3] = {MakeAgent(1), MakeAgent(2), MakeAgent(3)};
        AgentRegistry registry = MakeRegistry(agents, 3);
        callCount = 0;
        _ZN9Inspector13AgentRegistryD1Ev(&registry);
        Require(callCount == 6);
        Require(calls[0].slot == CallSlot::DiscardAgent && calls[0].agent == 1);
        Require(calls[1].slot == CallSlot::DiscardAgent && calls[1].agent == 2);
        Require(calls[2].slot == CallSlot::DiscardAgent && calls[2].agent == 3);
        Require(calls[3].slot == CallSlot::Delete && calls[3].agent == 1);
        Require(calls[4].slot == CallSlot::Delete && calls[4].agent == 2);
        Require(calls[5].slot == CallSlot::Delete && calls[5].agent == 3);
    }
    {
        FakeAgent* agents[3] = {MakeAgent(1), MakeAgent(2), MakeAgent(3)};
        AgentRegistry registry = MakeRegistry(agents, 3);
        callCount = 0;
        _ZN9Inspector13AgentRegistryD2Ev(&registry);
        Require(callCount == 6);
        Require(calls[0].slot == CallSlot::DiscardAgent && calls[0].agent == 1);
        Require(calls[3].slot == CallSlot::Delete && calls[3].agent == 1);
        Require(calls[5].slot == CallSlot::Delete && calls[5].agent == 3);
    }
    {
        AgentRegistry registry{nullptr, 0, 0};
        callCount = 0;
        _ZN9Inspector13AgentRegistryD1Ev(&registry);
        Require(callCount == 0);
        Require(registry.buffer == nullptr);
        Require(registry.capacity == 0);
    }
    std::printf("Inspector::AgentRegistry destructors: discardAgent then delete, and empty registry passed\n");
}

void TestNull() {
    RequireThrows([] { _ZN9Inspector13AgentRegistryC2Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector13AgentRegistryD1Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector13AgentRegistryD2Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector13AgentRegistry13discardValuesEv(nullptr); });
    RequireThrows([] { _ZN9Inspector13AgentRegistry27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE(nullptr, nullptr, nullptr); });
    RequireThrows([] { _ZN9Inspector13AgentRegistry29willDestroyFrontendAndBackendENS_16DisconnectReasonE(nullptr, DisconnectReason::InspectorDestroyed); });
    {
        FakeAgent* first = MakeAgent(1);
        AgentRegistry registry = MakeNullRegistry(first);
        callCount = 0;
        RequireThrows([&] { _ZN9Inspector13AgentRegistry13discardValuesEv(&registry); });
        Require(callCount == 0);
        registry.size = 1;
        _ZN9Inspector13AgentRegistryD2Ev(&registry);
    }
    {
        FakeAgent* first = MakeAgent(1);
        AgentRegistry registry = MakeNullRegistry(first);
        callCount = 0;
        RequireThrows([&] { _ZN9Inspector13AgentRegistryD1Ev(&registry); });
        Require(callCount == 0);
        registry.size = 1;
        _ZN9Inspector13AgentRegistryD2Ev(&registry);
    }
    std::printf("Inspector::AgentRegistry null self and null entry: throws at the dereference point passed\n");
}

}

int main() {
    TestConstructor();
    TestDispatch();
    TestDestruction();
    TestNull();
}
