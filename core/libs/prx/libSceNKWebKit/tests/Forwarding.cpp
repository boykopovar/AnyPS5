#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" std::uint64_t NotImplementedCalls_nid_no_patch();

extern "C" {
int JSRemoteInspectorDisableAutoStart();
int WKAccessibilityEnableEnhancedAccessibility();
int WKContextGetKeyValueStorageManager();
int WKContextGetResourceCacheManager();
int WKContextGetWebsiteDataStore();
int _ZN3JSC11createErrorEPNS_14JSGlobalObjectENS_9ErrorTypeERKN3WTF6StringE();
int _ZN3JSC17DeferredWorkTimerD1Ev();
int _ZN3JSC18GCActivityCallbackD2Ev();
int _ZN3JSC22EdenGCActivityCallbackD1Ev();
int _ZN3JSC22EdenGCActivityCallbackD2Ev();
int _ZN3JSC22FullGCActivityCallbackD1Ev();
int _ZN3JSC22FullGCActivityCallbackD2Ev();
int _ZN3JSC32JSDestructibleObjectHeapCellTypeD1Ev();
int _ZN3JSC32JSDestructibleObjectHeapCellTypeD2Ev();
int _ZN3JSC6JSLock12DropAllLocksC1ERNS_2VME();
int _ZN3JSC6JSLock12DropAllLocksC2ERNS_2VME();
int _ZN7WebCore11EventRegionC1Ev();
int _ZN7WebCore11EventRegionC2Ev();
int _ZN7WebCore12ISOWebVTTCueD1Ev();
int _ZN7WebCore13MIMETypeCacheD1Ev();
int _ZN7WebCore16HTMLMediaElement15clearMediaCacheERKN3WTF6StringENS1_8WallTimeE();
int _ZN7WebCore18CustomHeaderFieldsD1Ev();
int _ZN7WebCore26ISOProtectionSchemeInfoBoxD1Ev();
int _ZN7WebCore30InspectorInstrumentationWebKit16interceptRequestERNS_14ResourceLoaderEON3WTF8FunctionIFvRKNS_15ResourceRequestEEEE();
int _ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC1ERKS0_();
int _ZN7WebCore5Image12supportsTypeERKN3WTF6StringE();
int _ZN9Inspector15RemoteInspectorD1Ev();
int _ZN9Inspector17BackendDispatcherD1Ev();
int _ZN9Inspector20CSSBackendDispatcherD1Ev();
int _ZN9Inspector20CSSBackendDispatcherD2Ev();
int _ZN9Inspector20DOMBackendDispatcherD1Ev();
int _ZN9Inspector20DOMBackendDispatcherD2Ev();
int _ZN9Inspector20InjectedScriptModuleD1Ev();
int _ZN9Inspector20InjectedScriptModuleD2Ev();
int _ZN9Inspector21HeapBackendDispatcherD1Ev();
int _ZN9Inspector21HeapBackendDispatcherD2Ev();
int _ZN9Inspector21PageBackendDispatcherD1Ev();
int _ZN9Inspector21PageBackendDispatcherD2Ev();
int _ZN9Inspector22AuditBackendDispatcherD1Ev();
int _ZN9Inspector22AuditBackendDispatcherD2Ev();
int _ZN9Inspector22RemoteInspectionTargetD2Ev();
int _ZN9Inspector23CanvasBackendDispatcherD1Ev();
int _ZN9Inspector23CanvasBackendDispatcherD2Ev();
int _ZN9Inspector23TargetBackendDispatcherD1Ev();
int _ZN9Inspector23TargetBackendDispatcherD2Ev();
int _ZN9Inspector23WorkerBackendDispatcherD1Ev();
int _ZN9Inspector23WorkerBackendDispatcherD2Ev();
int _ZN9Inspector24BrowserBackendDispatcherD1Ev();
int _ZN9Inspector24BrowserBackendDispatcherD2Ev();
int _ZN9Inspector24ConsoleBackendDispatcherD1Ev();
int _ZN9Inspector24ConsoleBackendDispatcherD2Ev();
int _ZN9Inspector24NetworkBackendDispatcherD1Ev();
int _ZN9Inspector24NetworkBackendDispatcherD2Ev();
int _ZN9Inspector24RuntimeBackendDispatcherD1Ev();
int _ZN9Inspector24RuntimeBackendDispatcherD2Ev();
int _ZN9Inspector25DatabaseBackendDispatcherD1Ev();
int _ZN9Inspector25DatabaseBackendDispatcherD2Ev();
int _ZN9Inspector25DebuggerBackendDispatcherD1Ev();
int _ZN9Inspector25DebuggerBackendDispatcherD2Ev();
int _ZN9Inspector25TimelineBackendDispatcherD1Ev();
int _ZN9Inspector25TimelineBackendDispatcherD2Ev();
int _ZN9Inspector26AnimationBackendDispatcherD1Ev();
int _ZN9Inspector26AnimationBackendDispatcherD2Ev();
int _ZN9Inspector26InspectorBackendDispatcherD1Ev();
int _ZN9Inspector26InspectorBackendDispatcherD2Ev();
int _ZN9Inspector26LayerTreeBackendDispatcherD1Ev();
int _ZN9Inspector26LayerTreeBackendDispatcherD2Ev();
int _ZN9Inspector27DOMStorageBackendDispatcherD1Ev();
int _ZN9Inspector27DOMStorageBackendDispatcherD2Ev();
int _ZN9Inspector28DOMDebuggerBackendDispatcherD1Ev();
int _ZN9Inspector28DOMDebuggerBackendDispatcherD2Ev();
int _ZN9Inspector31ScriptProfilerBackendDispatcherD1Ev();
int _ZN9Inspector31ScriptProfilerBackendDispatcherD2Ev();
int _ZN9Inspector33ApplicationCacheBackendDispatcherD1Ev();
int _ZN9Inspector33ApplicationCacheBackendDispatcherD2Ev();
int _ZNK9Inspector15ScriptCallFrameeqERKS0_();
void* _ZN3JSC19HeapSnapshotBuildernwEm(unsigned long);
void* _ZN3JSC19HeapSnapshotBuildernaEm(unsigned long);
void _ZN3JSC19HeapSnapshotBuilderdlEPv(void*);
void _ZN3JSC19HeapSnapshotBuilderdaEPv(void*);
void _ZN3JSC12HeapCellTypeD0Ev(void*);
void* WKMutableArrayCreate();
void* WKUserMediaPermissionRequestAudioDeviceUIDs();
void* WKUserMediaPermissionRequestVideoDeviceUIDs();
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit forwarding check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

static void ExpectStub(int (*forward)(), int line) {
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
    RequireStub(&_ZN3JSC32JSDestructibleObjectHeapCellTypeD1Ev);
    RequireStub(&_ZN3JSC32JSDestructibleObjectHeapCellTypeD2Ev);
    RequireStub(&_ZN3JSC6JSLock12DropAllLocksC1ERNS_2VME);
    RequireStub(&_ZN3JSC6JSLock12DropAllLocksC2ERNS_2VME);
    RequireStub(&_ZN7WebCore11EventRegionC1Ev);
    RequireStub(&_ZN7WebCore11EventRegionC2Ev);
    RequireStub(&_ZN7WebCore12ISOWebVTTCueD1Ev);
    RequireStub(&_ZN7WebCore13MIMETypeCacheD1Ev);
    RequireStub(&_ZN7WebCore16HTMLMediaElement15clearMediaCacheERKN3WTF6StringENS1_8WallTimeE);
    RequireStub(&_ZN7WebCore18CustomHeaderFieldsD1Ev);
    RequireStub(&_ZN7WebCore26ISOProtectionSchemeInfoBoxD1Ev);
    RequireStub(&_ZN7WebCore30InspectorInstrumentationWebKit16interceptRequestERNS_14ResourceLoaderEON3WTF8FunctionIFvRKNS_15ResourceRequestEEEE);
    RequireStub(&_ZN7WebCore36ISOProtectionSystemSpecificHeaderBoxC1ERKS0_);
    RequireStub(&_ZN7WebCore5Image12supportsTypeERKN3WTF6StringE);
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

    auto* object = static_cast<std::uint8_t*>(_ZN3JSC19HeapSnapshotBuildernwEm(128));
    Require(object != nullptr);
    std::memset(object, 0xa5, 128);
    _ZN3JSC19HeapSnapshotBuilderdlEPv(object);

    auto* array = static_cast<std::uint8_t*>(_ZN3JSC19HeapSnapshotBuildernaEm(64));
    Require(array != nullptr);
    std::memset(array, 0x5a, 64);
    _ZN3JSC19HeapSnapshotBuilderdaEPv(array);

    auto* freed = static_cast<std::uint8_t*>(_ZN3JSC19HeapSnapshotBuildernwEm(32));
    Require(freed != nullptr);
    _ZN3JSC12HeapCellTypeD0Ev(freed);

    Require(WKUserMediaPermissionRequestAudioDeviceUIDs() != nullptr);
    Require(WKUserMediaPermissionRequestVideoDeviceUIDs() != nullptr);
    Require(WKMutableArrayCreate() != nullptr);

    return 0;
}
