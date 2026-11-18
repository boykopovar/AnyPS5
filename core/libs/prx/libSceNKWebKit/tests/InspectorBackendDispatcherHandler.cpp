#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/InspectorBackendDispatcherHandlerExports.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <unordered_set>

using NKWebKit::InspectorAbi::BackendDispatcherHandler;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "InspectorBackendDispatcherHandler check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::unordered_set<void*> guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(guestAllocations.erase(pointer) == 1);
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t, std::size_t size) {
    *pointer = Allocate_nid_no_patch(size);
    return *pointer ? 0 : 12;
}

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

using Construct = void (APS5_VABI*)(BackendDispatcherHandler*);
using CopyConstruct = void (APS5_VABI*)(BackendDispatcherHandler*, const BackendDispatcherHandler*);
using Destroy = void (APS5_VABI*)(BackendDispatcherHandler*);

struct Domain {
    const char* name;
    std::size_t commands;
    Construct construct;
    CopyConstruct copy;
    Destroy deletingDestructor;
};

static const Domain Domains[] = {
    {"Animation", 6, &_ZN9Inspector33AnimationBackendDispatcherHandlerC2Ev, &_ZN9Inspector33AnimationBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector33AnimationBackendDispatcherHandlerD0Ev},
    {"ApplicationCache", 5, &_ZN9Inspector40ApplicationCacheBackendDispatcherHandlerC2Ev, &_ZN9Inspector40ApplicationCacheBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector40ApplicationCacheBackendDispatcherHandlerD0Ev},
    {"Audit", 3, &_ZN9Inspector29AuditBackendDispatcherHandlerC2Ev, &_ZN9Inspector29AuditBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector29AuditBackendDispatcherHandlerD0Ev},
    {"Browser", 2, &_ZN9Inspector31BrowserBackendDispatcherHandlerC2Ev, &_ZN9Inspector31BrowserBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector31BrowserBackendDispatcherHandlerD0Ev},
    {"CSS", 19, &_ZN9Inspector27CSSBackendDispatcherHandlerC2Ev, &_ZN9Inspector27CSSBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector27CSSBackendDispatcherHandlerD0Ev},
    {"Canvas", 9, &_ZN9Inspector30CanvasBackendDispatcherHandlerC2Ev, &_ZN9Inspector30CanvasBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector30CanvasBackendDispatcherHandlerD0Ev},
    {"Console", 5, &_ZN9Inspector31ConsoleBackendDispatcherHandlerC2Ev, &_ZN9Inspector31ConsoleBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector31ConsoleBackendDispatcherHandlerD0Ev},
    {"DOM", 45, &_ZN9Inspector27DOMBackendDispatcherHandlerC2Ev, &_ZN9Inspector27DOMBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector27DOMBackendDispatcherHandlerD0Ev},
    {"DOMDebugger", 6, &_ZN9Inspector35DOMDebuggerBackendDispatcherHandlerC2Ev, &_ZN9Inspector35DOMDebuggerBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector35DOMDebuggerBackendDispatcherHandlerD0Ev},
    {"DOMStorage", 6, &_ZN9Inspector34DOMStorageBackendDispatcherHandlerC2Ev, &_ZN9Inspector34DOMStorageBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector34DOMStorageBackendDispatcherHandlerD0Ev},
    {"Database", 4, &_ZN9Inspector32DatabaseBackendDispatcherHandlerC2Ev, &_ZN9Inspector32DatabaseBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector32DatabaseBackendDispatcherHandlerD0Ev},
    {"Debugger", 29, &_ZN9Inspector32DebuggerBackendDispatcherHandlerC2Ev, &_ZN9Inspector32DebuggerBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector32DebuggerBackendDispatcherHandlerD0Ev},
    {"Heap", 8, &_ZN9Inspector28HeapBackendDispatcherHandlerC2Ev, &_ZN9Inspector28HeapBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector28HeapBackendDispatcherHandlerD0Ev},
    {"Inspector", 3, &_ZN9Inspector33InspectorBackendDispatcherHandlerC2Ev, &_ZN9Inspector33InspectorBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector33InspectorBackendDispatcherHandlerD0Ev},
    {"LayerTree", 4, &_ZN9Inspector33LayerTreeBackendDispatcherHandlerC2Ev, &_ZN9Inspector33LayerTreeBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector33LayerTreeBackendDispatcherHandlerD0Ev},
    {"Network", 16, &_ZN9Inspector31NetworkBackendDispatcherHandlerC2Ev, &_ZN9Inspector31NetworkBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector31NetworkBackendDispatcherHandlerD0Ev},
    {"Page", 21, &_ZN9Inspector28PageBackendDispatcherHandlerC2Ev, &_ZN9Inspector28PageBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector28PageBackendDispatcherHandlerD0Ev},
    {"Runtime", 20, &_ZN9Inspector31RuntimeBackendDispatcherHandlerC2Ev, &_ZN9Inspector31RuntimeBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector31RuntimeBackendDispatcherHandlerD0Ev},
    {"ScriptProfiler", 2, &_ZN9Inspector38ScriptProfilerBackendDispatcherHandlerC2Ev, &_ZN9Inspector38ScriptProfilerBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector38ScriptProfilerBackendDispatcherHandlerD0Ev},
    {"Target", 3, &_ZN9Inspector30TargetBackendDispatcherHandlerC2Ev, &_ZN9Inspector30TargetBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector30TargetBackendDispatcherHandlerD0Ev},
    {"Timeline", 6, nullptr, &_ZN9Inspector32TimelineBackendDispatcherHandlerC2ERKS0_, nullptr},
    {"Worker", 4, &_ZN9Inspector30WorkerBackendDispatcherHandlerC2Ev, &_ZN9Inspector30WorkerBackendDispatcherHandlerC2ERKS0_, &_ZN9Inspector30WorkerBackendDispatcherHandlerD0Ev},
};

