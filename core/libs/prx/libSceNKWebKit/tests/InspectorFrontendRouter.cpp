#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace {

using NKWebKit::InspectorAbi::ConnectionType;
using NKWebKit::InspectorAbi::FrontendChannel;
using NKWebKit::InspectorAbi::FrontendRouter;
using NKWebKit::InspectorAbi::FrontendRouterRef;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Inspector::FrontendRouter check failed at line %u\n", location.line());
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

using CreateRouter = FrontendRouterRef (APS5_VABI*)();
using Constructor = void (APS5_VABI*)(FrontendRouter*);
using Destructor = void (APS5_VABI*)(FrontendRouter*);
using ConnectFrontend = void (APS5_VABI*)(FrontendRouter*, FrontendChannel*);
using DisconnectAllFrontends = void (APS5_VABI*)(FrontendRouter*);
using HasFrontends = bool (APS5_VABI*)(const FrontendRouter*);
using FrontendCount = std::uint32_t (APS5_VABI*)(const void*);
using SendMessage = void (APS5_VABI*)(const FrontendRouter*, const String*);
static_assert(std::is_same_v<decltype(&_ZN9Inspector14FrontendRouter6createEv), CreateRouter>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector14FrontendRouterC1Ev), Constructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector14FrontendRouterC2Ev), Constructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector14FrontendRouterD1Ev), Destructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector14FrontendRouterD2Ev), Destructor>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE), ConnectFrontend>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE), ConnectFrontend>);
static_assert(std::is_same_v<decltype(&_ZN9Inspector14FrontendRouter22disconnectAllFrontendsEv), DisconnectAllFrontends>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector14FrontendRouter12hasFrontendsEv), HasFrontends>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector14FrontendRouter13frontendCountEv), FrontendCount>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector14FrontendRouter16hasLocalFrontendEv), HasFrontends>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector14FrontendRouter17hasRemoteFrontendEv), HasFrontends>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector14FrontendRouter9sendEventERKN3WTF6StringE), SendMessage>);
static_assert(std::is_same_v<decltype(&_ZNK9Inspector14FrontendRouter12sendResponseERKN3WTF6StringE), SendMessage>);

enum class CallSlot { ConnectionType, SendMessage };

struct Call {
    CallSlot slot;
    int channel;
    ConnectionType type;
    const StringImpl* impl;
    const String* message;
};

constexpr int MaximumCalls = 256;
Call calls[MaximumCalls];
int callCount = 0;

void Record(CallSlot slot, int channel) {
    Require(callCount < MaximumCalls);
    calls[callCount] = Call{slot, channel, ConnectionType::Remote, nullptr, nullptr};
    ++callCount;
}

struct FakeChannel {
    FrontendChannel base;
    int id;
    ConnectionType type;
};

ConnectionType APS5_VABI FakeConnectionType(FrontendChannel* self) {
    auto* channel = reinterpret_cast<FakeChannel*>(self);
    Record(CallSlot::ConnectionType, channel->id);
    calls[callCount - 1].type = channel->type;
    return channel->type;
}

void APS5_VABI FakeSendMessage(FrontendChannel* self, const String* message) {
    auto* channel = reinterpret_cast<FakeChannel*>(self);
    Record(CallSlot::SendMessage, channel->id);
    calls[callCount - 1].impl = message ? message->impl : nullptr;
    calls[callCount - 1].message = message;
}

void APS5_VABI FakeDestroyingDestructor(FrontendChannel*) {}

void APS5_VABI FakeDeletingDestructor(FrontendChannel*) {}

void* const FakeVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&FakeDestroyingDestructor),
    reinterpret_cast<void*>(&FakeDeletingDestructor),
    reinterpret_cast<void*>(&FakeConnectionType),
    reinterpret_cast<void*>(&FakeSendMessage),
};

FakeChannel MakeChannel(int id, ConnectionType type) {
    return FakeChannel{{&FakeVtable[2]}, id, type};
}

FrontendRouterRef MakeRouter() {
    return _ZN9Inspector14FrontendRouter6createEv();
}

