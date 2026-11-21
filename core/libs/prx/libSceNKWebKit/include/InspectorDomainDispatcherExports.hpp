#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/InspectorBackendDispatcherExports.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace NKWebKit::InspectorAbi {

struct DomainBackendDispatcher {
    const SupplementalBackendDispatcherVtable* vptr;
    unsigned refCount;
    std::uint8_t padding[4];
    BackendDispatcher* backendDispatcher;
    void* agent;
};

struct DomainBackendDispatcherRef {
    DomainBackendDispatcher* ptr;
    explicit DomainBackendDispatcherRef(DomainBackendDispatcher* value) : ptr(value) {}
    DomainBackendDispatcherRef(DomainBackendDispatcherRef&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
    DomainBackendDispatcherRef(const DomainBackendDispatcherRef&) = delete;
    DomainBackendDispatcherRef& operator=(const DomainBackendDispatcherRef&) = delete;
    ~DomainBackendDispatcherRef();
};

static_assert(std::is_standard_layout_v<DomainBackendDispatcher>);
static_assert(sizeof(DomainBackendDispatcher) == 32);
static_assert(offsetof(DomainBackendDispatcher, refCount) == offsetof(SupplementalBackendDispatcher, refCount));
static_assert(offsetof(DomainBackendDispatcher, backendDispatcher) == offsetof(SupplementalBackendDispatcher, backendDispatcher));
static_assert(offsetof(DomainBackendDispatcher, agent) == 24);
static_assert(sizeof(DomainBackendDispatcherRef) == 8);
static_assert(!std::is_trivially_destructible_v<DomainBackendDispatcherRef>);

void DerefDomainBackendDispatcher(DomainBackendDispatcher* dispatcher);

}  // namespace NKWebKit::InspectorAbi

