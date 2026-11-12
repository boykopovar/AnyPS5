// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/InspectorAgentRegistry.cpp
/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/InspectorAgentRegistry.h
/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/InspectorAgentBase.h
/*
 * Copyright (C) 2013, 2015 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/InspectorFrontendRouterExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::InspectorAbi::AgentRegistry;
using NKWebKit::InspectorAbi::BackendDispatcher;
using NKWebKit::InspectorAbi::DisconnectReason;
using NKWebKit::InspectorAbi::FrontendRouter;
using NKWebKit::InspectorAbi::InspectorAgentBase;

AgentRegistry& Checked(AgentRegistry* self) {
    if (!self) throw std::invalid_argument("null Inspector::AgentRegistry");
    return *self;
}

InspectorAgentBase& CheckedAgent(InspectorAgentBase* agent) {
    if (!agent) throw std::invalid_argument("null Inspector::InspectorAgentBase");
    return *agent;
}

void CheckAgents(const AgentRegistry& registry) {
    for (unsigned i = 0; i < registry.size; ++i) CheckedAgent(registry.buffer[i]);
}

void DidCreateFrontendAndBackend(InspectorAgentBase& agent, FrontendRouter* frontendRouter, BackendDispatcher* backendDispatcher) {
    using Function = void (APS5_VABI*)(InspectorAgentBase*, FrontendRouter*, BackendDispatcher*);
    reinterpret_cast<Function>(const_cast<void*>(agent.vtable[2]))(&agent, frontendRouter, backendDispatcher);
}

void WillDestroyFrontendAndBackend(InspectorAgentBase& agent, DisconnectReason reason) {
    using Function = void (APS5_VABI*)(InspectorAgentBase*, DisconnectReason);
    reinterpret_cast<Function>(const_cast<void*>(agent.vtable[3]))(&agent, reason);
}

void DiscardValues(InspectorAgentBase& agent) {
    using Function = void (APS5_VABI*)(InspectorAgentBase*);
    reinterpret_cast<Function>(const_cast<void*>(agent.vtable[4]))(&agent);
}

void DiscardAgent(InspectorAgentBase& agent) {
    using Function = void (APS5_VABI*)(InspectorAgentBase*);
    reinterpret_cast<Function>(const_cast<void*>(agent.vtable[5]))(&agent);
}

void DeleteAgent(InspectorAgentBase& agent) {
    using Function = void (APS5_VABI*)(InspectorAgentBase*);
    reinterpret_cast<Function>(const_cast<void*>(agent.vtable[1]))(&agent);
}

void Destroy(AgentRegistry& registry) {
    CheckAgents(registry);
    InspectorAgentBase** buffer = registry.buffer;
    const unsigned count = registry.size;
    for (unsigned i = 0; i < count; ++i) DiscardAgent(*buffer[i]);
    for (unsigned i = 0; i < count; ++i) DeleteAgent(*buffer[i]);
    if (buffer) _ZN3WTF8fastFreeEPv(buffer);
    registry.buffer = nullptr;
    registry.capacity = 0;
}

}

extern "C" {

void APS5_VABI _ZN9Inspector13AgentRegistryC2Ev(AgentRegistry* self) {
    AgentRegistry& registry = Checked(self);
    registry.buffer = nullptr;
    registry.capacity = 0;
    registry.size = 0;
}

void APS5_VABI _ZN9Inspector13AgentRegistryD1Ev(AgentRegistry* self) {
    Destroy(Checked(self));
}

void APS5_VABI _ZN9Inspector13AgentRegistryD2Ev(AgentRegistry* self) {
    Destroy(Checked(self));
}

void APS5_VABI _ZN9Inspector13AgentRegistry13discardValuesEv(AgentRegistry* self) {
    AgentRegistry& registry = Checked(self);
    CheckAgents(registry);
    InspectorAgentBase** buffer = registry.buffer;
    const unsigned count = registry.size;
    for (unsigned i = 0; i < count; ++i)
        DiscardValues(CheckedAgent(buffer[i]));
}

void APS5_VABI _ZN9Inspector13AgentRegistry27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE(AgentRegistry* self, FrontendRouter* frontendRouter, BackendDispatcher* backendDispatcher) {
    AgentRegistry& registry = Checked(self);
    CheckAgents(registry);
    InspectorAgentBase** buffer = registry.buffer;
    const unsigned count = registry.size;
    for (unsigned i = 0; i < count; ++i)
        DidCreateFrontendAndBackend(CheckedAgent(buffer[i]), frontendRouter, backendDispatcher);
}

void APS5_VABI _ZN9Inspector13AgentRegistry29willDestroyFrontendAndBackendENS_16DisconnectReasonE(AgentRegistry* self, DisconnectReason reason) {
    AgentRegistry& registry = Checked(self);
    CheckAgents(registry);
    InspectorAgentBase** buffer = registry.buffer;
    const unsigned count = registry.size;
    for (unsigned i = 0; i < count; ++i)
        WillDestroyFrontendAndBackend(CheckedAgent(buffer[i]), reason);
}

}