void TestCreate() {
    {
        FrontendRouterRef router = MakeRouter();
        Require(router.ptr != nullptr);
        Require(router.ptr->refCount == 1);
        Require(router.ptr->size == 0);
        Require(router.ptr->capacity == 2);
        Require(router.ptr->buffer == router.ptr->inlineBuffer);
    }
    {
        auto* router = static_cast<FrontendRouter*>(_ZN3WTF10fastMallocEm(sizeof(FrontendRouter)));
        _ZN9Inspector14FrontendRouterC1Ev(router);
        ++router->refCount;
        {
            FrontendRouterRef second(router);
            Require(router->refCount == 2);
        }
        Require(router->refCount == 1);
        Require(router->size == 0);
        FrontendRouterRef first(router);
    }
    std::printf("Inspector::FrontendRouter::create: allocation, reference count and release passed\n");
}

void TestConstructors() {
    alignas(8) unsigned char first[sizeof(FrontendRouter)];
    alignas(8) unsigned char second[sizeof(FrontendRouter)];
    std::memset(first, 0xCD, sizeof(first));
    std::memset(second, 0xCD, sizeof(second));
    auto* byC1 = reinterpret_cast<FrontendRouter*>(first);
    auto* byC2 = reinterpret_cast<FrontendRouter*>(second);
    _ZN9Inspector14FrontendRouterC1Ev(byC1);
    _ZN9Inspector14FrontendRouterC2Ev(byC2);
    FrontendRouter* routers[] = {byC1, byC2};
    for (auto* router : routers) {
        Require(router->refCount == 1);
        Require(router->size == 0);
        Require(router->capacity == 2);
        Require(router->buffer == router->inlineBuffer);
        Require(reinterpret_cast<const unsigned char*>(router->inlineBuffer) == reinterpret_cast<const unsigned char*>(router) + 24);
    }
    _ZN9Inspector14FrontendRouterD1Ev(byC1);
    _ZN9Inspector14FrontendRouterD2Ev(byC2);
    std::printf("Inspector::FrontendRouter constructors: C1 and C2 give the same 40-byte state\n");
}

void TestConnect() {
    FrontendRouterRef router = MakeRouter();
    FakeChannel a = MakeChannel(1, ConnectionType::Local);
    FakeChannel b = MakeChannel(2, ConnectionType::Remote);
    FakeChannel c = MakeChannel(3, ConnectionType::Local);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &a.base);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &b.base);
    Require(router.ptr->size == 2);
    Require(router.ptr->capacity == 2);
    Require(router.ptr->buffer == router.ptr->inlineBuffer);
    Require(router.ptr->buffer[0] == &a.base);
    Require(router.ptr->buffer[1] == &b.base);
    Require(_ZNK9Inspector14FrontendRouter13frontendCountEv(router.ptr) == 2);
    Require(_ZNK9Inspector14FrontendRouter12hasFrontendsEv(router.ptr));
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &a.base);
    Require(router.ptr->size == 2);
    Require(callCount == 0);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &c.base);
    Require(router.ptr->size == 3);
    Require(router.ptr->capacity == 16);
    Require(router.ptr->buffer != router.ptr->inlineBuffer);
    Require(router.ptr->buffer[0] == &a.base);
    Require(router.ptr->buffer[1] == &b.base);
    Require(router.ptr->buffer[2] == &c.base);
    std::printf("Inspector::FrontendRouter::connectFrontend: inline pairs, duplicate and first growth passed\n");
}

void TestGrowth() {
    FrontendRouterRef router = MakeRouter();
    FakeChannel channels[17] = {};
    for (int i = 0; i < 17; ++i) channels[i] = MakeChannel(i + 1, ConnectionType::Remote);
    for (int i = 0; i < 16; ++i) _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &channels[i].base);
    Require(router.ptr->capacity == 16);
    Require(router.ptr->size == 16);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &channels[16].base);
    Require(router.ptr->capacity == 21);
    Require(router.ptr->size == 17);
    for (unsigned i = 0; i < 17; ++i) Require(router.ptr->buffer[i] == &channels[i].base);
    std::printf("Inspector::FrontendRouter growth: 16 then 21 connections passed\n");
}

void TestDisconnect() {
    FrontendRouterRef router = MakeRouter();
    FakeChannel a = MakeChannel(1, ConnectionType::Local);
    FakeChannel b = MakeChannel(2, ConnectionType::Remote);
    FakeChannel c = MakeChannel(3, ConnectionType::Local);
    FakeChannel absent = MakeChannel(4, ConnectionType::Local);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &a.base);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &b.base);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &c.base);
    Require(router.ptr->capacity == 16);
    _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(router.ptr, &b.base);
    Require(router.ptr->size == 2);
    Require(router.ptr->capacity == 16);
    Require(router.ptr->buffer[0] == &a.base);
    Require(router.ptr->buffer[1] == &c.base);
    _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(router.ptr, &absent.base);
    Require(router.ptr->size == 2);
    Require(router.ptr->buffer[0] == &a.base);
    Require(router.ptr->buffer[1] == &c.base);
    _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(router.ptr, &a.base);
    Require(router.ptr->size == 1);
    Require(router.ptr->buffer[0] == &c.base);
    _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(router.ptr, &c.base);
    Require(router.ptr->size == 0);
    Require(router.ptr->capacity == 16);
    Require(!_ZNK9Inspector14FrontendRouter12hasFrontendsEv(router.ptr));
    std::printf("Inspector::FrontendRouter::disconnectFrontend: first match and absent channel passed\n");
}