extern "C" {

NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector26AnimationBackendDispatcher6createERNS_17BackendDispatcherEPNS_33AnimationBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector26AnimationBackendDispatcherC1ERNS_17BackendDispatcherEPNS_33AnimationBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector26AnimationBackendDispatcherC2ERNS_17BackendDispatcherEPNS_33AnimationBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector26AnimationBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector33ApplicationCacheBackendDispatcher6createERNS_17BackendDispatcherEPNS_40ApplicationCacheBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector33ApplicationCacheBackendDispatcherC1ERNS_17BackendDispatcherEPNS_40ApplicationCacheBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector33ApplicationCacheBackendDispatcherC2ERNS_17BackendDispatcherEPNS_40ApplicationCacheBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector33ApplicationCacheBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector22AuditBackendDispatcher6createERNS_17BackendDispatcherEPNS_29AuditBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector22AuditBackendDispatcherC1ERNS_17BackendDispatcherEPNS_29AuditBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector22AuditBackendDispatcherC2ERNS_17BackendDispatcherEPNS_29AuditBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector22AuditBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector24BrowserBackendDispatcher6createERNS_17BackendDispatcherEPNS_31BrowserBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24BrowserBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31BrowserBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24BrowserBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31BrowserBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24BrowserBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector20CSSBackendDispatcher6createERNS_17BackendDispatcherEPNS_27CSSBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector20CSSBackendDispatcherC1ERNS_17BackendDispatcherEPNS_27CSSBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector20CSSBackendDispatcherC2ERNS_17BackendDispatcherEPNS_27CSSBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector20CSSBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector23CanvasBackendDispatcher6createERNS_17BackendDispatcherEPNS_30CanvasBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector23CanvasBackendDispatcherC1ERNS_17BackendDispatcherEPNS_30CanvasBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector23CanvasBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector24ConsoleBackendDispatcher6createERNS_17BackendDispatcherEPNS_31ConsoleBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24ConsoleBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31ConsoleBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24ConsoleBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31ConsoleBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector20DOMBackendDispatcher6createERNS_17BackendDispatcherEPNS_27DOMBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector20DOMBackendDispatcherC1ERNS_17BackendDispatcherEPNS_27DOMBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector20DOMBackendDispatcherC2ERNS_17BackendDispatcherEPNS_27DOMBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector20DOMBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector28DOMDebuggerBackendDispatcher6createERNS_17BackendDispatcherEPNS_35DOMDebuggerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector28DOMDebuggerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_35DOMDebuggerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector28DOMDebuggerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_35DOMDebuggerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector27DOMStorageBackendDispatcher6createERNS_17BackendDispatcherEPNS_34DOMStorageBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector27DOMStorageBackendDispatcherC1ERNS_17BackendDispatcherEPNS_34DOMStorageBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector27DOMStorageBackendDispatcherC2ERNS_17BackendDispatcherEPNS_34DOMStorageBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector27DOMStorageBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector25DatabaseBackendDispatcher6createERNS_17BackendDispatcherEPNS_32DatabaseBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector25DatabaseBackendDispatcherC1ERNS_17BackendDispatcherEPNS_32DatabaseBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector25DatabaseBackendDispatcherC2ERNS_17BackendDispatcherEPNS_32DatabaseBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector25DatabaseBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector25DebuggerBackendDispatcher6createERNS_17BackendDispatcherEPNS_32DebuggerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector25DebuggerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_32DebuggerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector25DebuggerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_32DebuggerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector25DebuggerBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
void APS5_VABI _ZN9Inspector21HeapBackendDispatcherC1ERNS_17BackendDispatcherEPNS_28HeapBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector21HeapBackendDispatcherC2ERNS_17BackendDispatcherEPNS_28HeapBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector26InspectorBackendDispatcher6createERNS_17BackendDispatcherEPNS_33InspectorBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector26InspectorBackendDispatcherC1ERNS_17BackendDispatcherEPNS_33InspectorBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector26InspectorBackendDispatcherC2ERNS_17BackendDispatcherEPNS_33InspectorBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector26InspectorBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector26LayerTreeBackendDispatcher6createERNS_17BackendDispatcherEPNS_33LayerTreeBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector26LayerTreeBackendDispatcherC1ERNS_17BackendDispatcherEPNS_33LayerTreeBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector26LayerTreeBackendDispatcherC2ERNS_17BackendDispatcherEPNS_33LayerTreeBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector26LayerTreeBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector24NetworkBackendDispatcher6createERNS_17BackendDispatcherEPNS_31NetworkBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24NetworkBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31NetworkBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24NetworkBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31NetworkBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24NetworkBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector21PageBackendDispatcher6createERNS_17BackendDispatcherEPNS_28PageBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector21PageBackendDispatcherC1ERNS_17BackendDispatcherEPNS_28PageBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector21PageBackendDispatcherC2ERNS_17BackendDispatcherEPNS_28PageBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector24RuntimeBackendDispatcher6createERNS_17BackendDispatcherEPNS_31RuntimeBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24RuntimeBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31RuntimeBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24RuntimeBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31RuntimeBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector24RuntimeBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector31ScriptProfilerBackendDispatcher6createERNS_17BackendDispatcherEPNS_38ScriptProfilerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector31ScriptProfilerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_38ScriptProfilerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector31ScriptProfilerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_38ScriptProfilerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector31ScriptProfilerBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector23TargetBackendDispatcher6createERNS_17BackendDispatcherEPNS_30TargetBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector23TargetBackendDispatcherC1ERNS_17BackendDispatcherEPNS_30TargetBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector23TargetBackendDispatcherC2ERNS_17BackendDispatcherEPNS_30TargetBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector23TargetBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector25TimelineBackendDispatcher6createERNS_17BackendDispatcherEPNS_32TimelineBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector25TimelineBackendDispatcherC1ERNS_17BackendDispatcherEPNS_32TimelineBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector25TimelineBackendDispatcherC2ERNS_17BackendDispatcherEPNS_32TimelineBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector25TimelineBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);
NKWebKit::InspectorAbi::DomainBackendDispatcherRef APS5_VABI _ZN9Inspector23WorkerBackendDispatcher6createERNS_17BackendDispatcherEPNS_30WorkerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector23WorkerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_30WorkerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector23WorkerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_30WorkerBackendDispatcherHandlerE(NKWebKit::InspectorAbi::DomainBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher, void* agent);
void APS5_VABI _ZN9Inspector23WorkerBackendDispatcherD0Ev(NKWebKit::InspectorAbi::DomainBackendDispatcher* self);

}