static void RequireVtable(const Domain& domain, const void* const* vptr) {
    const auto* layout = NKWebKit::InspectorAbi::FindBackendDispatcherHandlerLayout(domain.name);
    Require(layout && layout->commandCount == domain.commands && layout->vtable == vptr);
    Require(reinterpret_cast<const std::ptrdiff_t*>(vptr)[-2] == 0 && vptr[-1] == nullptr);
    for (std::size_t i = 0; i < domain.commands; ++i) Require(vptr[i] == reinterpret_cast<const void*>(&__cxa_pure_virtual_nid_postfix));
    Require(vptr[domain.commands] == reinterpret_cast<const void*>(&NKWebKit::InspectorAbi::BackendDispatcherHandlerCompleteDestructor));
    Require(vptr[domain.commands + 1] == reinterpret_cast<const void*>(&NKWebKit::InspectorAbi::BackendDispatcherHandlerDeletingDestructor));
}

static void TestConstructors() {
    for (const auto& domain : Domains) {
        BackendDispatcherHandler handler{nullptr};
        BackendDispatcherHandler copy{nullptr};
        if (domain.construct) {
            domain.construct(&handler);
            RequireVtable(domain, handler.vptr);
            Require(ThrowsInvalidArgument([&] { domain.construct(nullptr); }));
        }
        if (domain.copy) {
            const BackendDispatcherHandler unrelated{reinterpret_cast<const void* const*>(&copy)};
            domain.copy(&copy, &unrelated);
            RequireVtable(domain, copy.vptr);
            Require(ThrowsInvalidArgument([&] { domain.copy(&copy, nullptr); }));
            Require(ThrowsInvalidArgument([&] { domain.copy(nullptr, &unrelated); }));
        }
    }
}

static void TestDeletingDestructors() {
    bool throughSlot = true;
    for (const auto& domain : Domains) {
        if (!domain.deletingDestructor) continue;
        auto* handler = static_cast<BackendDispatcherHandler*>(_Znwm_nid_postfix(sizeof(BackendDispatcherHandler)));
        Require(guestAllocations.count(handler) == 1);
        domain.construct(handler);
        if (throughSlot) {
            reinterpret_cast<Destroy>(const_cast<void*>(handler->vptr[domain.commands + 1]))(handler);
        } else {
            domain.deletingDestructor(handler);
        }
        throughSlot = !throughSlot;
        Require(guestAllocations.empty());
        Require(ThrowsInvalidArgument([&] { domain.deletingDestructor(nullptr); }));
    }
}

static void TestLayouts() {
    Require(sizeof(Domains) / sizeof(Domains[0]) == 22);
    Require(!NKWebKit::InspectorAbi::FindBackendDispatcherHandlerLayout("ServiceWorker"));
    Require(!NKWebKit::InspectorAbi::FindBackendDispatcherHandlerLayout(nullptr));
    BackendDispatcherHandler handler{nullptr};
    NKWebKit::InspectorAbi::BackendDispatcherHandlerCompleteDestructor(&handler);
    Require(ThrowsInvalidArgument([] { NKWebKit::InspectorAbi::BackendDispatcherHandlerCompleteDestructor(nullptr); }));
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestLayouts();
    TestConstructors();
    TestDeletingDestructors();
    return 0;
}