void TestFrontendKinds() {
    callCount = 0;
    {
        FrontendRouterRef router = MakeRouter();
        Require(!_ZNK9Inspector14FrontendRouter16hasLocalFrontendEv(router.ptr));
        Require(!_ZNK9Inspector14FrontendRouter17hasRemoteFrontendEv(router.ptr));
        Require(callCount == 0);
    }
    {
        FrontendRouterRef router = MakeRouter();
        FakeChannel remote = MakeChannel(1, ConnectionType::Remote);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &remote.base);
        Require(!_ZNK9Inspector14FrontendRouter16hasLocalFrontendEv(router.ptr));
        Require(_ZNK9Inspector14FrontendRouter17hasRemoteFrontendEv(router.ptr));
    }
    {
        FrontendRouterRef router = MakeRouter();
        FakeChannel remote = MakeChannel(1, ConnectionType::Remote);
        FakeChannel local = MakeChannel(2, ConnectionType::Local);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &remote.base);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &local.base);
        Require(_ZNK9Inspector14FrontendRouter16hasLocalFrontendEv(router.ptr));
        Require(_ZNK9Inspector14FrontendRouter17hasRemoteFrontendEv(router.ptr));
    }
    {
        FrontendRouterRef router = MakeRouter();
        FakeChannel local = MakeChannel(1, ConnectionType::Local);
        FakeChannel remote = MakeChannel(2, ConnectionType::Remote);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &local.base);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &remote.base);
        callCount = 0;
        Require(_ZNK9Inspector14FrontendRouter16hasLocalFrontendEv(router.ptr));
        Require(callCount == 1);
        callCount = 0;
        Require(_ZNK9Inspector14FrontendRouter17hasRemoteFrontendEv(router.ptr));
        Require(callCount == 2);
    }
    std::printf("Inspector::FrontendRouter frontend kinds: table and early stop passed\n");
}

void TestSend() {
    FrontendRouterRef router = MakeRouter();
    FakeChannel a = MakeChannel(1, ConnectionType::Local);
    FakeChannel b = MakeChannel(2, ConnectionType::Remote);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &a.base);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &b.base);
    StringImpl text{4, 7, "message", 4};
    String message(&text);
    callCount = 0;
    _ZNK9Inspector14FrontendRouter9sendEventERKN3WTF6StringE(router.ptr, &message);
    Require(callCount == 2);
    Require(calls[0].slot == CallSlot::SendMessage && calls[0].channel == 1);
    Require(calls[1].slot == CallSlot::SendMessage && calls[1].channel == 2);
    Require(calls[0].impl == &text && calls[1].impl == &text);
    Require(calls[0].message == &message && calls[1].message == &message);
    Require(text.refCount == 4);
    callCount = 0;
    _ZNK9Inspector14FrontendRouter12sendResponseERKN3WTF6StringE(router.ptr, &message);
    Require(callCount == 2);
    Require(calls[0].slot == CallSlot::SendMessage && calls[0].channel == 1);
    Require(calls[1].slot == CallSlot::SendMessage && calls[1].channel == 2);
    Require(calls[0].impl == &text && calls[1].impl == &text);
    Require(text.refCount == 4);
    std::printf("Inspector::FrontendRouter event and response: order, string identity and reference count passed\n");
}

