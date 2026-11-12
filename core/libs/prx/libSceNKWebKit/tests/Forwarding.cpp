#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <type_traits>

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

enum class AllocationResult {
    ExpectedStub,
    Implemented,
    Unexpected
};

static AllocationResult ExpectAllocationStub(StubForward forward, const char* name) {
    const auto before = NotImplementedCalls_nid_no_patch();
    try {
        (void)forward();
    } catch (const std::runtime_error& error) {
        char expectedMessage[256];
        std::snprintf(expectedMessage, sizeof(expectedMessage), "%s not implemented", name);
        const bool counted = NotImplementedCalls_nid_no_patch() == before + 1;
        const bool named = std::strcmp(error.what(), expectedMessage) == 0;
        std::fprintf(stderr, "EXPECTED_ALLOCATION_THROW name=%s message=%s count=%s\n", name, error.what(), counted ? "yes" : "no");
        return counted && named ? AllocationResult::ExpectedStub : AllocationResult::Unexpected;
    }
    std::fprintf(stderr, "MISSING_ALLOCATION_THROW name=%s\n", name);
    return AllocationResult::Unexpected;
}

template <typename Forward, typename DeleteForward>
static AllocationResult TestAllocation(Forward forward, DeleteForward release, const char* name) {
    if constexpr (std::is_invocable_v<Forward>) {
        return ExpectAllocationStub(forward, name);
    } else {
        const std::size_t sizes[] = {0, 128};
        for (const std::size_t size : sizes) {
            auto* memory = static_cast<std::uint8_t*>(forward(size));
            Require(memory != nullptr);
            Require(reinterpret_cast<std::uintptr_t>(memory) % alignof(std::max_align_t) == 0);
            Require(_ZN3WTF14fastMallocSizeEPKv(memory) == size);
            for (std::size_t index = 0; index < size; ++index) {
                memory[index] = static_cast<std::uint8_t>((index * 17 + size) & 0xff);
            }
            for (std::size_t index = 0; index < size; ++index) {
                Require(memory[index] == static_cast<std::uint8_t>((index * 17 + size) & 0xff));
            }
            release(memory);
        }
        return AllocationResult::Implemented;
    }
}

template <typename Forward>
static AllocationResult TestDelete(Forward forward, const char* name) {
    if constexpr (std::is_invocable_v<Forward>) {
        return ExpectAllocationStub(forward, name);
    } else {
        forward(nullptr);
        return AllocationResult::Implemented;
    }
}

template <typename Forward>
static AllocationResult TestPlacement(Forward forward, const char* name) {
    if constexpr (std::is_invocable_v<Forward>) {
        return ExpectAllocationStub(forward, name);
    } else {
        alignas(std::max_align_t) std::uint8_t storage[64]{};
        void* const place = storage + 1;
        Require(forward(0, place) == place);
        Require(forward(64, place) == place);
        Require(forward(0, nullptr) == nullptr);
        Require(forward(64, nullptr) == nullptr);
        return AllocationResult::Implemented;
    }
}

static int FinishAllocationResults(const AllocationResult* results, std::size_t count) {
    std::size_t expectedStubs = 0;
    std::size_t implemented = 0;
    std::size_t unexpected = 0;
    for (std::size_t index = 0; index < count; ++index) {
        expectedStubs += results[index] == AllocationResult::ExpectedStub;
        implemented += results[index] == AllocationResult::Implemented;
        unexpected += results[index] == AllocationResult::Unexpected;
    }
    if (expectedStubs == count && unexpected == 0) {
        std::fprintf(stderr, "RED_EXPECTED_STUBS count=%zu\n", expectedStubs);
        return 1;
    }
    if (implemented == count && unexpected == 0) {
        return 0;
    }
    std::fprintf(stderr, "ALLOCATION_FIXTURE_MISMATCH expected_stubs=%zu implemented=%zu unexpected=%zu\n", expectedStubs, implemented, unexpected);
    return 2;
}

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

    const AllocationResult allocationResults[] = {
        TestAllocation(&_ZN7WebCore9GLContextnwEm, &_ZN7WebCore9GLContextdlEPv, "_ZN7WebCore9GLContextnwEm"),
        TestAllocation(&_ZN7WebCore9GLContextnaEm, &_ZN7WebCore9GLContextdaEPv, "_ZN7WebCore9GLContextnaEm"),
        TestDelete(&_ZN7WebCore9GLContextdlEPv, "_ZN7WebCore9GLContextdlEPv"),
        TestDelete(&_ZN7WebCore9GLContextdaEPv, "_ZN7WebCore9GLContextdaEPv"),
        TestPlacement(&_ZN7WebCore9GLContextnwEmPv, "_ZN7WebCore9GLContextnwEmPv"),
        TestPlacement(&_ZN7WebCore9GLContextnaEmPv, "_ZN7WebCore9GLContextnaEmPv"),
        TestAllocation(&_ZN7WebCore17LibWebRTCProvidernwEm, &_ZN7WebCore17LibWebRTCProviderdlEPv, "_ZN7WebCore17LibWebRTCProvidernwEm"),
        TestAllocation(&_ZN7WebCore17LibWebRTCProvidernaEm, &_ZN7WebCore17LibWebRTCProviderdaEPv, "_ZN7WebCore17LibWebRTCProvidernaEm"),
        TestDelete(&_ZN7WebCore17LibWebRTCProviderdlEPv, "_ZN7WebCore17LibWebRTCProviderdlEPv"),
        TestDelete(&_ZN7WebCore17LibWebRTCProviderdaEPv, "_ZN7WebCore17LibWebRTCProviderdaEPv"),
        TestPlacement(&_ZN7WebCore17LibWebRTCProvidernwEmPv, "_ZN7WebCore17LibWebRTCProvidernwEmPv"),
        TestPlacement(&_ZN7WebCore17LibWebRTCProvidernaEmPv, "_ZN7WebCore17LibWebRTCProvidernaEmPv"),
        TestAllocation(&_ZN7WebCore6ISOBoxnwEm, &_ZN7WebCore6ISOBoxdlEPv, "_ZN7WebCore6ISOBoxnwEm"),
        TestAllocation(&_ZN7WebCore6ISOBoxnaEm, &_ZN7WebCore6ISOBoxdaEPv, "_ZN7WebCore6ISOBoxnaEm"),
        TestDelete(&_ZN7WebCore6ISOBoxdlEPv, "_ZN7WebCore6ISOBoxdlEPv"),
        TestDelete(&_ZN7WebCore6ISOBoxdaEPv, "_ZN7WebCore6ISOBoxdaEPv"),
        TestPlacement(&_ZN7WebCore6ISOBoxnwEmPv, "_ZN7WebCore6ISOBoxnwEmPv"),
        TestPlacement(&_ZN7WebCore6ISOBoxnaEmPv, "_ZN7WebCore6ISOBoxnaEmPv")
    };

    return FinishAllocationResults(allocationResults, sizeof(allocationResults) / sizeof(allocationResults[0]));
}
