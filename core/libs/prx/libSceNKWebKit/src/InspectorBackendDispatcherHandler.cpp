#include "prx/libSceNKWebKit/include/InspectorBackendDispatcherHandlerExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::InspectorAbi::BackendDispatcherHandler;
using NKWebKit::InspectorAbi::BackendDispatcherHandlerLayout;

template<std::size_t CommandCount> struct HandlerVtable {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    std::array<const void*, CommandCount + 2> slots;
};

template<std::size_t CommandCount, std::size_t... Index> HandlerVtable<CommandCount> MakeVtable(std::index_sequence<Index...>) {
    return {0, nullptr, {{(static_cast<void>(Index), reinterpret_cast<const void*>(&__cxa_pure_virtual_nid_postfix))..., reinterpret_cast<const void*>(&NKWebKit::InspectorAbi::BackendDispatcherHandlerCompleteDestructor), reinterpret_cast<const void*>(&NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor)}}};
}

template<std::size_t CommandCount> HandlerVtable<CommandCount> MakeVtable() {
    return MakeVtable<CommandCount>(std::make_index_sequence<CommandCount>());
}

const HandlerVtable<6> AnimationHandlerVtable = MakeVtable<6>();
const HandlerVtable<5> ApplicationCacheHandlerVtable = MakeVtable<5>();
const HandlerVtable<3> AuditHandlerVtable = MakeVtable<3>();
const HandlerVtable<2> BrowserHandlerVtable = MakeVtable<2>();
const HandlerVtable<19> CSSHandlerVtable = MakeVtable<19>();
const HandlerVtable<9> CanvasHandlerVtable = MakeVtable<9>();
const HandlerVtable<5> ConsoleHandlerVtable = MakeVtable<5>();
const HandlerVtable<45> DOMHandlerVtable = MakeVtable<45>();
const HandlerVtable<6> DOMDebuggerHandlerVtable = MakeVtable<6>();
const HandlerVtable<6> DOMStorageHandlerVtable = MakeVtable<6>();
const HandlerVtable<4> DatabaseHandlerVtable = MakeVtable<4>();
const HandlerVtable<29> DebuggerHandlerVtable = MakeVtable<29>();
const HandlerVtable<8> HeapHandlerVtable = MakeVtable<8>();
const HandlerVtable<3> InspectorHandlerVtable = MakeVtable<3>();
const HandlerVtable<4> LayerTreeHandlerVtable = MakeVtable<4>();
const HandlerVtable<16> NetworkHandlerVtable = MakeVtable<16>();
const HandlerVtable<21> PageHandlerVtable = MakeVtable<21>();
const HandlerVtable<20> RuntimeHandlerVtable = MakeVtable<20>();
const HandlerVtable<2> ScriptProfilerHandlerVtable = MakeVtable<2>();
const HandlerVtable<3> TargetHandlerVtable = MakeVtable<3>();
const HandlerVtable<6> TimelineHandlerVtable = MakeVtable<6>();
const HandlerVtable<4> WorkerHandlerVtable = MakeVtable<4>();

const BackendDispatcherHandlerLayout Layouts[] = {
    {"Animation", 6, AnimationHandlerVtable.slots.data()},
    {"ApplicationCache", 5, ApplicationCacheHandlerVtable.slots.data()},
    {"Audit", 3, AuditHandlerVtable.slots.data()},
    {"Browser", 2, BrowserHandlerVtable.slots.data()},
    {"CSS", 19, CSSHandlerVtable.slots.data()},
    {"Canvas", 9, CanvasHandlerVtable.slots.data()},
    {"Console", 5, ConsoleHandlerVtable.slots.data()},
    {"DOM", 45, DOMHandlerVtable.slots.data()},
    {"DOMDebugger", 6, DOMDebuggerHandlerVtable.slots.data()},
    {"DOMStorage", 6, DOMStorageHandlerVtable.slots.data()},
    {"Database", 4, DatabaseHandlerVtable.slots.data()},
    {"Debugger", 29, DebuggerHandlerVtable.slots.data()},
    {"Heap", 8, HeapHandlerVtable.slots.data()},
    {"Inspector", 3, InspectorHandlerVtable.slots.data()},
    {"LayerTree", 4, LayerTreeHandlerVtable.slots.data()},
    {"Network", 16, NetworkHandlerVtable.slots.data()},
    {"Page", 21, PageHandlerVtable.slots.data()},
    {"Runtime", 20, RuntimeHandlerVtable.slots.data()},
    {"ScriptProfiler", 2, ScriptProfilerHandlerVtable.slots.data()},
    {"Target", 3, TargetHandlerVtable.slots.data()},
    {"Timeline", 6, TimelineHandlerVtable.slots.data()},
    {"Worker", 4, WorkerHandlerVtable.slots.data()},
};