void TestDisconnectAll() {
    {
        FrontendRouterRef router = MakeRouter();
        FakeChannel a = MakeChannel(1, ConnectionType::Local);
        FakeChannel b = MakeChannel(2, ConnectionType::Remote);
        FakeChannel c = MakeChannel(3, ConnectionType::Local);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &a.base);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &b.base);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &c.base);
        Require(router.ptr->capacity == 16);
        _ZN9Inspector14FrontendRouter22disconnectAllFrontendsEv(router.ptr);
        Require(router.ptr->size == 0);
        Require(router.ptr->capacity == 2);
        Require(router.ptr->buffer == router.ptr->inlineBuffer);
        Require(!_ZNK9Inspector14FrontendRouter12hasFrontendsEv(router.ptr));
    }
    {
        FrontendRouterRef router = MakeRouter();
        FakeChannel a = MakeChannel(1, ConnectionType::Local);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &a.base);
        _ZN9Inspector14FrontendRouter22disconnectAllFrontendsEv(router.ptr);
        Require(router.ptr->size == 0);
        Require(router.ptr->capacity == 2);
        Require(router.ptr->buffer == router.ptr->inlineBuffer);
    }
    std::printf("Inspector::FrontendRouter::disconnectAllFrontends: heap and inline states passed\n");
}

void TestDestruction() {
    {
        auto* router = static_cast<FrontendRouter*>(_ZN3WTF10fastMallocEm(sizeof(FrontendRouter)));
        _ZN9Inspector14FrontendRouterC1Ev(router);
        FakeChannel a = MakeChannel(1, ConnectionType::Local);
        FakeChannel b = MakeChannel(2, ConnectionType::Remote);
        FakeChannel c = MakeChannel(3, ConnectionType::Local);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router, &a.base);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router, &b.base);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router, &c.base);
        Require(router->capacity == 16);
        _ZN9Inspector14FrontendRouterD1Ev(router);
        _ZN3WTF8fastFreeEPv(router);
    }
    {
        alignas(8) unsigned char storage[sizeof(FrontendRouter)];
        std::memset(storage, 0xCD, sizeof(storage));
        auto* router = reinterpret_cast<FrontendRouter*>(storage);
        _ZN9Inspector14FrontendRouterC2Ev(router);
        FakeChannel a = MakeChannel(1, ConnectionType::Local);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router, &a.base);
        _ZN9Inspector14FrontendRouterD2Ev(router);
    }
    std::printf("Inspector::FrontendRouter destructors: heap buffer and inline buffer passed\n");
}

void TestNull() {
    FakeChannel channel = MakeChannel(1, ConnectionType::Local);
    StringImpl text{4, 4, "null", 4};
    String message(&text);
    RequireThrows([] { _ZN9Inspector14FrontendRouterC1Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector14FrontendRouterC2Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector14FrontendRouterD1Ev(nullptr); });
    RequireThrows([] { _ZN9Inspector14FrontendRouterD2Ev(nullptr); });
    RequireThrows([&] { _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(nullptr, &channel.base); });
    RequireThrows([&] { _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(nullptr, &channel.base); });
    RequireThrows([] { _ZN9Inspector14FrontendRouter22disconnectAllFrontendsEv(nullptr); });
    RequireThrows([] { _ZNK9Inspector14FrontendRouter12hasFrontendsEv(nullptr); });
    RequireThrows([] { _ZNK9Inspector14FrontendRouter16hasLocalFrontendEv(nullptr); });
    RequireThrows([] { _ZNK9Inspector14FrontendRouter17hasRemoteFrontendEv(nullptr); });
    RequireThrows([&] { _ZNK9Inspector14FrontendRouter9sendEventERKN3WTF6StringE(nullptr, &message); });
    RequireThrows([&] { _ZNK9Inspector14FrontendRouter12sendResponseERKN3WTF6StringE(nullptr, &message); });
    FrontendRouterRef router = MakeRouter();
    RequireThrows([&] { _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, nullptr); });
    RequireThrows([&] { _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(router.ptr, nullptr); });
    FakeChannel remote = MakeChannel(2, ConnectionType::Remote);
    FakeChannel local = MakeChannel(3, ConnectionType::Local);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &remote.base);
    _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router.ptr, &local.base);
    router.ptr->buffer[1] = nullptr;
    callCount = 0;
    RequireThrows([&] { _ZNK9Inspector14FrontendRouter9sendEventERKN3WTF6StringE(router.ptr, &message); });
    Require(callCount == 0);
    callCount = 0;
    RequireThrows([&] { _ZNK9Inspector14FrontendRouter16hasLocalFrontendEv(router.ptr); });
    Require(callCount == 0);
    std::printf("Inspector::FrontendRouter null arguments and entries: throws at the dereference point passed\n");
}

}

int main() {
    TestCreate();
    TestConstructors();
    TestConnect();
    TestGrowth();
    TestDisconnect();
    TestFrontendKinds();
    TestSend();
    TestDisconnectAll();
    TestDestruction();
    TestNull();
}
