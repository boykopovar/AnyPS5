#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit forwarding check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

using StubForward = int (APS5_VABI *)();

using Allocate = void* (APS5_VABI*)(std::size_t);
using Release = void (APS5_VABI*)(void*);
using Value = void* (APS5_VABI*)();

static void ExpectStub(StubForward forward, int line) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        forward();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw || NotImplementedCalls_nid_no_patch() != before + 1) {
        std::fprintf(stderr, "libSceNKWebKit forwarding reached no stub at line %d\n", line);
        std::abort();
    }
}

#define RequireStub(forward) ExpectStub(forward, __LINE__)

int main() {
    RequireStub(&JSRemoteInspectorDisableAutoStart);
    RequireStub(&WKAccessibilityEnableEnhancedAccessibility);
    RequireStub(&WKContextGetApplicationCacheManager);
    RequireStub(&WKContextGetKeyValueStorageManager);
    RequireStub(&WKContextGetResourceCacheManager);
    RequireStub(&WKContextGetWebsiteDataStore);
    RequireStub(&_ZN3JSC11createErrorEPNS_14JSGlobalObjectENS_9ErrorTypeERKN3WTF6StringE);
    RequireStub(&_ZN3JSC17DeferredWorkTimerD1Ev);
    RequireStub(&_ZN3JSC18GCActivityCallbackD2Ev);
    RequireStub(&_ZN3JSC22EdenGCActivityCallbackD1Ev);
    RequireStub(&_ZN3JSC22EdenGCActivityCallbackD2Ev);
    RequireStub(&_ZN3JSC22FullGCActivityCallbackD1Ev);
    RequireStub(&_ZN3JSC22FullGCActivityCallbackD2Ev);
    RequireStub(&_ZN3JSC6JSLock12DropAllLocksC1ERNS_2VME);
    RequireStub(&_ZN3JSC6JSLock12DropAllLocksC2ERNS_2VME);
    RequireStub(&_ZN7WebCore12ISOWebVTTCueD1Ev);
    RequireStub(&_ZN7WebCore13MIMETypeCacheD1Ev);
    RequireStub(&_ZN7WebCore16HTMLMediaElement15clearMediaCacheERKN3WTF6StringENS1_8WallTimeE);
    RequireStub(&_ZN7WebCore18CustomHeaderFieldsD1Ev);
    RequireStub(&_ZN7WebCore26ISOProtectionSchemeInfoBoxD1Ev);
    RequireStub(&_ZN7WebCore30InspectorInstrumentationWebKit16interceptRequestERNS_14ResourceLoaderEON3WTF8FunctionIFvRKNS_15ResourceRequestEEEE);
    RequireStub(&_ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC1ERKS0_);
    RequireStub(&_ZN9Inspector15RemoteInspectorD1Ev);
    RequireStub(&_ZN9Inspector17BackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector20CSSBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector20CSSBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector20DOMBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector20DOMBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector20InjectedScriptModuleD1Ev);
    RequireStub(&_ZN9Inspector20InjectedScriptModuleD2Ev);
    RequireStub(&_ZN9Inspector21HeapBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector21HeapBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector21PageBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector21PageBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector22AuditBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector22AuditBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector22RemoteInspectionTargetD2Ev);
    RequireStub(&_ZN9Inspector23CanvasBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector23CanvasBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector23TargetBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector23TargetBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector23WorkerBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector23WorkerBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector24BrowserBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector24BrowserBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector24ConsoleBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector24ConsoleBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector24NetworkBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector24NetworkBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector24RuntimeBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector24RuntimeBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector25DatabaseBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector25DatabaseBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector25DebuggerBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector25DebuggerBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector25TimelineBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector25TimelineBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector26AnimationBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector26AnimationBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector26InspectorBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector26InspectorBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector26LayerTreeBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector26LayerTreeBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector27DOMStorageBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector27DOMStorageBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector28DOMDebuggerBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector28DOMDebuggerBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector31ScriptProfilerBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector31ScriptProfilerBackendDispatcherD2Ev);
    RequireStub(&_ZN9Inspector33ApplicationCacheBackendDispatcherD1Ev);
    RequireStub(&_ZN9Inspector33ApplicationCacheBackendDispatcherD2Ev);
    RequireStub(&_ZNK9Inspector15ScriptCallFrameeqERKS0_);

    auto* object = static_cast<std::uint8_t*>(reinterpret_cast<Allocate>(&_ZN3JSC19HeapSnapshotBuildernwEm)(128));
    Require(object != nullptr);
    std::memset(object, 0xa5, 128);
    reinterpret_cast<Release>(&_ZN3JSC19HeapSnapshotBuilderdlEPv)(object);

    auto* array = static_cast<std::uint8_t*>(reinterpret_cast<Allocate>(&_ZN3JSC19HeapSnapshotBuildernaEm)(64));
    Require(array != nullptr);
    std::memset(array, 0x5a, 64);
    reinterpret_cast<Release>(&_ZN3JSC19HeapSnapshotBuilderdaEPv)(array);

    auto* freed = static_cast<std::uint8_t*>(reinterpret_cast<Allocate>(&_ZN3JSC19HeapSnapshotBuildernwEm)(32));
    Require(freed != nullptr);
    reinterpret_cast<Release>(&_ZN3JSC32JSDestructibleObjectHeapCellTypeD1Ev)(freed);
    reinterpret_cast<Release>(&_ZN3JSC32JSDestructibleObjectHeapCellTypeD2Ev)(freed);
    reinterpret_cast<Release>(&_ZN3JSC12HeapCellTypeD0Ev)(freed);

    auto* mime = static_cast<std::uint8_t*>(reinterpret_cast<Allocate>(&_ZN7WebCore13MIMETypeCachenwEm)(24));
    Require(mime != nullptr);
    reinterpret_cast<Release>(&_ZN7WebCore13MIMETypeCachedlEPv)(mime);

    auto* inspected = static_cast<std::uint8_t*>(reinterpret_cast<Allocate>(&_ZN3JSC19HeapSnapshotBuildernwEm)(16));
    Require(inspected != nullptr);
    reinterpret_cast<Release>(&_ZN9Inspector18InspectorHeapAgentdlEPv)(inspected);

    Require(reinterpret_cast<Value>(&WKUserMediaPermissionRequestAudioDeviceUIDs)() != nullptr);
    Require(reinterpret_cast<Value>(&WKUserMediaPermissionRequestVideoDeviceUIDs)() != nullptr);
    Require(WKMutableArrayCreate() != nullptr);

    return 0;
}