void Construct(BackendDispatcherHandler* self, const void* const* vtable, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + " requires an object");
    self->vptr = vtable;
}

void Copy(BackendDispatcherHandler* self, const BackendDispatcherHandler* other, const void* const* vtable, const char* function) {
    if (!other) throw std::invalid_argument(std::string(function) + " requires a source object");
    Construct(self, vtable, function);
}

}  // namespace

namespace NKWebKit::InspectorAbi {

const BackendDispatcherHandlerLayout* FindBackendDispatcherHandlerLayout(const char* domain) {
    for (const auto& layout : Layouts) {
        if (domain && !std::strcmp(layout.domain, domain)) return &layout;
    }
    return nullptr;
}

void APS5_VABI BackendDispatcherHandlerCompleteDestructor(BackendDispatcherHandler* self) {
    if (!self) throw std::invalid_argument("Inspector::BackendDispatcherHandler destructor requires an object");
}

void APS5_VABI BackendDispatcherHandlerDeletingDestructor(BackendDispatcherHandler* self) {
    BackendDispatcherHandlerCompleteDestructor(self);
    _ZdlPv_nid_postfix(self);
}

}  // namespace NKWebKit::InspectorAbi

extern "C" {

void APS5_VABI _ZN9Inspector33AnimationBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, AnimationHandlerVtable.slots.data(), "Inspector::AnimationBackendDispatcherHandler::AnimationBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector33AnimationBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, AnimationHandlerVtable.slots.data(), "Inspector::AnimationBackendDispatcherHandler::AnimationBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector33AnimationBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector40ApplicationCacheBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, ApplicationCacheHandlerVtable.slots.data(), "Inspector::ApplicationCacheBackendDispatcherHandler::ApplicationCacheBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector40ApplicationCacheBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, ApplicationCacheHandlerVtable.slots.data(), "Inspector::ApplicationCacheBackendDispatcherHandler::ApplicationCacheBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector40ApplicationCacheBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector29AuditBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, AuditHandlerVtable.slots.data(), "Inspector::AuditBackendDispatcherHandler::AuditBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector29AuditBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, AuditHandlerVtable.slots.data(), "Inspector::AuditBackendDispatcherHandler::AuditBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector29AuditBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector31BrowserBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, BrowserHandlerVtable.slots.data(), "Inspector::BrowserBackendDispatcherHandler::BrowserBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector31BrowserBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, BrowserHandlerVtable.slots.data(), "Inspector::BrowserBackendDispatcherHandler::BrowserBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector31BrowserBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector27CSSBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, CSSHandlerVtable.slots.data(), "Inspector::CSSBackendDispatcherHandler::CSSBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector27CSSBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, CSSHandlerVtable.slots.data(), "Inspector::CSSBackendDispatcherHandler::CSSBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector27CSSBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector30CanvasBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, CanvasHandlerVtable.slots.data(), "Inspector::CanvasBackendDispatcherHandler::CanvasBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector30CanvasBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, CanvasHandlerVtable.slots.data(), "Inspector::CanvasBackendDispatcherHandler::CanvasBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector30CanvasBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector31ConsoleBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, ConsoleHandlerVtable.slots.data(), "Inspector::ConsoleBackendDispatcherHandler::ConsoleBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector31ConsoleBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, ConsoleHandlerVtable.slots.data(), "Inspector::ConsoleBackendDispatcherHandler::ConsoleBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector31ConsoleBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector27DOMBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, DOMHandlerVtable.slots.data(), "Inspector::DOMBackendDispatcherHandler::DOMBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector27DOMBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, DOMHandlerVtable.slots.data(), "Inspector::DOMBackendDispatcherHandler::DOMBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector27DOMBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector35DOMDebuggerBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, DOMDebuggerHandlerVtable.slots.data(), "Inspector::DOMDebuggerBackendDispatcherHandler::DOMDebuggerBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector35DOMDebuggerBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, DOMDebuggerHandlerVtable.slots.data(), "Inspector::DOMDebuggerBackendDispatcherHandler::DOMDebuggerBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector35DOMDebuggerBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector34DOMStorageBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, DOMStorageHandlerVtable.slots.data(), "Inspector::DOMStorageBackendDispatcherHandler::DOMStorageBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector34DOMStorageBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, DOMStorageHandlerVtable.slots.data(), "Inspector::DOMStorageBackendDispatcherHandler::DOMStorageBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector34DOMStorageBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector32DatabaseBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, DatabaseHandlerVtable.slots.data(), "Inspector::DatabaseBackendDispatcherHandler::DatabaseBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector32DatabaseBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, DatabaseHandlerVtable.slots.data(), "Inspector::DatabaseBackendDispatcherHandler::DatabaseBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector32DatabaseBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector32DebuggerBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, DebuggerHandlerVtable.slots.data(), "Inspector::DebuggerBackendDispatcherHandler::DebuggerBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector32DebuggerBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, DebuggerHandlerVtable.slots.data(), "Inspector::DebuggerBackendDispatcherHandler::DebuggerBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector32DebuggerBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector28HeapBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, HeapHandlerVtable.slots.data(), "Inspector::HeapBackendDispatcherHandler::HeapBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector28HeapBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, HeapHandlerVtable.slots.data(), "Inspector::HeapBackendDispatcherHandler::HeapBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector28HeapBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector33InspectorBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, InspectorHandlerVtable.slots.data(), "Inspector::InspectorBackendDispatcherHandler::InspectorBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector33InspectorBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, InspectorHandlerVtable.slots.data(), "Inspector::InspectorBackendDispatcherHandler::InspectorBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector33InspectorBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector33LayerTreeBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, LayerTreeHandlerVtable.slots.data(), "Inspector::LayerTreeBackendDispatcherHandler::LayerTreeBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector33LayerTreeBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, LayerTreeHandlerVtable.slots.data(), "Inspector::LayerTreeBackendDispatcherHandler::LayerTreeBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector33LayerTreeBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector31NetworkBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, NetworkHandlerVtable.slots.data(), "Inspector::NetworkBackendDispatcherHandler::NetworkBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector31NetworkBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, NetworkHandlerVtable.slots.data(), "Inspector::NetworkBackendDispatcherHandler::NetworkBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector31NetworkBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector28PageBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, PageHandlerVtable.slots.data(), "Inspector::PageBackendDispatcherHandler::PageBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector28PageBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, PageHandlerVtable.slots.data(), "Inspector::PageBackendDispatcherHandler::PageBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector28PageBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector31RuntimeBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, RuntimeHandlerVtable.slots.data(), "Inspector::RuntimeBackendDispatcherHandler::RuntimeBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector31RuntimeBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, RuntimeHandlerVtable.slots.data(), "Inspector::RuntimeBackendDispatcherHandler::RuntimeBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector31RuntimeBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector38ScriptProfilerBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, ScriptProfilerHandlerVtable.slots.data(), "Inspector::ScriptProfilerBackendDispatcherHandler::ScriptProfilerBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector38ScriptProfilerBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, ScriptProfilerHandlerVtable.slots.data(), "Inspector::ScriptProfilerBackendDispatcherHandler::ScriptProfilerBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector38ScriptProfilerBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector30TargetBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, TargetHandlerVtable.slots.data(), "Inspector::TargetBackendDispatcherHandler::TargetBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector30TargetBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, TargetHandlerVtable.slots.data(), "Inspector::TargetBackendDispatcherHandler::TargetBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector30TargetBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

void APS5_VABI _ZN9Inspector32TimelineBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, TimelineHandlerVtable.slots.data(), "Inspector::TimelineBackendDispatcherHandler::TimelineBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector30WorkerBackendDispatcherHandlerC2Ev(BackendDispatcherHandler* self) {
    Construct(self, WorkerHandlerVtable.slots.data(), "Inspector::WorkerBackendDispatcherHandler::WorkerBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector30WorkerBackendDispatcherHandlerC2ERKS0_(BackendDispatcherHandler* self, const BackendDispatcherHandler* other) {
    Copy(self, other, WorkerHandlerVtable.slots.data(), "Inspector::WorkerBackendDispatcherHandler::WorkerBackendDispatcherHandler");
}

void APS5_VABI _ZN9Inspector30WorkerBackendDispatcherHandlerD0Ev(BackendDispatcherHandler* self) {
    NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor(self);
}

}
