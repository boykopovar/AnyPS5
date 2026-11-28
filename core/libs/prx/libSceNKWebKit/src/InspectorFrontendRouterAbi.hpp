#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace NKWebKit::InspectorAbi {

struct BackendDispatcher;

enum class ConnectionType : int {
    Remote,
    Local
};

enum class DisconnectReason : int {
    InspectedTargetDestroyed,
    InspectorDestroyed
};

struct FrontendChannel {
    void* const* vtable;
};

struct InspectorAgentBase {
    void* const* vtable;
    WtfAbi::String name;
};

struct FrontendRouter {
    unsigned refCount;
    FrontendChannel** buffer;
    unsigned capacity;
    unsigned size;
    FrontendChannel* inlineBuffer[2];
};

struct AgentRegistry {
    InspectorAgentBase** buffer;
    unsigned capacity;
    unsigned size;
};

struct FrontendRouterRef {
    FrontendRouter* ptr;
    explicit FrontendRouterRef(FrontendRouter* value) : ptr(value) {}
    FrontendRouterRef(FrontendRouterRef&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
    FrontendRouterRef(const FrontendRouterRef&) = delete;
    FrontendRouterRef& operator=(const FrontendRouterRef&) = delete;
    ~FrontendRouterRef();
};

static_assert(sizeof(FrontendChannel) == 8);
static_assert(offsetof(FrontendChannel, vtable) == 0);
static_assert(sizeof(InspectorAgentBase) == 16);
static_assert(offsetof(InspectorAgentBase, vtable) == 0);
static_assert(offsetof(InspectorAgentBase, name) == 8);
static_assert(sizeof(FrontendRouter) == 40);
static_assert(offsetof(FrontendRouter, refCount) == 0);
static_assert(offsetof(FrontendRouter, buffer) == 8);
static_assert(offsetof(FrontendRouter, capacity) == 16);
static_assert(offsetof(FrontendRouter, size) == 20);
static_assert(offsetof(FrontendRouter, inlineBuffer) == 24);
static_assert(sizeof(AgentRegistry) == 16);
static_assert(offsetof(AgentRegistry, buffer) == 0);
static_assert(offsetof(AgentRegistry, capacity) == 8);
static_assert(offsetof(AgentRegistry, size) == 12);
static_assert(sizeof(FrontendRouterRef) == 8);
static_assert(!std::is_trivially_destructible_v<FrontendRouterRef>);

}
