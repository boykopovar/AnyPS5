#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::InspectorAbi {

struct AsyncCallIdentifier {
    std::uint32_t type;
    std::int32_t callbackId;
};

static_assert(sizeof(AsyncCallIdentifier) == 8);
static_assert(std::is_trivially_copyable_v<AsyncCallIdentifier>);

struct RemoteTarget {
    const void* vtable;
    std::uint32_t identifier;
    bool flag;
};

static_assert(offsetof(RemoteTarget, identifier) == 8);
static_assert(offsetof(RemoteTarget, flag) == 12);

struct OptionalString {
    bool engaged;
    void* impl;
};

static_assert(sizeof(OptionalString) == 16);

struct InjectedScriptHost {
    const void* vtable;
    std::uint32_t refCount;
    void* wrappers;
    OptionalString savedResultAlias;
};

static_assert(offsetof(InjectedScriptHost, refCount) == 8);
static_assert(offsetof(InjectedScriptHost, wrappers) == 16);
static_assert(offsetof(InjectedScriptHost, savedResultAlias) == 24);
static_assert(sizeof(InjectedScriptHost) == 40);

} // namespace Jsc::InspectorAbi

extern "C" {

Jsc::InspectorAbi::AsyncCallIdentifier APS5_VABI _ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi(std::uint32_t asyncCallType, std::int32_t callbackId);
bool APS5_VABI _ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv(const Jsc::InspectorAbi::RemoteTarget* self);
bool APS5_VABI _ZNK9Inspector22RemoteAutomationTarget20remoteControlAllowedEv(const Jsc::InspectorAbi::RemoteTarget* self);
const Jsc::InspectorAbi::OptionalString* APS5_VABI _ZNK9Inspector18InjectedScriptHost16savedResultAliasEv(const Jsc::InspectorAbi::InjectedScriptHost* self);

}
