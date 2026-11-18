#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

namespace NKWebKit::InspectorAbi {

struct BackendDispatcherHandler {
    const void* const* vptr;
};

static_assert(sizeof(BackendDispatcherHandler) == 8);

struct BackendDispatcherHandlerLayout {
    const char* domain;
    std::size_t commandCount;
    const void* const* vtable;
};

const BackendDispatcherHandlerLayout* FindBackendDispatcherHandlerLayout(const char* domain);
void APS5_VABI BackendDispatcherHandlerCompleteDestructor(BackendDispatcherHandler* self);
void APS5_VABI BackendDispatcherHandlerDeletingDestructor(BackendDispatcherHandler* self);

}  // namespace NKWebKit::InspectorAbi

extern "C" {

void APS5_VABI _ZN9Inspector33AnimationBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector33AnimationBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector33AnimationBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector40ApplicationCacheBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector40ApplicationCacheBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector40ApplicationCacheBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector29AuditBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector29AuditBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector29AuditBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector31BrowserBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector31BrowserBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector31BrowserBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector27CSSBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector27CSSBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector27CSSBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector30CanvasBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector30CanvasBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector30CanvasBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector31ConsoleBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector31ConsoleBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector31ConsoleBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector27DOMBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector27DOMBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector27DOMBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector35DOMDebuggerBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector35DOMDebuggerBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector35DOMDebuggerBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector34DOMStorageBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector34DOMStorageBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector34DOMStorageBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector32DatabaseBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector32DatabaseBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector32DatabaseBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector32DebuggerBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector32DebuggerBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector32DebuggerBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector28HeapBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector28HeapBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector28HeapBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector33InspectorBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector33InspectorBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector33InspectorBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector33LayerTreeBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector33LayerTreeBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector33LayerTreeBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector31NetworkBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector31NetworkBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector31NetworkBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector28PageBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector28PageBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector28PageBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector31RuntimeBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector31RuntimeBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector31RuntimeBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector38ScriptProfilerBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector38ScriptProfilerBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector38ScriptProfilerBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector30TargetBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector30TargetBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector30TargetBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector32TimelineBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector30WorkerBackendDispatcherHandlerC2Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);
void APS5_VABI _ZN9Inspector30WorkerBackendDispatcherHandlerC2ERKS0_(NKWebKit::InspectorAbi::BackendDispatcherHandler* self, const NKWebKit::InspectorAbi::BackendDispatcherHandler* other);
void APS5_VABI _ZN9Inspector30WorkerBackendDispatcherHandlerD0Ev(NKWebKit::InspectorAbi::BackendDispatcherHandler* self);

}
