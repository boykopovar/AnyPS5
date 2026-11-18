#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <iterator>
#include <utility>
#include <vector>

namespace {

using NKWebKit::InspectorAbi::BackendDispatcher;
using NKWebKit::InspectorAbi::DomainBackendDispatcher;
using NKWebKit::InspectorAbi::DomainBackendDispatcherRef;
using NKWebKit::InspectorAbi::FrontendChannel;
using NKWebKit::InspectorAbi::FrontendRouter;
using NKWebKit::InspectorAbi::FrontendRouterRef;
using NKWebKit::InspectorAbi::SupplementalBackendDispatcher;
using NKWebKit::InspectorAbi::CallbackBase;
using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::Value;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Inspector domain dispatcher check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename Exception, typename Action> bool Throws(Action action) {
    try {
        action();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

int guestAllocations;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations++;
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (pointer) guestAllocations--;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    if (pointer) guestAllocations++;
    return pointer;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (!pointer) return Allocate_nid_no_patch(size);
    return std::realloc(pointer, size);
}

void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) {
    return Allocate_nid_no_patch(size);
}

void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) {
    return Reallocate_nid_no_patch(pointer, size);
}

int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

void RegisterHeap() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
}

using CreateFunction = DomainBackendDispatcherRef (APS5_VABI*)(BackendDispatcher*, void*);
using ConstructFunction = void (APS5_VABI*)(DomainBackendDispatcher*, BackendDispatcher*, void*);
using DestroyFunction = void (APS5_VABI*)(DomainBackendDispatcher*);

struct DomainExport {
    const char* name;
    CreateFunction create;
    ConstructFunction complete;
    ConstructFunction base;
    DestroyFunction deleting;
};

const DomainExport DomainExports[] = {
    {"Animation", &_ZN9Inspector26AnimationBackendDispatcher6createERNS_17BackendDispatcherEPNS_33AnimationBackendDispatcherHandlerE, &_ZN9Inspector26AnimationBackendDispatcherC1ERNS_17BackendDispatcherEPNS_33AnimationBackendDispatcherHandlerE, &_ZN9Inspector26AnimationBackendDispatcherC2ERNS_17BackendDispatcherEPNS_33AnimationBackendDispatcherHandlerE, &_ZN9Inspector26AnimationBackendDispatcherD0Ev},
    {"ApplicationCache", &_ZN9Inspector33ApplicationCacheBackendDispatcher6createERNS_17BackendDispatcherEPNS_40ApplicationCacheBackendDispatcherHandlerE, &_ZN9Inspector33ApplicationCacheBackendDispatcherC1ERNS_17BackendDispatcherEPNS_40ApplicationCacheBackendDispatcherHandlerE, &_ZN9Inspector33ApplicationCacheBackendDispatcherC2ERNS_17BackendDispatcherEPNS_40ApplicationCacheBackendDispatcherHandlerE, &_ZN9Inspector33ApplicationCacheBackendDispatcherD0Ev},
    {"Audit", &_ZN9Inspector22AuditBackendDispatcher6createERNS_17BackendDispatcherEPNS_29AuditBackendDispatcherHandlerE, &_ZN9Inspector22AuditBackendDispatcherC1ERNS_17BackendDispatcherEPNS_29AuditBackendDispatcherHandlerE, &_ZN9Inspector22AuditBackendDispatcherC2ERNS_17BackendDispatcherEPNS_29AuditBackendDispatcherHandlerE, &_ZN9Inspector22AuditBackendDispatcherD0Ev},
    {"Browser", &_ZN9Inspector24BrowserBackendDispatcher6createERNS_17BackendDispatcherEPNS_31BrowserBackendDispatcherHandlerE, &_ZN9Inspector24BrowserBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31BrowserBackendDispatcherHandlerE, &_ZN9Inspector24BrowserBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31BrowserBackendDispatcherHandlerE, &_ZN9Inspector24BrowserBackendDispatcherD0Ev},
    {"Canvas", &_ZN9Inspector23CanvasBackendDispatcher6createERNS_17BackendDispatcherEPNS_30CanvasBackendDispatcherHandlerE, &_ZN9Inspector23CanvasBackendDispatcherC1ERNS_17BackendDispatcherEPNS_30CanvasBackendDispatcherHandlerE, nullptr, &_ZN9Inspector23CanvasBackendDispatcherD0Ev},
    {"Console", &_ZN9Inspector24ConsoleBackendDispatcher6createERNS_17BackendDispatcherEPNS_31ConsoleBackendDispatcherHandlerE, &_ZN9Inspector24ConsoleBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31ConsoleBackendDispatcherHandlerE, &_ZN9Inspector24ConsoleBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31ConsoleBackendDispatcherHandlerE, nullptr},
    {"CSS", &_ZN9Inspector20CSSBackendDispatcher6createERNS_17BackendDispatcherEPNS_27CSSBackendDispatcherHandlerE, &_ZN9Inspector20CSSBackendDispatcherC1ERNS_17BackendDispatcherEPNS_27CSSBackendDispatcherHandlerE, &_ZN9Inspector20CSSBackendDispatcherC2ERNS_17BackendDispatcherEPNS_27CSSBackendDispatcherHandlerE, &_ZN9Inspector20CSSBackendDispatcherD0Ev},
    {"Database", &_ZN9Inspector25DatabaseBackendDispatcher6createERNS_17BackendDispatcherEPNS_32DatabaseBackendDispatcherHandlerE, &_ZN9Inspector25DatabaseBackendDispatcherC1ERNS_17BackendDispatcherEPNS_32DatabaseBackendDispatcherHandlerE, &_ZN9Inspector25DatabaseBackendDispatcherC2ERNS_17BackendDispatcherEPNS_32DatabaseBackendDispatcherHandlerE, &_ZN9Inspector25DatabaseBackendDispatcherD0Ev},
    {"Debugger", &_ZN9Inspector25DebuggerBackendDispatcher6createERNS_17BackendDispatcherEPNS_32DebuggerBackendDispatcherHandlerE, &_ZN9Inspector25DebuggerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_32DebuggerBackendDispatcherHandlerE, &_ZN9Inspector25DebuggerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_32DebuggerBackendDispatcherHandlerE, &_ZN9Inspector25DebuggerBackendDispatcherD0Ev},
    {"DOM", &_ZN9Inspector20DOMBackendDispatcher6createERNS_17BackendDispatcherEPNS_27DOMBackendDispatcherHandlerE, &_ZN9Inspector20DOMBackendDispatcherC1ERNS_17BackendDispatcherEPNS_27DOMBackendDispatcherHandlerE, &_ZN9Inspector20DOMBackendDispatcherC2ERNS_17BackendDispatcherEPNS_27DOMBackendDispatcherHandlerE, &_ZN9Inspector20DOMBackendDispatcherD0Ev},
    {"DOMDebugger", &_ZN9Inspector28DOMDebuggerBackendDispatcher6createERNS_17BackendDispatcherEPNS_35DOMDebuggerBackendDispatcherHandlerE, &_ZN9Inspector28DOMDebuggerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_35DOMDebuggerBackendDispatcherHandlerE, &_ZN9Inspector28DOMDebuggerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_35DOMDebuggerBackendDispatcherHandlerE, nullptr},
    {"DOMStorage", &_ZN9Inspector27DOMStorageBackendDispatcher6createERNS_17BackendDispatcherEPNS_34DOMStorageBackendDispatcherHandlerE, &_ZN9Inspector27DOMStorageBackendDispatcherC1ERNS_17BackendDispatcherEPNS_34DOMStorageBackendDispatcherHandlerE, &_ZN9Inspector27DOMStorageBackendDispatcherC2ERNS_17BackendDispatcherEPNS_34DOMStorageBackendDispatcherHandlerE, &_ZN9Inspector27DOMStorageBackendDispatcherD0Ev},
    {"Heap", nullptr, &_ZN9Inspector21HeapBackendDispatcherC1ERNS_17BackendDispatcherEPNS_28HeapBackendDispatcherHandlerE, &_ZN9Inspector21HeapBackendDispatcherC2ERNS_17BackendDispatcherEPNS_28HeapBackendDispatcherHandlerE, nullptr},
    {"Inspector", &_ZN9Inspector26InspectorBackendDispatcher6createERNS_17BackendDispatcherEPNS_33InspectorBackendDispatcherHandlerE, &_ZN9Inspector26InspectorBackendDispatcherC1ERNS_17BackendDispatcherEPNS_33InspectorBackendDispatcherHandlerE, &_ZN9Inspector26InspectorBackendDispatcherC2ERNS_17BackendDispatcherEPNS_33InspectorBackendDispatcherHandlerE, &_ZN9Inspector26InspectorBackendDispatcherD0Ev},
    {"LayerTree", &_ZN9Inspector26LayerTreeBackendDispatcher6createERNS_17BackendDispatcherEPNS_33LayerTreeBackendDispatcherHandlerE, &_ZN9Inspector26LayerTreeBackendDispatcherC1ERNS_17BackendDispatcherEPNS_33LayerTreeBackendDispatcherHandlerE, &_ZN9Inspector26LayerTreeBackendDispatcherC2ERNS_17BackendDispatcherEPNS_33LayerTreeBackendDispatcherHandlerE, &_ZN9Inspector26LayerTreeBackendDispatcherD0Ev},
    {"Network", &_ZN9Inspector24NetworkBackendDispatcher6createERNS_17BackendDispatcherEPNS_31NetworkBackendDispatcherHandlerE, &_ZN9Inspector24NetworkBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31NetworkBackendDispatcherHandlerE, &_ZN9Inspector24NetworkBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31NetworkBackendDispatcherHandlerE, &_ZN9Inspector24NetworkBackendDispatcherD0Ev},
    {"Page", &_ZN9Inspector21PageBackendDispatcher6createERNS_17BackendDispatcherEPNS_28PageBackendDispatcherHandlerE, &_ZN9Inspector21PageBackendDispatcherC1ERNS_17BackendDispatcherEPNS_28PageBackendDispatcherHandlerE, &_ZN9Inspector21PageBackendDispatcherC2ERNS_17BackendDispatcherEPNS_28PageBackendDispatcherHandlerE, nullptr},
    {"Runtime", &_ZN9Inspector24RuntimeBackendDispatcher6createERNS_17BackendDispatcherEPNS_31RuntimeBackendDispatcherHandlerE, &_ZN9Inspector24RuntimeBackendDispatcherC1ERNS_17BackendDispatcherEPNS_31RuntimeBackendDispatcherHandlerE, &_ZN9Inspector24RuntimeBackendDispatcherC2ERNS_17BackendDispatcherEPNS_31RuntimeBackendDispatcherHandlerE, &_ZN9Inspector24RuntimeBackendDispatcherD0Ev},
    {"ScriptProfiler", &_ZN9Inspector31ScriptProfilerBackendDispatcher6createERNS_17BackendDispatcherEPNS_38ScriptProfilerBackendDispatcherHandlerE, &_ZN9Inspector31ScriptProfilerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_38ScriptProfilerBackendDispatcherHandlerE, &_ZN9Inspector31ScriptProfilerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_38ScriptProfilerBackendDispatcherHandlerE, &_ZN9Inspector31ScriptProfilerBackendDispatcherD0Ev},
    {"Target", &_ZN9Inspector23TargetBackendDispatcher6createERNS_17BackendDispatcherEPNS_30TargetBackendDispatcherHandlerE, &_ZN9Inspector23TargetBackendDispatcherC1ERNS_17BackendDispatcherEPNS_30TargetBackendDispatcherHandlerE, &_ZN9Inspector23TargetBackendDispatcherC2ERNS_17BackendDispatcherEPNS_30TargetBackendDispatcherHandlerE, &_ZN9Inspector23TargetBackendDispatcherD0Ev},
    {"Timeline", &_ZN9Inspector25TimelineBackendDispatcher6createERNS_17BackendDispatcherEPNS_32TimelineBackendDispatcherHandlerE, &_ZN9Inspector25TimelineBackendDispatcherC1ERNS_17BackendDispatcherEPNS_32TimelineBackendDispatcherHandlerE, &_ZN9Inspector25TimelineBackendDispatcherC2ERNS_17BackendDispatcherEPNS_32TimelineBackendDispatcherHandlerE, &_ZN9Inspector25TimelineBackendDispatcherD0Ev},
    {"Worker", &_ZN9Inspector23WorkerBackendDispatcher6createERNS_17BackendDispatcherEPNS_30WorkerBackendDispatcherHandlerE, &_ZN9Inspector23WorkerBackendDispatcherC1ERNS_17BackendDispatcherEPNS_30WorkerBackendDispatcherHandlerE, &_ZN9Inspector23WorkerBackendDispatcherC2ERNS_17BackendDispatcherEPNS_30WorkerBackendDispatcherHandlerE, &_ZN9Inspector23WorkerBackendDispatcherD0Ev},
};

const std::uint64_t ExpectedHashes[] = {
    0xae03a38d8ab0a079ULL,
    0x33c875732c35357bULL,
    0xc821a694b4eb8d73ULL,
    0x87dab35beb914b2fULL,
    0xf245938190ebe5edULL,
    0xb3d1db1102272740ULL,
    0x8e603d6b2846ee43ULL,
    0x9aa8bb7c99acdfe6ULL,
    0xee08fec7d9b70c63ULL,
    0x9c3aa20d98e3b55aULL,
    0x7c7a49602e17f251ULL,
    0xc468c064911384bcULL,
    0x6a076f940a8a827cULL,
    0xb2f04367fac0d104ULL,
    0x04aa3972060dd6c6ULL,
    0x4f5ebee67c8c97e8ULL,
};

namespace DomainDispatchDriver {

struct Hash {
    std::uint64_t value = 1469598103934665603ull;
    void Mix(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            value ^= (v >> (8 * i)) & 0xff;
            value *= 1099511628211ull;
        }
    }
    void MixText(const std::u16string& text) {
        Mix(text.size());
        for (char16_t c : text) Mix(c);
    }
};

struct Random {
    std::uint32_t state;
    std::uint32_t operator()(std::uint32_t bound) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state % bound;
    }
};

struct Text {
    std::u16string text;
    bool isNull = false;
    bool is8Bit = true;
};

struct Message {
    bool is8Bit;
    std::u16string text;
};

struct EnumList {
    const int* values;
    int count;
};

struct Param {
    const char* key;
    char getter;
    bool required;
    int enumIndex;
};

struct Out {
    char kind;
    bool conditional;
    int enumIndex;
};

struct Command {
    const char* name;
    const Param* params;
    int paramCount;
    const Out* outs;
    int outCount;
    bool async;
};

struct Domain {
    const char* name;
    const Command* commands;
    int count;
};
inline const char* const EnumNames[] = {
    "ready", "delayed", "active", "canceled", "done", "normal", "reverse", "alternate",
    "alternate-reverse", "none", "forwards", "backwards", "both", "auto", "main", "webkit",
    "user", "user-agent", "author", "inspector", "first-line", "first-letter", "highlight", "marker",
    "before", "after", "selection", "backdrop", "scrollbar", "scrollbar-thumb", "scrollbar-button", "scrollbar-track",
    "scrollbar-track-piece", "scrollbar-corner", "resizer", "focus", "focus-visible", "focus-within", "hover", "target",
    "visited", "inactive", "disabled", "style", "media-rule", "media-import-rule", "media-link-node", "media-style-node",
    "supports-rule", "layer-rule", "layer-import-rule", "container-rule", "style-rule", "rendered", "scrollable", "flex",
    "grid", "event", "observed", "all", "srgb", "display-p3", "canvas-2d", "offscreen-canvas-2d",
    "bitmaprenderer", "webgl", "webgl2", "compute", "render", "fragment", "vertex", "xml",
    "javascript", "network", "console-api", "storage", "appcache", "rendering", "css", "security",
    "content-blocker", "media", "mediasource", "webrtc", "itp-debug", "private-click-measurement", "payment-request", "other",
    "off", "basic", "verbose", "main-frame-navigation", "log", "info", "warning", "error",
    "debug", "dir", "dirxml", "table", "trace", "clear", "startGroup", "startGroupCollapsed",
    "endGroup", "assert", "timing", "profile", "profileEnd", "image", "open", "closed",
    "builtin", "custom", "waiting", "failed", "additions", "removals", "text", "true",
    "false", "mixed", "page", "step", "location", "date", "time", "grammar",
    "spelling", "assertive", "polite", "subtree-modified", "attribute-modified", "node-removed", "animation-frame", "interval",
    "listener", "timeout", "evaluate", "sound", "probe", "global", "with", "closure",
    "catch", "functionName", "globalLexicalEnvironment", "nestedLexical", "full", "partial", "number", "string",
    "array", "null", "jit", "images", "layers", "empty-string", "no-referrer", "no-referrer-when-downgrade",
    "same-origin", "origin", "strict-origin", "origin-when-cross-origin", "strict-origin-when-cross-origin", "unsafe-url", "unknown", "memory-cache",
    "disk-cache", "service-worker", "inspector-override", "low", "medium", "high", "Document", "StyleSheet",
    "Image", "Font", "Script", "XHR", "Fetch", "Ping", "Beacon", "WebSocket",
    "EventSource", "Other", "parser", "script", "request", "response", "General", "AccessControl",
    "Cancellation", "Timeout", "PrivateClickMeasurementDebugModeEnabled", "AuthorAndUserStylesEnabled", "ICECandidateFilteringEnabled", "ITPDebugModeEnabled", "ImagesEnabled", "MediaCaptureRequiresSecureConnection",
    "MockCaptureDevicesEnabled", "NeedsSiteSpecificQuirks", "ScriptEnabled", "ShowDebugBorders", "ShowRepaintCounter", "WebSecurityEnabled", "PrefersReducedMotion", "PrefersContrast",
    "PrefersColorScheme", "NoPreference", "Reduce", "More", "Light", "Dark", "Viewport", "Page",
    "None", "Lax", "Strict", "canvas-bitmaprenderer", "canvas-webgl", "canvas-webgl2", "frontend", "console",
    "auto-capture", "object", "function", "undefined", "boolean", "symbol", "bigint", "node",
    "regexp", "map", "set", "weakmap", "weakset", "iterator", "class", "proxy",
    "weakref", "accessor", "internal", "irrecoverable", "unterminated-literal", "recoverable", "API", "Microtask",
    "worker", "EventDispatch", "ScheduleStyleRecalculation", "RecalculateStyles", "InvalidateLayout", "Layout", "Paint", "Composite",
    "RenderingFrame", "TimerInstall", "TimerRemove", "TimerFire", "EvaluateScript", "TimeStamp", "Time", "TimeEnd",
    "FunctionCall", "ProbeSample", "ConsoleProfile", "RequestAnimationFrame", "CancelAnimationFrame", "FireAnimationFrame", "ObserverCallback", "Screenshot",
    "ScriptProfiler", "Timeline", "CPU", "Memory", "Heap", "Animation", "URL", "DOM",
    "AnimationFrame", "Interval", "Listener", "exception", "CSPViolation", "DebuggerStatement", "Breakpoint", "PauseOnNextStatement",
    "BlackboxedScript", "critical", "non-critical", "uncaught",
};
inline const int EnumValues0[] = {71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87};
inline const int EnumValues1[] = {88, 89, 90};
inline const int EnumValues2[] = {58, 59};
inline const int EnumValues3[] = {131, 132, 133};
inline const int EnumValues4[] = {134, 135, 136, 137};
inline const int EnumValues5[] = {188, 189};
inline const int EnumValues6[] = {190, 191, 192, 193};
inline const int EnumValues7[] = {194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205};
inline const int EnumValues8[] = {206, 207, 208};
inline const int EnumValues9[] = {209, 210, 211, 212, 213};
inline const int EnumValues10[] = {214, 215};
inline const int EnumValues11[] = {9, 243, 244, 245};
inline const EnumList EnumLists[] = {
    {EnumValues0, 17},
    {EnumValues1, 3},
    {EnumValues2, 2},
    {EnumValues3, 3},
    {EnumValues4, 4},
    {EnumValues5, 2},
    {EnumValues6, 4},
    {EnumValues7, 12},
    {EnumValues8, 3},
    {EnumValues9, 5},
    {EnumValues10, 2},
    {EnumValues11, 4},
};
inline const Param P_Animation_requestEffectTarget[] = {{"animationId", 'S', true, -1}};
inline const Out O_Animation_requestEffectTarget[] = {{'O', false, -1}};
inline const Param P_Animation_resolveAnimation[] = {{"animationId", 'S', true, -1}, {"objectGroup", 'S', false, -1}};
inline const Out O_Animation_resolveAnimation[] = {{'O', false, -1}};
inline const Command C_Animation[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"requestEffectTarget", P_Animation_requestEffectTarget, 1, O_Animation_requestEffectTarget, 1, false},
    {"resolveAnimation", P_Animation_resolveAnimation, 2, O_Animation_resolveAnimation, 1, false},
    {"startTracking", nullptr, 0, nullptr, 0, false},
    {"stopTracking", nullptr, 0, nullptr, 0, false},
};
inline const Out O_ApplicationCache_getFramesWithManifests[] = {{'A', false, -1}};
inline const Param P_ApplicationCache_getManifestForFrame[] = {{"frameId", 'S', true, -1}};
inline const Out O_ApplicationCache_getManifestForFrame[] = {{'S', false, -1}};
inline const Param P_ApplicationCache_getApplicationCacheForFrame[] = {{"frameId", 'S', true, -1}};
inline const Out O_ApplicationCache_getApplicationCacheForFrame[] = {{'O', false, -1}};
inline const Command C_ApplicationCache[] = {
    {"getFramesWithManifests", nullptr, 0, O_ApplicationCache_getFramesWithManifests, 1, false},
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"getManifestForFrame", P_ApplicationCache_getManifestForFrame, 1, O_ApplicationCache_getManifestForFrame, 1, false},
    {"getApplicationCacheForFrame", P_ApplicationCache_getApplicationCacheForFrame, 1, O_ApplicationCache_getApplicationCacheForFrame, 1, false},
};
inline const Param P_Audit_setup[] = {{"contextId", 'I', false, -1}};
inline const Param P_Audit_run[] = {{"test", 'S', true, -1}, {"contextId", 'I', false, -1}};
inline const Out O_Audit_run[] = {{'O', false, -1}, {'b', true, -1}};
inline const Command C_Audit[] = {
    {"setup", P_Audit_setup, 1, nullptr, 0, false},
    {"run", P_Audit_run, 2, O_Audit_run, 2, false},
    {"teardown", nullptr, 0, nullptr, 0, false},
};
inline const Command C_Browser[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
};
inline const Param P_Canvas_requestNode[] = {{"canvasId", 'S', true, -1}};
inline const Out O_Canvas_requestNode[] = {{'I', false, -1}};
inline const Param P_Canvas_requestContent[] = {{"canvasId", 'S', true, -1}};
inline const Out O_Canvas_requestContent[] = {{'S', false, -1}};
inline const Param P_Canvas_requestClientNodes[] = {{"canvasId", 'S', true, -1}};
inline const Out O_Canvas_requestClientNodes[] = {{'A', false, -1}};
inline const Param P_Canvas_resolveContext[] = {{"canvasId", 'S', true, -1}, {"objectGroup", 'S', false, -1}};
inline const Out O_Canvas_resolveContext[] = {{'O', false, -1}};
inline const Param P_Canvas_setRecordingAutoCaptureFrameCount[] = {{"count", 'I', true, -1}};
inline const Param P_Canvas_startRecording[] = {{"canvasId", 'S', true, -1}, {"frameCount", 'I', false, -1}, {"memoryLimit", 'I', false, -1}};
inline const Param P_Canvas_stopRecording[] = {{"canvasId", 'S', true, -1}};
inline const Command C_Canvas[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"requestNode", P_Canvas_requestNode, 1, O_Canvas_requestNode, 1, false},
    {"requestContent", P_Canvas_requestContent, 1, O_Canvas_requestContent, 1, false},
    {"requestClientNodes", P_Canvas_requestClientNodes, 1, O_Canvas_requestClientNodes, 1, false},
    {"resolveContext", P_Canvas_resolveContext, 2, O_Canvas_resolveContext, 1, false},
    {"setRecordingAutoCaptureFrameCount", P_Canvas_setRecordingAutoCaptureFrameCount, 1, nullptr, 0, false},
    {"startRecording", P_Canvas_startRecording, 3, nullptr, 0, false},
    {"stopRecording", P_Canvas_stopRecording, 1, nullptr, 0, false},
};
inline const Out O_Console_getLoggingChannels[] = {{'A', false, -1}};
inline const Param P_Console_setLoggingChannelLevel[] = {{"source", 'S', true, 0}, {"level", 'S', true, 1}};
inline const Command C_Console[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"clearMessages", nullptr, 0, nullptr, 0, false},
    {"getLoggingChannels", nullptr, 0, O_Console_getLoggingChannels, 1, false},
    {"setLoggingChannelLevel", P_Console_setLoggingChannelLevel, 2, nullptr, 0, false},
};
inline const Param P_CSS_getMatchedStylesForNode[] = {{"nodeId", 'I', true, -1}, {"includePseudo", 'B', false, -1}, {"includeInherited", 'B', false, -1}};
inline const Out O_CSS_getMatchedStylesForNode[] = {{'a', true, -1}, {'a', true, -1}, {'a', true, -1}};
inline const Param P_CSS_getInlineStylesForNode[] = {{"nodeId", 'I', true, -1}};
inline const Out O_CSS_getInlineStylesForNode[] = {{'o', true, -1}, {'o', true, -1}};
inline const Param P_CSS_getComputedStyleForNode[] = {{"nodeId", 'I', true, -1}};
inline const Out O_CSS_getComputedStyleForNode[] = {{'A', false, -1}};
inline const Param P_CSS_getFontDataForNode[] = {{"nodeId", 'I', true, -1}};
inline const Out O_CSS_getFontDataForNode[] = {{'O', false, -1}};
inline const Out O_CSS_getAllStyleSheets[] = {{'A', false, -1}};
inline const Param P_CSS_getStyleSheet[] = {{"styleSheetId", 'S', true, -1}};
inline const Out O_CSS_getStyleSheet[] = {{'O', false, -1}};
inline const Param P_CSS_getStyleSheetText[] = {{"styleSheetId", 'S', true, -1}};
inline const Out O_CSS_getStyleSheetText[] = {{'S', false, -1}};
inline const Param P_CSS_setStyleSheetText[] = {{"styleSheetId", 'S', true, -1}, {"text", 'S', true, -1}};
inline const Param P_CSS_setStyleText[] = {{"styleId", 'O', true, -1}, {"text", 'S', true, -1}};
inline const Out O_CSS_setStyleText[] = {{'O', false, -1}};
inline const Param P_CSS_setRuleSelector[] = {{"ruleId", 'O', true, -1}, {"selector", 'S', true, -1}};
inline const Out O_CSS_setRuleSelector[] = {{'O', false, -1}};
inline const Param P_CSS_setGroupingHeaderText[] = {{"ruleId", 'O', true, -1}, {"headerText", 'S', true, -1}};
inline const Out O_CSS_setGroupingHeaderText[] = {{'O', false, -1}};
inline const Param P_CSS_createStyleSheet[] = {{"frameId", 'S', true, -1}};
inline const Out O_CSS_createStyleSheet[] = {{'S', false, -1}};
inline const Param P_CSS_addRule[] = {{"styleSheetId", 'S', true, -1}, {"selector", 'S', true, -1}};
inline const Out O_CSS_addRule[] = {{'O', false, -1}};
inline const Out O_CSS_getSupportedCSSProperties[] = {{'A', false, -1}};
inline const Out O_CSS_getSupportedSystemFontFamilyNames[] = {{'A', false, -1}};
inline const Param P_CSS_forcePseudoState[] = {{"nodeId", 'I', true, -1}, {"forcedPseudoClasses", 'A', true, -1}};
inline const Param P_CSS_setLayoutContextTypeChangedMode[] = {{"mode", 'S', true, 2}};
inline const Command C_CSS[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"getMatchedStylesForNode", P_CSS_getMatchedStylesForNode, 3, O_CSS_getMatchedStylesForNode, 3, false},
    {"getInlineStylesForNode", P_CSS_getInlineStylesForNode, 1, O_CSS_getInlineStylesForNode, 2, false},
    {"getComputedStyleForNode", P_CSS_getComputedStyleForNode, 1, O_CSS_getComputedStyleForNode, 1, false},
    {"getFontDataForNode", P_CSS_getFontDataForNode, 1, O_CSS_getFontDataForNode, 1, false},
    {"getAllStyleSheets", nullptr, 0, O_CSS_getAllStyleSheets, 1, false},
    {"getStyleSheet", P_CSS_getStyleSheet, 1, O_CSS_getStyleSheet, 1, false},
    {"getStyleSheetText", P_CSS_getStyleSheetText, 1, O_CSS_getStyleSheetText, 1, false},
    {"setStyleSheetText", P_CSS_setStyleSheetText, 2, nullptr, 0, false},
    {"setStyleText", P_CSS_setStyleText, 2, O_CSS_setStyleText, 1, false},
    {"setRuleSelector", P_CSS_setRuleSelector, 2, O_CSS_setRuleSelector, 1, false},
    {"setGroupingHeaderText", P_CSS_setGroupingHeaderText, 2, O_CSS_setGroupingHeaderText, 1, false},
    {"createStyleSheet", P_CSS_createStyleSheet, 1, O_CSS_createStyleSheet, 1, false},
    {"addRule", P_CSS_addRule, 2, O_CSS_addRule, 1, false},
    {"getSupportedCSSProperties", nullptr, 0, O_CSS_getSupportedCSSProperties, 1, false},
    {"getSupportedSystemFontFamilyNames", nullptr, 0, O_CSS_getSupportedSystemFontFamilyNames, 1, false},
    {"forcePseudoState", P_CSS_forcePseudoState, 2, nullptr, 0, false},
    {"setLayoutContextTypeChangedMode", P_CSS_setLayoutContextTypeChangedMode, 1, nullptr, 0, false},
};
inline const Param P_Database_getDatabaseTableNames[] = {{"databaseId", 'S', true, -1}};
inline const Out O_Database_getDatabaseTableNames[] = {{'A', false, -1}};
inline const Param P_Database_executeSQL[] = {{"databaseId", 'S', true, -1}, {"query", 'S', true, -1}};
inline const Command C_Database[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"getDatabaseTableNames", P_Database_getDatabaseTableNames, 1, O_Database_getDatabaseTableNames, 1, false},
    {"executeSQL", P_Database_executeSQL, 2, nullptr, 0, true},
};
inline const Param P_Debugger_setAsyncStackTraceDepth[] = {{"depth", 'I', true, -1}};
inline const Param P_Debugger_setBreakpointsActive[] = {{"active", 'B', true, -1}};
inline const Param P_Debugger_setBreakpointByUrl[] = {{"lineNumber", 'I', true, -1}, {"url", 'S', false, -1}, {"urlRegex", 'S', false, -1}, {"columnNumber", 'I', false, -1}, {"options", 'O', false, -1}};
inline const Out O_Debugger_setBreakpointByUrl[] = {{'S', false, -1}, {'A', false, -1}};
inline const Param P_Debugger_setBreakpoint[] = {{"location", 'O', true, -1}, {"options", 'O', false, -1}};
inline const Out O_Debugger_setBreakpoint[] = {{'S', false, -1}, {'O', false, -1}};
inline const Param P_Debugger_removeBreakpoint[] = {{"breakpointId", 'S', true, -1}};
inline const Param P_Debugger_addSymbolicBreakpoint[] = {{"symbol", 'S', true, -1}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}, {"options", 'O', false, -1}};
inline const Param P_Debugger_removeSymbolicBreakpoint[] = {{"symbol", 'S', true, -1}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}};
inline const Param P_Debugger_continueToLocation[] = {{"location", 'O', true, -1}};
inline const Param P_Debugger_searchInContent[] = {{"scriptId", 'S', true, -1}, {"query", 'S', true, -1}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}};
inline const Out O_Debugger_searchInContent[] = {{'A', false, -1}};
inline const Param P_Debugger_getScriptSource[] = {{"scriptId", 'S', true, -1}};
inline const Out O_Debugger_getScriptSource[] = {{'S', false, -1}};
inline const Param P_Debugger_getFunctionDetails[] = {{"functionId", 'S', true, -1}};
inline const Out O_Debugger_getFunctionDetails[] = {{'O', false, -1}};
inline const Param P_Debugger_getBreakpointLocations[] = {{"start", 'O', true, -1}, {"end", 'O', true, -1}};
inline const Out O_Debugger_getBreakpointLocations[] = {{'A', false, -1}};
inline const Param P_Debugger_setPauseOnDebuggerStatements[] = {{"enabled", 'B', true, -1}, {"options", 'O', false, -1}};
inline const Param P_Debugger_setPauseOnExceptions[] = {{"state", 'S', true, -1}, {"options", 'O', false, -1}};
inline const Param P_Debugger_setPauseOnAssertions[] = {{"enabled", 'B', true, -1}, {"options", 'O', false, -1}};
inline const Param P_Debugger_setPauseOnMicrotasks[] = {{"enabled", 'B', true, -1}, {"options", 'O', false, -1}};
inline const Param P_Debugger_setPauseForInternalScripts[] = {{"shouldPause", 'B', true, -1}};
inline const Param P_Debugger_evaluateOnCallFrame[] = {{"callFrameId", 'S', true, -1}, {"expression", 'S', true, -1}, {"objectGroup", 'S', false, -1}, {"includeCommandLineAPI", 'B', false, -1}, {"doNotPauseOnExceptionsAndMuteConsole", 'B', false, -1}, {"returnByValue", 'B', false, -1}, {"generatePreview", 'B', false, -1}, {"saveResult", 'B', false, -1}, {"emulateUserGesture", 'B', false, -1}};
inline const Out O_Debugger_evaluateOnCallFrame[] = {{'O', false, -1}, {'b', true, -1}, {'i', true, -1}};
inline const Param P_Debugger_setShouldBlackboxURL[] = {{"url", 'S', true, -1}, {"shouldBlackbox", 'B', true, -1}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}};
inline const Param P_Debugger_setBlackboxBreakpointEvaluations[] = {{"blackboxBreakpointEvaluations", 'B', true, -1}};
inline const Command C_Debugger[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"setAsyncStackTraceDepth", P_Debugger_setAsyncStackTraceDepth, 1, nullptr, 0, false},
    {"setBreakpointsActive", P_Debugger_setBreakpointsActive, 1, nullptr, 0, false},
    {"setBreakpointByUrl", P_Debugger_setBreakpointByUrl, 5, O_Debugger_setBreakpointByUrl, 2, false},
    {"setBreakpoint", P_Debugger_setBreakpoint, 2, O_Debugger_setBreakpoint, 2, false},
    {"removeBreakpoint", P_Debugger_removeBreakpoint, 1, nullptr, 0, false},
    {"addSymbolicBreakpoint", P_Debugger_addSymbolicBreakpoint, 4, nullptr, 0, false},
    {"removeSymbolicBreakpoint", P_Debugger_removeSymbolicBreakpoint, 3, nullptr, 0, false},
    {"continueUntilNextRunLoop", nullptr, 0, nullptr, 0, false},
    {"continueToLocation", P_Debugger_continueToLocation, 1, nullptr, 0, false},
    {"stepNext", nullptr, 0, nullptr, 0, false},
    {"stepOver", nullptr, 0, nullptr, 0, false},
    {"stepInto", nullptr, 0, nullptr, 0, false},
    {"stepOut", nullptr, 0, nullptr, 0, false},
    {"pause", nullptr, 0, nullptr, 0, false},
    {"resume", nullptr, 0, nullptr, 0, false},
    {"searchInContent", P_Debugger_searchInContent, 4, O_Debugger_searchInContent, 1, false},
    {"getScriptSource", P_Debugger_getScriptSource, 1, O_Debugger_getScriptSource, 1, false},
    {"getFunctionDetails", P_Debugger_getFunctionDetails, 1, O_Debugger_getFunctionDetails, 1, false},
    {"getBreakpointLocations", P_Debugger_getBreakpointLocations, 2, O_Debugger_getBreakpointLocations, 1, false},
    {"setPauseOnDebuggerStatements", P_Debugger_setPauseOnDebuggerStatements, 2, nullptr, 0, false},
    {"setPauseOnExceptions", P_Debugger_setPauseOnExceptions, 2, nullptr, 0, false},
    {"setPauseOnAssertions", P_Debugger_setPauseOnAssertions, 2, nullptr, 0, false},
    {"setPauseOnMicrotasks", P_Debugger_setPauseOnMicrotasks, 2, nullptr, 0, false},
    {"setPauseForInternalScripts", P_Debugger_setPauseForInternalScripts, 1, nullptr, 0, false},
    {"evaluateOnCallFrame", P_Debugger_evaluateOnCallFrame, 9, O_Debugger_evaluateOnCallFrame, 3, false},
    {"setShouldBlackboxURL", P_Debugger_setShouldBlackboxURL, 4, nullptr, 0, false},
    {"setBlackboxBreakpointEvaluations", P_Debugger_setBlackboxBreakpointEvaluations, 1, nullptr, 0, false},
};
inline const Out O_DOM_getDocument[] = {{'O', false, -1}};
inline const Param P_DOM_requestChildNodes[] = {{"nodeId", 'I', true, -1}, {"depth", 'I', false, -1}};
inline const Param P_DOM_querySelector[] = {{"nodeId", 'I', true, -1}, {"selector", 'S', true, -1}};
inline const Out O_DOM_querySelector[] = {{'i', true, -1}};
inline const Param P_DOM_querySelectorAll[] = {{"nodeId", 'I', true, -1}, {"selector", 'S', true, -1}};
inline const Out O_DOM_querySelectorAll[] = {{'A', false, -1}};
inline const Param P_DOM_setNodeName[] = {{"nodeId", 'I', true, -1}, {"name", 'S', true, -1}};
inline const Out O_DOM_setNodeName[] = {{'I', false, -1}};
inline const Param P_DOM_setNodeValue[] = {{"nodeId", 'I', true, -1}, {"value", 'S', true, -1}};
inline const Param P_DOM_removeNode[] = {{"nodeId", 'I', true, -1}};
inline const Param P_DOM_setAttributeValue[] = {{"nodeId", 'I', true, -1}, {"name", 'S', true, -1}, {"value", 'S', true, -1}};
inline const Param P_DOM_setAttributesAsText[] = {{"nodeId", 'I', true, -1}, {"text", 'S', true, -1}, {"name", 'S', false, -1}};
inline const Param P_DOM_removeAttribute[] = {{"nodeId", 'I', true, -1}, {"name", 'S', true, -1}};
inline const Out O_DOM_getSupportedEventNames[] = {{'A', false, -1}};
inline const Param P_DOM_getEventListenersForNode[] = {{"nodeId", 'I', true, -1}, {"includeAncestors", 'B', false, -1}};
inline const Out O_DOM_getEventListenersForNode[] = {{'A', false, -1}};
inline const Param P_DOM_setEventListenerDisabled[] = {{"eventListenerId", 'I', true, -1}, {"disabled", 'B', true, -1}};
inline const Param P_DOM_setBreakpointForEventListener[] = {{"eventListenerId", 'I', true, -1}, {"options", 'O', false, -1}};
inline const Param P_DOM_removeBreakpointForEventListener[] = {{"eventListenerId", 'I', true, -1}};
inline const Param P_DOM_getAccessibilityPropertiesForNode[] = {{"nodeId", 'I', true, -1}};
inline const Out O_DOM_getAccessibilityPropertiesForNode[] = {{'O', false, -1}};
inline const Param P_DOM_getOuterHTML[] = {{"nodeId", 'I', true, -1}};
inline const Out O_DOM_getOuterHTML[] = {{'S', false, -1}};
inline const Param P_DOM_setOuterHTML[] = {{"nodeId", 'I', true, -1}, {"outerHTML", 'S', true, -1}};
inline const Param P_DOM_insertAdjacentHTML[] = {{"nodeId", 'I', true, -1}, {"position", 'S', true, -1}, {"html", 'S', true, -1}};
inline const Param P_DOM_performSearch[] = {{"query", 'S', true, -1}, {"nodeIds", 'A', false, -1}, {"caseSensitive", 'B', false, -1}};
inline const Out O_DOM_performSearch[] = {{'S', false, -1}, {'I', false, -1}};
inline const Param P_DOM_getSearchResults[] = {{"searchId", 'S', true, -1}, {"fromIndex", 'I', true, -1}, {"toIndex", 'I', true, -1}};
inline const Out O_DOM_getSearchResults[] = {{'A', false, -1}};
inline const Param P_DOM_discardSearchResults[] = {{"searchId", 'S', true, -1}};
inline const Param P_DOM_requestNode[] = {{"objectId", 'S', true, -1}};
inline const Out O_DOM_requestNode[] = {{'I', false, -1}};
inline const Param P_DOM_setInspectModeEnabled[] = {{"enabled", 'B', true, -1}, {"highlightConfig", 'O', false, -1}, {"gridOverlayConfig", 'O', false, -1}, {"flexOverlayConfig", 'O', false, -1}, {"showRulers", 'B', false, -1}};
inline const Param P_DOM_highlightRect[] = {{"x", 'I', true, -1}, {"y", 'I', true, -1}, {"width", 'I', true, -1}, {"height", 'I', true, -1}, {"color", 'O', false, -1}, {"outlineColor", 'O', false, -1}, {"usePageCoordinates", 'B', false, -1}};
inline const Param P_DOM_highlightQuad[] = {{"quad", 'A', true, -1}, {"color", 'O', false, -1}, {"outlineColor", 'O', false, -1}, {"usePageCoordinates", 'B', false, -1}};
inline const Param P_DOM_highlightSelector[] = {{"selectorString", 'S', true, -1}, {"frameId", 'S', false, -1}, {"highlightConfig", 'O', true, -1}, {"gridOverlayConfig", 'O', false, -1}, {"flexOverlayConfig", 'O', false, -1}, {"showRulers", 'B', false, -1}};
inline const Param P_DOM_highlightNode[] = {{"nodeId", 'I', false, -1}, {"objectId", 'S', false, -1}, {"highlightConfig", 'O', true, -1}, {"gridOverlayConfig", 'O', false, -1}, {"flexOverlayConfig", 'O', false, -1}, {"showRulers", 'B', false, -1}};
inline const Param P_DOM_highlightNodeList[] = {{"nodeIds", 'A', true, -1}, {"highlightConfig", 'O', true, -1}, {"gridOverlayConfig", 'O', false, -1}, {"flexOverlayConfig", 'O', false, -1}, {"showRulers", 'B', false, -1}};
inline const Param P_DOM_highlightFrame[] = {{"frameId", 'S', true, -1}, {"contentColor", 'O', false, -1}, {"contentOutlineColor", 'O', false, -1}};
inline const Param P_DOM_showGridOverlay[] = {{"nodeId", 'I', true, -1}, {"gridOverlayConfig", 'O', true, -1}};
inline const Param P_DOM_hideGridOverlay[] = {{"nodeId", 'I', false, -1}};
inline const Param P_DOM_showFlexOverlay[] = {{"nodeId", 'I', true, -1}, {"flexOverlayConfig", 'O', true, -1}};
inline const Param P_DOM_hideFlexOverlay[] = {{"nodeId", 'I', false, -1}};
inline const Param P_DOM_pushNodeByPathToFrontend[] = {{"path", 'S', true, -1}};
inline const Out O_DOM_pushNodeByPathToFrontend[] = {{'I', false, -1}};
inline const Param P_DOM_resolveNode[] = {{"nodeId", 'I', true, -1}, {"objectGroup", 'S', false, -1}};
inline const Out O_DOM_resolveNode[] = {{'O', false, -1}};
inline const Param P_DOM_getAttributes[] = {{"nodeId", 'I', true, -1}};
inline const Out O_DOM_getAttributes[] = {{'A', false, -1}};
inline const Param P_DOM_moveTo[] = {{"nodeId", 'I', true, -1}, {"targetNodeId", 'I', true, -1}, {"insertBeforeNodeId", 'I', false, -1}};
inline const Out O_DOM_moveTo[] = {{'I', false, -1}};
inline const Param P_DOM_focus[] = {{"nodeId", 'I', true, -1}};
inline const Param P_DOM_setInspectedNode[] = {{"nodeId", 'I', true, -1}};
inline const Param P_DOM_setAllowEditingUserAgentShadowTrees[] = {{"allow", 'B', true, -1}};
inline const Command C_DOM[] = {
    {"getDocument", nullptr, 0, O_DOM_getDocument, 1, false},
    {"requestChildNodes", P_DOM_requestChildNodes, 2, nullptr, 0, false},
    {"querySelector", P_DOM_querySelector, 2, O_DOM_querySelector, 1, false},
    {"querySelectorAll", P_DOM_querySelectorAll, 2, O_DOM_querySelectorAll, 1, false},
    {"setNodeName", P_DOM_setNodeName, 2, O_DOM_setNodeName, 1, false},
    {"setNodeValue", P_DOM_setNodeValue, 2, nullptr, 0, false},
    {"removeNode", P_DOM_removeNode, 1, nullptr, 0, false},
    {"setAttributeValue", P_DOM_setAttributeValue, 3, nullptr, 0, false},
    {"setAttributesAsText", P_DOM_setAttributesAsText, 3, nullptr, 0, false},
    {"removeAttribute", P_DOM_removeAttribute, 2, nullptr, 0, false},
    {"getSupportedEventNames", nullptr, 0, O_DOM_getSupportedEventNames, 1, false},
    {"getEventListenersForNode", P_DOM_getEventListenersForNode, 2, O_DOM_getEventListenersForNode, 1, false},
    {"setEventListenerDisabled", P_DOM_setEventListenerDisabled, 2, nullptr, 0, false},
    {"setBreakpointForEventListener", P_DOM_setBreakpointForEventListener, 2, nullptr, 0, false},
    {"removeBreakpointForEventListener", P_DOM_removeBreakpointForEventListener, 1, nullptr, 0, false},
    {"getAccessibilityPropertiesForNode", P_DOM_getAccessibilityPropertiesForNode, 1, O_DOM_getAccessibilityPropertiesForNode, 1, false},
    {"getOuterHTML", P_DOM_getOuterHTML, 1, O_DOM_getOuterHTML, 1, false},
    {"setOuterHTML", P_DOM_setOuterHTML, 2, nullptr, 0, false},
    {"insertAdjacentHTML", P_DOM_insertAdjacentHTML, 3, nullptr, 0, false},
    {"performSearch", P_DOM_performSearch, 3, O_DOM_performSearch, 2, false},
    {"getSearchResults", P_DOM_getSearchResults, 3, O_DOM_getSearchResults, 1, false},
    {"discardSearchResults", P_DOM_discardSearchResults, 1, nullptr, 0, false},
    {"requestNode", P_DOM_requestNode, 1, O_DOM_requestNode, 1, false},
    {"setInspectModeEnabled", P_DOM_setInspectModeEnabled, 5, nullptr, 0, false},
    {"highlightRect", P_DOM_highlightRect, 7, nullptr, 0, false},
    {"highlightQuad", P_DOM_highlightQuad, 4, nullptr, 0, false},
    {"highlightSelector", P_DOM_highlightSelector, 6, nullptr, 0, false},
    {"highlightNode", P_DOM_highlightNode, 6, nullptr, 0, false},
    {"highlightNodeList", P_DOM_highlightNodeList, 5, nullptr, 0, false},
    {"hideHighlight", nullptr, 0, nullptr, 0, false},
    {"highlightFrame", P_DOM_highlightFrame, 3, nullptr, 0, false},
    {"showGridOverlay", P_DOM_showGridOverlay, 2, nullptr, 0, false},
    {"hideGridOverlay", P_DOM_hideGridOverlay, 1, nullptr, 0, false},
    {"showFlexOverlay", P_DOM_showFlexOverlay, 2, nullptr, 0, false},
    {"hideFlexOverlay", P_DOM_hideFlexOverlay, 1, nullptr, 0, false},
    {"pushNodeByPathToFrontend", P_DOM_pushNodeByPathToFrontend, 1, O_DOM_pushNodeByPathToFrontend, 1, false},
    {"resolveNode", P_DOM_resolveNode, 2, O_DOM_resolveNode, 1, false},
    {"getAttributes", P_DOM_getAttributes, 1, O_DOM_getAttributes, 1, false},
    {"moveTo", P_DOM_moveTo, 3, O_DOM_moveTo, 1, false},
    {"undo", nullptr, 0, nullptr, 0, false},
    {"redo", nullptr, 0, nullptr, 0, false},
    {"markUndoableState", nullptr, 0, nullptr, 0, false},
    {"focus", P_DOM_focus, 1, nullptr, 0, false},
    {"setInspectedNode", P_DOM_setInspectedNode, 1, nullptr, 0, false},
    {"setAllowEditingUserAgentShadowTrees", P_DOM_setAllowEditingUserAgentShadowTrees, 1, nullptr, 0, false},
};
inline const Param P_DOMDebugger_setDOMBreakpoint[] = {{"nodeId", 'I', true, -1}, {"type", 'S', true, 3}, {"options", 'O', false, -1}};
inline const Param P_DOMDebugger_removeDOMBreakpoint[] = {{"nodeId", 'I', true, -1}, {"type", 'S', true, 3}};
inline const Param P_DOMDebugger_setEventBreakpoint[] = {{"breakpointType", 'S', true, 4}, {"eventName", 'S', false, -1}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}, {"options", 'O', false, -1}};
inline const Param P_DOMDebugger_removeEventBreakpoint[] = {{"breakpointType", 'S', true, 4}, {"eventName", 'S', false, -1}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}};
inline const Param P_DOMDebugger_setURLBreakpoint[] = {{"url", 'S', true, -1}, {"isRegex", 'B', false, -1}, {"options", 'O', false, -1}};
inline const Param P_DOMDebugger_removeURLBreakpoint[] = {{"url", 'S', true, -1}, {"isRegex", 'B', false, -1}};
inline const Command C_DOMDebugger[] = {
    {"setDOMBreakpoint", P_DOMDebugger_setDOMBreakpoint, 3, nullptr, 0, false},
    {"removeDOMBreakpoint", P_DOMDebugger_removeDOMBreakpoint, 2, nullptr, 0, false},
    {"setEventBreakpoint", P_DOMDebugger_setEventBreakpoint, 5, nullptr, 0, false},
    {"removeEventBreakpoint", P_DOMDebugger_removeEventBreakpoint, 4, nullptr, 0, false},
    {"setURLBreakpoint", P_DOMDebugger_setURLBreakpoint, 3, nullptr, 0, false},
    {"removeURLBreakpoint", P_DOMDebugger_removeURLBreakpoint, 2, nullptr, 0, false},
};
inline const Param P_DOMStorage_getDOMStorageItems[] = {{"storageId", 'O', true, -1}};
inline const Out O_DOMStorage_getDOMStorageItems[] = {{'A', false, -1}};
inline const Param P_DOMStorage_setDOMStorageItem[] = {{"storageId", 'O', true, -1}, {"key", 'S', true, -1}, {"value", 'S', true, -1}};
inline const Param P_DOMStorage_removeDOMStorageItem[] = {{"storageId", 'O', true, -1}, {"key", 'S', true, -1}};
inline const Param P_DOMStorage_clearDOMStorageItems[] = {{"storageId", 'O', true, -1}};
inline const Command C_DOMStorage[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"getDOMStorageItems", P_DOMStorage_getDOMStorageItems, 1, O_DOMStorage_getDOMStorageItems, 1, false},
    {"setDOMStorageItem", P_DOMStorage_setDOMStorageItem, 3, nullptr, 0, false},
    {"removeDOMStorageItem", P_DOMStorage_removeDOMStorageItem, 2, nullptr, 0, false},
    {"clearDOMStorageItems", P_DOMStorage_clearDOMStorageItems, 1, nullptr, 0, false},
};
inline const Out O_Heap_snapshot[] = {{'D', false, -1}, {'S', false, -1}};
inline const Param P_Heap_getPreview[] = {{"heapObjectId", 'I', true, -1}};
inline const Out O_Heap_getPreview[] = {{'S', true, -1}, {'o', true, -1}, {'o', true, -1}};
inline const Param P_Heap_getRemoteObject[] = {{"heapObjectId", 'I', true, -1}, {"objectGroup", 'S', false, -1}};
inline const Out O_Heap_getRemoteObject[] = {{'O', false, -1}};
inline const Command C_Heap[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"gc", nullptr, 0, nullptr, 0, false},
    {"snapshot", nullptr, 0, O_Heap_snapshot, 2, false},
    {"startTracking", nullptr, 0, nullptr, 0, false},
    {"stopTracking", nullptr, 0, nullptr, 0, false},
    {"getPreview", P_Heap_getPreview, 1, O_Heap_getPreview, 3, false},
    {"getRemoteObject", P_Heap_getRemoteObject, 2, O_Heap_getRemoteObject, 1, false},
};
inline const Command C_Inspector[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"initialized", nullptr, 0, nullptr, 0, false},
};
inline const Param P_LayerTree_layersForNode[] = {{"nodeId", 'I', true, -1}};
inline const Out O_LayerTree_layersForNode[] = {{'A', false, -1}};
inline const Param P_LayerTree_reasonsForCompositingLayer[] = {{"layerId", 'S', true, -1}};
inline const Out O_LayerTree_reasonsForCompositingLayer[] = {{'O', false, -1}};
inline const Command C_LayerTree[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"layersForNode", P_LayerTree_layersForNode, 1, O_LayerTree_layersForNode, 1, false},
    {"reasonsForCompositingLayer", P_LayerTree_reasonsForCompositingLayer, 1, O_LayerTree_reasonsForCompositingLayer, 1, false},
};
inline const Param P_Network_setExtraHTTPHeaders[] = {{"headers", 'O', true, -1}};
inline const Param P_Network_getResponseBody[] = {{"requestId", 'S', true, -1}};
inline const Out O_Network_getResponseBody[] = {{'S', false, -1}, {'B', false, -1}};
inline const Param P_Network_setResourceCachingDisabled[] = {{"disabled", 'B', true, -1}};
inline const Param P_Network_loadResource[] = {{"frameId", 'S', true, -1}, {"url", 'S', true, -1}};
inline const Param P_Network_getSerializedCertificate[] = {{"requestId", 'S', true, -1}};
inline const Out O_Network_getSerializedCertificate[] = {{'S', false, -1}};
inline const Param P_Network_resolveWebSocket[] = {{"requestId", 'S', true, -1}, {"objectGroup", 'S', false, -1}};
inline const Out O_Network_resolveWebSocket[] = {{'O', false, -1}};
inline const Param P_Network_setInterceptionEnabled[] = {{"enabled", 'B', true, -1}};
inline const Param P_Network_addInterception[] = {{"url", 'S', true, -1}, {"stage", 'S', true, 5}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}};
inline const Param P_Network_removeInterception[] = {{"url", 'S', true, -1}, {"stage", 'S', true, 5}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}};
inline const Param P_Network_interceptContinue[] = {{"requestId", 'S', true, -1}, {"stage", 'S', true, 5}};
inline const Param P_Network_interceptWithRequest[] = {{"requestId", 'S', true, -1}, {"url", 'S', false, -1}, {"method", 'S', false, -1}, {"headers", 'O', false, -1}, {"postData", 'S', false, -1}};
inline const Param P_Network_interceptWithResponse[] = {{"requestId", 'S', true, -1}, {"content", 'S', true, -1}, {"base64Encoded", 'B', true, -1}, {"mimeType", 'S', false, -1}, {"status", 'I', false, -1}, {"statusText", 'S', false, -1}, {"headers", 'O', false, -1}};
inline const Param P_Network_interceptRequestWithResponse[] = {{"requestId", 'S', true, -1}, {"content", 'S', true, -1}, {"base64Encoded", 'B', true, -1}, {"mimeType", 'S', true, -1}, {"status", 'I', true, -1}, {"statusText", 'S', true, -1}, {"headers", 'O', true, -1}};
inline const Param P_Network_interceptRequestWithError[] = {{"requestId", 'S', true, -1}, {"errorType", 'S', true, 6}};
inline const Command C_Network[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"setExtraHTTPHeaders", P_Network_setExtraHTTPHeaders, 1, nullptr, 0, false},
    {"getResponseBody", P_Network_getResponseBody, 1, O_Network_getResponseBody, 2, false},
    {"setResourceCachingDisabled", P_Network_setResourceCachingDisabled, 1, nullptr, 0, false},
    {"loadResource", P_Network_loadResource, 2, nullptr, 0, true},
    {"getSerializedCertificate", P_Network_getSerializedCertificate, 1, O_Network_getSerializedCertificate, 1, false},
    {"resolveWebSocket", P_Network_resolveWebSocket, 2, O_Network_resolveWebSocket, 1, false},
    {"setInterceptionEnabled", P_Network_setInterceptionEnabled, 1, nullptr, 0, false},
    {"addInterception", P_Network_addInterception, 4, nullptr, 0, false},
    {"removeInterception", P_Network_removeInterception, 4, nullptr, 0, false},
    {"interceptContinue", P_Network_interceptContinue, 2, nullptr, 0, false},
    {"interceptWithRequest", P_Network_interceptWithRequest, 5, nullptr, 0, false},
    {"interceptWithResponse", P_Network_interceptWithResponse, 7, nullptr, 0, false},
    {"interceptRequestWithResponse", P_Network_interceptRequestWithResponse, 7, nullptr, 0, false},
    {"interceptRequestWithError", P_Network_interceptRequestWithError, 2, nullptr, 0, false},
};
inline const Param P_Page_reload[] = {{"ignoreCache", 'B', false, -1}, {"revalidateAllResources", 'B', false, -1}};
inline const Param P_Page_navigate[] = {{"url", 'S', true, -1}};
inline const Param P_Page_overrideUserAgent[] = {{"value", 'S', false, -1}};
inline const Param P_Page_overrideSetting[] = {{"setting", 'S', true, 7}, {"value", 'B', false, -1}};
inline const Param P_Page_overrideUserPreference[] = {{"name", 'S', true, 8}, {"value", 'S', false, 9}};
inline const Out O_Page_getCookies[] = {{'A', false, -1}};
inline const Param P_Page_setCookie[] = {{"cookie", 'O', true, -1}};
inline const Param P_Page_deleteCookie[] = {{"cookieName", 'S', true, -1}, {"url", 'S', true, -1}};
inline const Out O_Page_getResourceTree[] = {{'O', false, -1}};
inline const Param P_Page_getResourceContent[] = {{"frameId", 'S', true, -1}, {"url", 'S', true, -1}};
inline const Out O_Page_getResourceContent[] = {{'S', false, -1}, {'B', false, -1}};
inline const Param P_Page_setBootstrapScript[] = {{"source", 'S', false, -1}};
inline const Param P_Page_searchInResource[] = {{"frameId", 'S', true, -1}, {"url", 'S', true, -1}, {"query", 'S', true, -1}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}, {"requestId", 'S', false, -1}};
inline const Out O_Page_searchInResource[] = {{'A', false, -1}};
inline const Param P_Page_searchInResources[] = {{"text", 'S', true, -1}, {"caseSensitive", 'B', false, -1}, {"isRegex", 'B', false, -1}};
inline const Out O_Page_searchInResources[] = {{'A', false, -1}};
inline const Param P_Page_setShowRulers[] = {{"result", 'B', true, -1}};
inline const Param P_Page_setShowPaintRects[] = {{"result", 'B', true, -1}};
inline const Param P_Page_setEmulatedMedia[] = {{"media", 'S', true, -1}};
inline const Param P_Page_snapshotNode[] = {{"nodeId", 'I', true, -1}};
inline const Out O_Page_snapshotNode[] = {{'S', false, -1}};
inline const Param P_Page_snapshotRect[] = {{"x", 'I', true, -1}, {"y", 'I', true, -1}, {"width", 'I', true, -1}, {"height", 'I', true, -1}, {"coordinateSystem", 'S', true, 10}};
inline const Out O_Page_snapshotRect[] = {{'S', false, -1}};
inline const Param P_Page_setScreenSizeOverride[] = {{"width", 'I', false, -1}, {"height", 'I', false, -1}};
inline const Command C_Page[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"reload", P_Page_reload, 2, nullptr, 0, false},
    {"navigate", P_Page_navigate, 1, nullptr, 0, false},
    {"overrideUserAgent", P_Page_overrideUserAgent, 1, nullptr, 0, false},
    {"overrideSetting", P_Page_overrideSetting, 2, nullptr, 0, false},
    {"overrideUserPreference", P_Page_overrideUserPreference, 2, nullptr, 0, false},
    {"getCookies", nullptr, 0, O_Page_getCookies, 1, false},
    {"setCookie", P_Page_setCookie, 1, nullptr, 0, false},
    {"deleteCookie", P_Page_deleteCookie, 2, nullptr, 0, false},
    {"getResourceTree", nullptr, 0, O_Page_getResourceTree, 1, false},
    {"getResourceContent", P_Page_getResourceContent, 2, O_Page_getResourceContent, 2, false},
    {"setBootstrapScript", P_Page_setBootstrapScript, 1, nullptr, 0, false},
    {"searchInResource", P_Page_searchInResource, 6, O_Page_searchInResource, 1, false},
    {"searchInResources", P_Page_searchInResources, 3, O_Page_searchInResources, 1, false},
    {"setShowRulers", P_Page_setShowRulers, 1, nullptr, 0, false},
    {"setShowPaintRects", P_Page_setShowPaintRects, 1, nullptr, 0, false},
    {"setEmulatedMedia", P_Page_setEmulatedMedia, 1, nullptr, 0, false},
    {"snapshotNode", P_Page_snapshotNode, 1, O_Page_snapshotNode, 1, false},
    {"snapshotRect", P_Page_snapshotRect, 5, O_Page_snapshotRect, 1, false},
    {"setScreenSizeOverride", P_Page_setScreenSizeOverride, 2, nullptr, 0, false},
};
inline const Param P_Runtime_parse[] = {{"source", 'S', true, -1}};
inline const Out O_Runtime_parse[] = {{'E', false, 11}, {'S', true, -1}, {'o', true, -1}};
inline const Param P_Runtime_evaluate[] = {{"expression", 'S', true, -1}, {"objectGroup", 'S', false, -1}, {"includeCommandLineAPI", 'B', false, -1}, {"doNotPauseOnExceptionsAndMuteConsole", 'B', false, -1}, {"contextId", 'I', false, -1}, {"returnByValue", 'B', false, -1}, {"generatePreview", 'B', false, -1}, {"saveResult", 'B', false, -1}, {"emulateUserGesture", 'B', false, -1}};
inline const Out O_Runtime_evaluate[] = {{'O', false, -1}, {'b', true, -1}, {'i', true, -1}};
inline const Param P_Runtime_awaitPromise[] = {{"promiseObjectId", 'S', true, -1}, {"returnByValue", 'B', false, -1}, {"generatePreview", 'B', false, -1}, {"saveResult", 'B', false, -1}};
inline const Param P_Runtime_callFunctionOn[] = {{"objectId", 'S', true, -1}, {"functionDeclaration", 'S', true, -1}, {"arguments", 'A', false, -1}, {"doNotPauseOnExceptionsAndMuteConsole", 'B', false, -1}, {"returnByValue", 'B', false, -1}, {"generatePreview", 'B', false, -1}, {"emulateUserGesture", 'B', false, -1}};
inline const Out O_Runtime_callFunctionOn[] = {{'O', false, -1}, {'b', true, -1}};
inline const Param P_Runtime_getPreview[] = {{"objectId", 'S', true, -1}};
inline const Out O_Runtime_getPreview[] = {{'O', false, -1}};
inline const Param P_Runtime_getProperties[] = {{"objectId", 'S', true, -1}, {"ownProperties", 'B', false, -1}, {"fetchStart", 'I', false, -1}, {"fetchCount", 'I', false, -1}, {"generatePreview", 'B', false, -1}};
inline const Out O_Runtime_getProperties[] = {{'A', false, -1}, {'a', true, -1}};
inline const Param P_Runtime_getDisplayableProperties[] = {{"objectId", 'S', true, -1}, {"fetchStart", 'I', false, -1}, {"fetchCount", 'I', false, -1}, {"generatePreview", 'B', false, -1}};
inline const Out O_Runtime_getDisplayableProperties[] = {{'A', false, -1}, {'a', true, -1}};
inline const Param P_Runtime_getCollectionEntries[] = {{"objectId", 'S', true, -1}, {"objectGroup", 'S', false, -1}, {"fetchStart", 'I', false, -1}, {"fetchCount", 'I', false, -1}};
inline const Out O_Runtime_getCollectionEntries[] = {{'A', false, -1}};
inline const Param P_Runtime_saveResult[] = {{"value", 'O', true, -1}, {"contextId", 'I', false, -1}};
inline const Out O_Runtime_saveResult[] = {{'i', true, -1}};
inline const Param P_Runtime_setSavedResultAlias[] = {{"alias", 'S', false, -1}};
inline const Param P_Runtime_releaseObject[] = {{"objectId", 'S', true, -1}};
inline const Param P_Runtime_releaseObjectGroup[] = {{"objectGroup", 'S', true, -1}};
inline const Param P_Runtime_getRuntimeTypesForVariablesAtOffsets[] = {{"locations", 'A', true, -1}};
inline const Out O_Runtime_getRuntimeTypesForVariablesAtOffsets[] = {{'A', false, -1}};
inline const Param P_Runtime_getBasicBlocks[] = {{"sourceID", 'S', true, -1}};
inline const Out O_Runtime_getBasicBlocks[] = {{'A', false, -1}};
inline const Command C_Runtime[] = {
    {"parse", P_Runtime_parse, 1, O_Runtime_parse, 3, false},
    {"evaluate", P_Runtime_evaluate, 9, O_Runtime_evaluate, 3, false},
    {"awaitPromise", P_Runtime_awaitPromise, 4, nullptr, 0, true},
    {"callFunctionOn", P_Runtime_callFunctionOn, 7, O_Runtime_callFunctionOn, 2, false},
    {"getPreview", P_Runtime_getPreview, 1, O_Runtime_getPreview, 1, false},
    {"getProperties", P_Runtime_getProperties, 5, O_Runtime_getProperties, 2, false},
    {"getDisplayableProperties", P_Runtime_getDisplayableProperties, 4, O_Runtime_getDisplayableProperties, 2, false},
    {"getCollectionEntries", P_Runtime_getCollectionEntries, 4, O_Runtime_getCollectionEntries, 1, false},
    {"saveResult", P_Runtime_saveResult, 2, O_Runtime_saveResult, 1, false},
    {"setSavedResultAlias", P_Runtime_setSavedResultAlias, 1, nullptr, 0, false},
    {"releaseObject", P_Runtime_releaseObject, 1, nullptr, 0, false},
    {"releaseObjectGroup", P_Runtime_releaseObjectGroup, 1, nullptr, 0, false},
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"getRuntimeTypesForVariablesAtOffsets", P_Runtime_getRuntimeTypesForVariablesAtOffsets, 1, O_Runtime_getRuntimeTypesForVariablesAtOffsets, 1, false},
    {"enableTypeProfiler", nullptr, 0, nullptr, 0, false},
    {"disableTypeProfiler", nullptr, 0, nullptr, 0, false},
    {"enableControlFlowProfiler", nullptr, 0, nullptr, 0, false},
    {"disableControlFlowProfiler", nullptr, 0, nullptr, 0, false},
    {"getBasicBlocks", P_Runtime_getBasicBlocks, 1, O_Runtime_getBasicBlocks, 1, false},
};
inline const Param P_ScriptProfiler_startTracking[] = {{"includeSamples", 'B', false, -1}};
inline const Command C_ScriptProfiler[] = {
    {"startTracking", P_ScriptProfiler_startTracking, 1, nullptr, 0, false},
    {"stopTracking", nullptr, 0, nullptr, 0, false},
};
inline const Param P_Target_setPauseOnStart[] = {{"pauseOnStart", 'B', true, -1}};
inline const Param P_Target_resume[] = {{"targetId", 'S', true, -1}};
inline const Param P_Target_sendMessageToTarget[] = {{"targetId", 'S', true, -1}, {"message", 'S', true, -1}};
inline const Command C_Target[] = {
    {"setPauseOnStart", P_Target_setPauseOnStart, 1, nullptr, 0, false},
    {"resume", P_Target_resume, 1, nullptr, 0, false},
    {"sendMessageToTarget", P_Target_sendMessageToTarget, 2, nullptr, 0, false},
};
inline const Param P_Timeline_start[] = {{"maxCallStackDepth", 'I', false, -1}};
inline const Param P_Timeline_setAutoCaptureEnabled[] = {{"enabled", 'B', true, -1}};
inline const Param P_Timeline_setInstruments[] = {{"instruments", 'A', true, -1}};
inline const Command C_Timeline[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"start", P_Timeline_start, 1, nullptr, 0, false},
    {"stop", nullptr, 0, nullptr, 0, false},
    {"setAutoCaptureEnabled", P_Timeline_setAutoCaptureEnabled, 1, nullptr, 0, false},
    {"setInstruments", P_Timeline_setInstruments, 1, nullptr, 0, false},
};
inline const Param P_Worker_initialized[] = {{"workerId", 'S', true, -1}};
inline const Param P_Worker_sendMessageToWorker[] = {{"workerId", 'S', true, -1}, {"message", 'S', true, -1}};
inline const Command C_Worker[] = {
    {"enable", nullptr, 0, nullptr, 0, false},
    {"disable", nullptr, 0, nullptr, 0, false},
    {"initialized", P_Worker_initialized, 1, nullptr, 0, false},
    {"sendMessageToWorker", P_Worker_sendMessageToWorker, 2, nullptr, 0, false},
};
inline const Domain Domains[] = {
    {"Animation", C_Animation, 6},
    {"ApplicationCache", C_ApplicationCache, 5},
    {"Audit", C_Audit, 3},
    {"Browser", C_Browser, 2},
    {"Canvas", C_Canvas, 9},
    {"Console", C_Console, 5},
    {"CSS", C_CSS, 19},
    {"Database", C_Database, 4},
    {"Debugger", C_Debugger, 29},
    {"DOM", C_DOM, 45},
    {"DOMDebugger", C_DOMDebugger, 6},
    {"DOMStorage", C_DOMStorage, 6},
    {"Heap", C_Heap, 8},
    {"Inspector", C_Inspector, 3},
    {"LayerTree", C_LayerTree, 4},
    {"Network", C_Network, 16},
    {"Page", C_Page, 21},
    {"Runtime", C_Runtime, 20},
    {"ScriptProfiler", C_ScriptProfiler, 2},
    {"Target", C_Target, 3},
    {"Timeline", C_Timeline, 6},
    {"Worker", C_Worker, 4},
};

inline Random* rng;
inline Hash* hash;

inline std::u16string Ascii(const char* text) {
    std::u16string result;
    while (*text) result.push_back(static_cast<unsigned char>(*text++));
    return result;
}

inline std::u16string Decimal(long long value) {
    return Ascii(std::to_string(value).c_str());
}

inline void Begin(int domain, int command) {
    hash->Mix(0xC0DE0000u + static_cast<unsigned>(domain) * 256u + static_cast<unsigned>(command));
}

inline void RecordText(const Text& text) {
    hash->Mix(text.isNull ? 2 : text.is8Bit);
    if (!text.isNull) hash->MixText(text.text);
}

inline void RecordInteger(long long value) {
    hash->Mix(static_cast<std::uint64_t>(value));
}

inline void RecordOptional(bool has, long long value) {
    hash->Mix(has);
    if (has) hash->Mix(static_cast<std::uint64_t>(value));
}

inline Text RandomText() {
    static const char16_t* const pool[] = {u"", u"a", u"node-7", u"x y", u"q\"\\/", u"line\nbreak", u"tab-here", u"0"};
    Text result;
    result.text = pool[(*rng)(8)];
    result.is8Bit = (*rng)(4) != 0;
    for (char16_t c : result.text)
        if (c > 0xFF) result.is8Bit = false;
    return result;
}

inline bool Fail() {
    return (*rng)(6) == 0;
}

inline Text ErrorText() {
    Text result = RandomText();
    if (result.text.empty()) result.text = u"failed";
    return result;
}

inline Text OutString(bool conditional) {
    if (conditional && !(*rng)(3)) {
        Text result;
        result.isNull = true;
        return result;
    }
    return RandomText();
}

inline int OutInteger() {
    static const int pool[] = {0, 1, -1, 42, 2147483647, -2147483647 - 1, 1000000};
    return pool[(*rng)(7)];
}

inline bool OutBoolean() {
    return (*rng)(2) != 0;
}

inline double OutDouble() {
    static const double pool[] = {0.0, 1.5, -2.25, 1e21, 123456789.125, 1e-7, -0.0};
    return pool[(*rng)(7)];
}

inline int OutJson(bool conditional) {
    if (conditional && !(*rng)(3)) return -1;
    return static_cast<int>((*rng)(1000));
}

inline int OutOptionalBoolean() {
    return static_cast<int>((*rng)(3)) - 1;
}

inline bool OutOptionalInteger(int& value) {
    if (!(*rng)(3)) return false;
    value = OutInteger();
    return true;
}

inline int OutEnum(int list) {
    const EnumList& values = EnumLists[list];
    return values.values[(*rng)(static_cast<std::uint32_t>(values.count))];
}

inline bool CallbackFails() {
    return (*rng)(2) != 0;
}

inline void AppendQuoted(std::u16string& out, const std::u16string& text) {
    out.push_back(u'"');
    for (char16_t c : text) {
        if (c == u'"' || c == u'\\') {
            out.push_back(u'\\');
            out.push_back(c);
        } else if (c == u'\n') {
            out += u"\\n";
        } else {
            out.push_back(c);
        }
    }
    out.push_back(u'"');
}

inline void AppendValue(std::u16string& out, char getter, int enumIndex) {
    switch (getter) {
    case 'B':
        out += (*rng)(2) ? u"true" : u"false";
        return;
    case 'I': {
        static const char16_t* const pool[] = {u"0", u"1", u"-5", u"42", u"2147483647", u"-2147483648", u"3.75", u"-2.5", u"1e10", u"4294967296", u"-0"};
        out += pool[(*rng)(11)];
        return;
    }
    case 'S':
        if (enumIndex >= 0 && (*rng)(5)) {
            const EnumList& values = EnumLists[enumIndex];
            AppendQuoted(out, Ascii(EnumNames[values.values[(*rng)(static_cast<std::uint32_t>(values.count))]]));
            return;
        }
        if (enumIndex >= 0 && (*rng)(2)) {
            AppendQuoted(out, u"bogus");
            return;
        }
        AppendQuoted(out, RandomText().text);
        return;
    case 'O': {
        static const char16_t* const pool[] = {u"{}", u"{\"x\":1}", u"{\"a\":\"b\",\"n\":{\"m\":[1,2]}}", u"{\"lineNumber\":3,\"scriptId\":\"s\"}"};
        out += pool[(*rng)(4)];
        return;
    }
    case 'A': {
        static const char16_t* const pool[] = {u"[]", u"[1]", u"[\"a\",{\"b\":true},null]"};
        out += pool[(*rng)(3)];
        return;
    }
    }
}

inline void AppendWrongValue(std::u16string& out, char getter) {
    static const char16_t* const pool[] = {u"null", u"7", u"\"7\"", u"true", u"{}", u"[]"};
    for (;;) {
        int pick = static_cast<int>((*rng)(6));
        if (getter == 'I' && pick == 1) continue;
        if (getter == 'S' && pick == 2) continue;
        if (getter == 'B' && pick == 3) continue;
        if (getter == 'O' && pick == 4) continue;
        if (getter == 'A' && pick == 5) continue;
        out += pool[pick];
        return;
    }
}

inline std::u16string BuildMessage(bool& is8Bit) {
    static const char16_t* const ids[] = {u"1", u"7", u"123456", u"-3", u"2147483647", u"-2147483648", u"0", u"2.9"};
    const int domainCount = static_cast<int>(sizeof(Domains) / sizeof(Domains[0]));
    const Domain& domain = Domains[(*rng)(static_cast<std::uint32_t>(domainCount))];
    std::u16string out = u"{\"id\":";
    out += ids[(*rng)(8)];
    out += u",\"method\":\"";
    out += Ascii(domain.name);
    out.push_back(u'.');
    const Command* command = nullptr;
    int unknown = static_cast<int>((*rng)(12));
    if (unknown == 0) out += u"noSuchCommand";
    else if (unknown == 1) out += u"Enable";
    else if (unknown == 2) out += u"enable2";
    else {
        command = &domain.commands[(*rng)(static_cast<std::uint32_t>(domain.count))];
        out += Ascii(command->name);
    }
    out.push_back(u'"');
    int paramsShape = static_cast<int>((*rng)(25));
    if (paramsShape == 0) {
    } else if (paramsShape == 1) {
        out += u",\"params\":[1]";
    } else if (paramsShape == 2) {
        out += u",\"params\":null";
    } else {
        out += u",\"params\":{";
        bool first = true;
        auto key = [&](const char* name) {
            if (!first) out.push_back(u',');
            first = false;
            AppendQuoted(out, Ascii(name));
            out.push_back(u':');
        };
        if (command) {
            for (int i = 0; i < command->paramCount; ++i) {
                const Param& param = command->params[i];
                int pick = static_cast<int>((*rng)(20));
                if (param.required) {
                    if (pick < 1) continue;
                    key(param.key);
                    if (pick < 2) AppendWrongValue(out, param.getter);
                    else AppendValue(out, param.getter, param.enumIndex);
                } else {
                    if (pick < 9) continue;
                    key(param.key);
                    if (pick < 11) AppendWrongValue(out, param.getter);
                    else AppendValue(out, param.getter, param.enumIndex);
                }
            }
        }
        if (!(*rng)(6)) {
            key("extra");
            out += u"\"z\"";
        }
        out.push_back(u'}');
    }
    out.push_back(u'}');
    is8Bit = (*rng)(5) != 0;
    for (char16_t c : out)
        if (c > 0xFF) is8Bit = false;
    return out;
}

template<typename Adapter> std::uint64_t Run(Adapter& adapter, std::uint32_t seed, int count) {
    Random generator{seed ? seed : 1u};
    Hash result;
    rng = &generator;
    hash = &result;
    for (int i = 0; i < count; ++i) {
        bool is8Bit = true;
        std::u16string message = BuildMessage(is8Bit);
        result.Mix(0xFEED);
        std::vector<Message> sent = adapter.Dispatch(message, is8Bit);
        result.Mix(sent.size());
        for (const Message& output : sent) {
            result.Mix(output.is8Bit);
            result.MixText(output.text);
        }
    }
    rng = nullptr;
    hash = nullptr;
    return result.value;
}

}  // namespace DomainDispatchDriver

using namespace DomainDispatchDriver;

struct OptionalBoolean {
    bool value;
    bool hasValue;
};

struct OptionalInteger {
    int value;
    bool hasValue;
};

String S(const Text& text) {
    if (text.isNull) return String();
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(text.text.size()), text.is8Bit, characters));
    for (std::size_t i = 0; i < text.text.size(); ++i) {
        if (text.is8Bit) static_cast<unsigned char*>(characters)[i] = static_cast<unsigned char>(text.text[i]);
        else static_cast<char16_t*>(characters)[i] = text.text[i];
    }
    return result;
}

Text T(const String& string) {
    Text text;
    if (!string.impl) {
        text.isNull = true;
        return text;
    }
    text.is8Bit = (string.impl->hashAndFlags & (1U << 2)) != 0;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (unsigned i = 0; i < string.impl->length; ++i) text.text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, i));
    return text;
}

void RecString(const String* value) {
    RecordText(T(*value));
}

void RecJson(const Value* value) {
    if (!value) {
        Text text;
        text.isNull = true;
        RecordText(text);
        return;
    }
    RecordText(T(NKWebKit::JsonAbi::ToJSONString(*value)));
}

void WriteString(unsigned char* field, String&& value) {
    StringImpl* impl = std::exchange(value.impl, nullptr);
    std::memcpy(field, &impl, sizeof(impl));
}

void WriteError(unsigned char* result, std::size_t hasValueOffset, String&& error) {
    WriteString(result, std::move(error));
    result[hasValueOffset] = 0;
}

void WriteInteger(unsigned char* field, int value) {
    std::memcpy(field, &value, sizeof(value));
}

void WriteDouble(unsigned char* field, double value) {
    std::memcpy(field, &value, sizeof(value));
}

void WriteJson(unsigned char* field, bool array, int n) {
    Value* value = nullptr;
    if (n >= 0 && array) {
        Ref<NKWebKit::JsonAbi::Array> created = _ZN3WTF8JSONImpl5Array6createEv();
        _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(created.Get(), n);
        value = created.Leak();
    } else if (n >= 0) {
        Ref<Object> created = _ZN3WTF8JSONImpl6Object6createEv();
        NKWebKit::JsonAbi::SetValue(*created, S(Text{u"n", false, true}), _ZN3WTF8JSONImpl5Value6createEi(n));
        value = created.Leak();
    }
    std::memcpy(field, &value, sizeof(value));
}

void WriteOptionalBoolean(unsigned char* field, int value) {
    const OptionalBoolean optional{value > 0, value >= 0};
    std::memcpy(field, &optional, sizeof(optional));
}

void WriteOptionalInteger(unsigned char* field) {
    OptionalInteger optional{0, false};
    optional.hasValue = OutOptionalInteger(optional.value);
    std::memcpy(field, &optional, sizeof(optional));
}

void APS5_VABI Agent_Animation_enable(unsigned char* result, void*) {
    Begin(0, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Animation_disable(unsigned char* result, void*) {
    Begin(0, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Animation_requestEffectTarget(unsigned char* result, void*, const String* a0) {
    Begin(0, 2);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Animation_resolveAnimation(unsigned char* result, void*, const String* a0, const String* a1) {
    Begin(0, 3);
    RecString(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Animation_startTracking(unsigned char* result, void*) {
    Begin(0, 4);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Animation_stopTracking(unsigned char* result, void*) {
    Begin(0, 5);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const AnimationAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Animation_enable), reinterpret_cast<const void*>(&Agent_Animation_disable), reinterpret_cast<const void*>(&Agent_Animation_requestEffectTarget), reinterpret_cast<const void*>(&Agent_Animation_resolveAnimation), reinterpret_cast<const void*>(&Agent_Animation_startTracking), reinterpret_cast<const void*>(&Agent_Animation_stopTracking), nullptr, nullptr};

void APS5_VABI Agent_ApplicationCache_getFramesWithManifests(unsigned char* result, void*) {
    Begin(1, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_ApplicationCache_enable(unsigned char* result, void*) {
    Begin(1, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_ApplicationCache_disable(unsigned char* result, void*) {
    Begin(1, 2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_ApplicationCache_getManifestForFrame(unsigned char* result, void*, const String* a0) {
    Begin(1, 3);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = 1;
}

void APS5_VABI Agent_ApplicationCache_getApplicationCacheForFrame(unsigned char* result, void*, const String* a0) {
    Begin(1, 4);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

const void* const ApplicationCacheAgentVtable[] = {reinterpret_cast<const void*>(&Agent_ApplicationCache_getFramesWithManifests), reinterpret_cast<const void*>(&Agent_ApplicationCache_enable), reinterpret_cast<const void*>(&Agent_ApplicationCache_disable), reinterpret_cast<const void*>(&Agent_ApplicationCache_getManifestForFrame), reinterpret_cast<const void*>(&Agent_ApplicationCache_getApplicationCacheForFrame), nullptr, nullptr};

void APS5_VABI Agent_Audit_setup(unsigned char* result, void*, const OptionalInteger* a0) {
    Begin(2, 0);
    RecordOptional(a0->hasValue, a0->hasValue ? a0->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Audit_run(unsigned char* result, void*, const String* a0, const OptionalInteger* a1) {
    Begin(2, 1);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    WriteOptionalBoolean(result + 8, OutOptionalBoolean());
    result[16] = 1;
}

void APS5_VABI Agent_Audit_teardown(unsigned char* result, void*) {
    Begin(2, 2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const AuditAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Audit_setup), reinterpret_cast<const void*>(&Agent_Audit_run), reinterpret_cast<const void*>(&Agent_Audit_teardown), nullptr, nullptr};

void APS5_VABI Agent_Browser_enable(unsigned char* result, void*) {
    Begin(3, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Browser_disable(unsigned char* result, void*) {
    Begin(3, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const BrowserAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Browser_enable), reinterpret_cast<const void*>(&Agent_Browser_disable), nullptr, nullptr};

void APS5_VABI Agent_Canvas_enable(unsigned char* result, void*) {
    Begin(4, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Canvas_disable(unsigned char* result, void*) {
    Begin(4, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Canvas_requestNode(unsigned char* result, void*, const String* a0) {
    Begin(4, 2);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteInteger(result + 0, OutInteger());
    result[8] = 1;
}

void APS5_VABI Agent_Canvas_requestContent(unsigned char* result, void*, const String* a0) {
    Begin(4, 3);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = 1;
}

void APS5_VABI Agent_Canvas_requestClientNodes(unsigned char* result, void*, const String* a0) {
    Begin(4, 4);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Canvas_resolveContext(unsigned char* result, void*, const String* a0, const String* a1) {
    Begin(4, 5);
    RecString(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Canvas_setRecordingAutoCaptureFrameCount(unsigned char* result, void*, int a0) {
    Begin(4, 6);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Canvas_startRecording(unsigned char* result, void*, const String* a0, const OptionalInteger* a1, const OptionalInteger* a2) {
    Begin(4, 7);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Canvas_stopRecording(unsigned char* result, void*, const String* a0) {
    Begin(4, 8);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const CanvasAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Canvas_enable), reinterpret_cast<const void*>(&Agent_Canvas_disable), reinterpret_cast<const void*>(&Agent_Canvas_requestNode), reinterpret_cast<const void*>(&Agent_Canvas_requestContent), reinterpret_cast<const void*>(&Agent_Canvas_requestClientNodes), reinterpret_cast<const void*>(&Agent_Canvas_resolveContext), reinterpret_cast<const void*>(&Agent_Canvas_setRecordingAutoCaptureFrameCount), reinterpret_cast<const void*>(&Agent_Canvas_startRecording), reinterpret_cast<const void*>(&Agent_Canvas_stopRecording), nullptr, nullptr};

void APS5_VABI Agent_Console_enable(unsigned char* result, void*) {
    Begin(5, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Console_disable(unsigned char* result, void*) {
    Begin(5, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Console_clearMessages(unsigned char* result, void*) {
    Begin(5, 2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Console_getLoggingChannels(unsigned char* result, void*) {
    Begin(5, 3);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Console_setLoggingChannelLevel(unsigned char* result, void*, int a0, int a1) {
    Begin(5, 4);
    RecordInteger(a0);
    RecordInteger(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const ConsoleAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Console_enable), reinterpret_cast<const void*>(&Agent_Console_disable), reinterpret_cast<const void*>(&Agent_Console_clearMessages), reinterpret_cast<const void*>(&Agent_Console_getLoggingChannels), reinterpret_cast<const void*>(&Agent_Console_setLoggingChannelLevel), nullptr, nullptr};

void APS5_VABI Agent_CSS_enable(unsigned char* result, void*) {
    Begin(6, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_CSS_disable(unsigned char* result, void*) {
    Begin(6, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_CSS_getMatchedStylesForNode(unsigned char* result, void*, int a0, const OptionalBoolean* a1, const OptionalBoolean* a2) {
    Begin(6, 2);
    RecordInteger(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    if (Fail()) {
        WriteError(result, 24, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(true));
    WriteJson(result + 8, true, OutJson(true));
    WriteJson(result + 16, true, OutJson(true));
    result[24] = 1;
}

void APS5_VABI Agent_CSS_getInlineStylesForNode(unsigned char* result, void*, int a0) {
    Begin(6, 3);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(true));
    WriteJson(result + 8, false, OutJson(true));
    result[16] = 1;
}

void APS5_VABI Agent_CSS_getComputedStyleForNode(unsigned char* result, void*, int a0) {
    Begin(6, 4);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_getFontDataForNode(unsigned char* result, void*, int a0) {
    Begin(6, 5);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_getAllStyleSheets(unsigned char* result, void*) {
    Begin(6, 6);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_getStyleSheet(unsigned char* result, void*, const String* a0) {
    Begin(6, 7);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_getStyleSheetText(unsigned char* result, void*, const String* a0) {
    Begin(6, 8);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_setStyleSheetText(unsigned char* result, void*, const String* a0, const String* a1) {
    Begin(6, 9);
    RecString(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_CSS_setStyleText(unsigned char* result, void*, Value** a0, const String* a1) {
    Begin(6, 10);
    RecJson(*a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_setRuleSelector(unsigned char* result, void*, Value** a0, const String* a1) {
    Begin(6, 11);
    RecJson(*a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_setGroupingHeaderText(unsigned char* result, void*, Value** a0, const String* a1) {
    Begin(6, 12);
    RecJson(*a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_createStyleSheet(unsigned char* result, void*, const String* a0) {
    Begin(6, 13);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_addRule(unsigned char* result, void*, const String* a0, const String* a1) {
    Begin(6, 14);
    RecString(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_getSupportedCSSProperties(unsigned char* result, void*) {
    Begin(6, 15);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_getSupportedSystemFontFamilyNames(unsigned char* result, void*) {
    Begin(6, 16);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_CSS_forcePseudoState(unsigned char* result, void*, int a0, Value** a1) {
    Begin(6, 17);
    RecordInteger(a0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_CSS_setLayoutContextTypeChangedMode(unsigned char* result, void*, int a0) {
    Begin(6, 18);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const CSSAgentVtable[] = {reinterpret_cast<const void*>(&Agent_CSS_enable), reinterpret_cast<const void*>(&Agent_CSS_disable), reinterpret_cast<const void*>(&Agent_CSS_getMatchedStylesForNode), reinterpret_cast<const void*>(&Agent_CSS_getInlineStylesForNode), reinterpret_cast<const void*>(&Agent_CSS_getComputedStyleForNode), reinterpret_cast<const void*>(&Agent_CSS_getFontDataForNode), reinterpret_cast<const void*>(&Agent_CSS_getAllStyleSheets), reinterpret_cast<const void*>(&Agent_CSS_getStyleSheet), reinterpret_cast<const void*>(&Agent_CSS_getStyleSheetText), reinterpret_cast<const void*>(&Agent_CSS_setStyleSheetText), reinterpret_cast<const void*>(&Agent_CSS_setStyleText), reinterpret_cast<const void*>(&Agent_CSS_setRuleSelector), reinterpret_cast<const void*>(&Agent_CSS_setGroupingHeaderText), reinterpret_cast<const void*>(&Agent_CSS_createStyleSheet), reinterpret_cast<const void*>(&Agent_CSS_addRule), reinterpret_cast<const void*>(&Agent_CSS_getSupportedCSSProperties), reinterpret_cast<const void*>(&Agent_CSS_getSupportedSystemFontFamilyNames), reinterpret_cast<const void*>(&Agent_CSS_forcePseudoState), reinterpret_cast<const void*>(&Agent_CSS_setLayoutContextTypeChangedMode), nullptr, nullptr};

void APS5_VABI Agent_Database_enable(unsigned char* result, void*) {
    Begin(7, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Database_disable(unsigned char* result, void*) {
    Begin(7, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Database_getDatabaseTableNames(unsigned char* result, void*, const String* a0) {
    Begin(7, 2);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Database_executeSQL(void*, const String* a0, const String* a1, CallbackBase** a2) {
    Begin(7, 3);
    RecString(a0);
    RecString(a1);
    if (CallbackFails()) {
        const String error = S(ErrorText());
        _ZN9Inspector17BackendDispatcher12CallbackBase11sendFailureERKN3WTF6StringE(*a2, &error);
    }
}

const void* const DatabaseAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Database_enable), reinterpret_cast<const void*>(&Agent_Database_disable), reinterpret_cast<const void*>(&Agent_Database_getDatabaseTableNames), reinterpret_cast<const void*>(&Agent_Database_executeSQL), nullptr, nullptr};

void APS5_VABI Agent_Debugger_enable(unsigned char* result, void*) {
    Begin(8, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_disable(unsigned char* result, void*) {
    Begin(8, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_setAsyncStackTraceDepth(unsigned char* result, void*, int a0) {
    Begin(8, 2);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_setBreakpointsActive(unsigned char* result, void*, bool a0) {
    Begin(8, 3);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_setBreakpointByUrl(unsigned char* result, void*, int a0, const String* a1, const String* a2, const OptionalInteger* a3, Value** a4) {
    Begin(8, 4);
    RecordInteger(a0);
    RecString(a1);
    RecString(a2);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    RecJson(*a4);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    WriteJson(result + 8, true, OutJson(false));
    result[16] = 1;
}

void APS5_VABI Agent_Debugger_setBreakpoint(unsigned char* result, void*, Value** a0, Value** a1) {
    Begin(8, 5);
    RecJson(*a0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    WriteJson(result + 8, false, OutJson(false));
    result[16] = 1;
}

void APS5_VABI Agent_Debugger_removeBreakpoint(unsigned char* result, void*, const String* a0) {
    Begin(8, 6);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_addSymbolicBreakpoint(unsigned char* result, void*, const String* a0, const OptionalBoolean* a1, const OptionalBoolean* a2, Value** a3) {
    Begin(8, 7);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecJson(*a3);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_removeSymbolicBreakpoint(unsigned char* result, void*, const String* a0, const OptionalBoolean* a1, const OptionalBoolean* a2) {
    Begin(8, 8);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_continueUntilNextRunLoop(unsigned char* result, void*) {
    Begin(8, 9);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_continueToLocation(unsigned char* result, void*, Value** a0) {
    Begin(8, 10);
    RecJson(*a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_stepNext(unsigned char* result, void*) {
    Begin(8, 11);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_stepOver(unsigned char* result, void*) {
    Begin(8, 12);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_stepInto(unsigned char* result, void*) {
    Begin(8, 13);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_stepOut(unsigned char* result, void*) {
    Begin(8, 14);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_pause(unsigned char* result, void*) {
    Begin(8, 15);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_resume(unsigned char* result, void*) {
    Begin(8, 16);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_searchInContent(unsigned char* result, void*, const String* a0, const String* a1, const OptionalBoolean* a2, const OptionalBoolean* a3) {
    Begin(8, 17);
    RecString(a0);
    RecString(a1);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_getScriptSource(unsigned char* result, void*, const String* a0) {
    Begin(8, 18);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_getFunctionDetails(unsigned char* result, void*, const String* a0) {
    Begin(8, 19);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_getBreakpointLocations(unsigned char* result, void*, Value** a0, Value** a1) {
    Begin(8, 20);
    RecJson(*a0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_setPauseOnDebuggerStatements(unsigned char* result, void*, bool a0, Value** a1) {
    Begin(8, 21);
    RecordInteger(a0 ? 1 : 0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_setPauseOnExceptions(unsigned char* result, void*, const String* a0, Value** a1) {
    Begin(8, 22);
    RecString(a0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_setPauseOnAssertions(unsigned char* result, void*, bool a0, Value** a1) {
    Begin(8, 23);
    RecordInteger(a0 ? 1 : 0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_setPauseOnMicrotasks(unsigned char* result, void*, bool a0, Value** a1) {
    Begin(8, 24);
    RecordInteger(a0 ? 1 : 0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_setPauseForInternalScripts(unsigned char* result, void*, bool a0) {
    Begin(8, 25);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_evaluateOnCallFrame(unsigned char* result, void*, const String* a0, const String* a1, const String* a2, const OptionalBoolean* a3, const OptionalBoolean* a4, const OptionalBoolean* a5, const OptionalBoolean* a6, const OptionalBoolean* a7, const OptionalBoolean* a8) {
    Begin(8, 26);
    RecString(a0);
    RecString(a1);
    RecString(a2);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    RecordOptional(a4->hasValue, a4->hasValue ? a4->value : 0);
    RecordOptional(a5->hasValue, a5->hasValue ? a5->value : 0);
    RecordOptional(a6->hasValue, a6->hasValue ? a6->value : 0);
    RecordOptional(a7->hasValue, a7->hasValue ? a7->value : 0);
    RecordOptional(a8->hasValue, a8->hasValue ? a8->value : 0);
    if (Fail()) {
        WriteError(result, 24, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    WriteOptionalBoolean(result + 8, OutOptionalBoolean());
    WriteOptionalInteger(result + 12);
    result[24] = 1;
}

void APS5_VABI Agent_Debugger_setShouldBlackboxURL(unsigned char* result, void*, const String* a0, bool a1, const OptionalBoolean* a2, const OptionalBoolean* a3) {
    Begin(8, 27);
    RecString(a0);
    RecordInteger(a1 ? 1 : 0);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Debugger_setBlackboxBreakpointEvaluations(unsigned char* result, void*, bool a0) {
    Begin(8, 28);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const DebuggerAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Debugger_enable), reinterpret_cast<const void*>(&Agent_Debugger_disable), reinterpret_cast<const void*>(&Agent_Debugger_setAsyncStackTraceDepth), reinterpret_cast<const void*>(&Agent_Debugger_setBreakpointsActive), reinterpret_cast<const void*>(&Agent_Debugger_setBreakpointByUrl), reinterpret_cast<const void*>(&Agent_Debugger_setBreakpoint), reinterpret_cast<const void*>(&Agent_Debugger_removeBreakpoint), reinterpret_cast<const void*>(&Agent_Debugger_addSymbolicBreakpoint), reinterpret_cast<const void*>(&Agent_Debugger_removeSymbolicBreakpoint), reinterpret_cast<const void*>(&Agent_Debugger_continueUntilNextRunLoop), reinterpret_cast<const void*>(&Agent_Debugger_continueToLocation), reinterpret_cast<const void*>(&Agent_Debugger_stepNext), reinterpret_cast<const void*>(&Agent_Debugger_stepOver), reinterpret_cast<const void*>(&Agent_Debugger_stepInto), reinterpret_cast<const void*>(&Agent_Debugger_stepOut), reinterpret_cast<const void*>(&Agent_Debugger_pause), reinterpret_cast<const void*>(&Agent_Debugger_resume), reinterpret_cast<const void*>(&Agent_Debugger_searchInContent), reinterpret_cast<const void*>(&Agent_Debugger_getScriptSource), reinterpret_cast<const void*>(&Agent_Debugger_getFunctionDetails), reinterpret_cast<const void*>(&Agent_Debugger_getBreakpointLocations), reinterpret_cast<const void*>(&Agent_Debugger_setPauseOnDebuggerStatements), reinterpret_cast<const void*>(&Agent_Debugger_setPauseOnExceptions), reinterpret_cast<const void*>(&Agent_Debugger_setPauseOnAssertions), reinterpret_cast<const void*>(&Agent_Debugger_setPauseOnMicrotasks), reinterpret_cast<const void*>(&Agent_Debugger_setPauseForInternalScripts), reinterpret_cast<const void*>(&Agent_Debugger_evaluateOnCallFrame), reinterpret_cast<const void*>(&Agent_Debugger_setShouldBlackboxURL), reinterpret_cast<const void*>(&Agent_Debugger_setBlackboxBreakpointEvaluations), nullptr, nullptr};

void APS5_VABI Agent_DOM_getDocument(unsigned char* result, void*) {
    Begin(9, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_DOM_requestChildNodes(unsigned char* result, void*, int a0, const OptionalInteger* a1) {
    Begin(9, 1);
    RecordInteger(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_querySelector(unsigned char* result, void*, int a0, const String* a1) {
    Begin(9, 2);
    RecordInteger(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteOptionalInteger(result + 0);
    result[8] = 1;
}

void APS5_VABI Agent_DOM_querySelectorAll(unsigned char* result, void*, int a0, const String* a1) {
    Begin(9, 3);
    RecordInteger(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setNodeName(unsigned char* result, void*, int a0, const String* a1) {
    Begin(9, 4);
    RecordInteger(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteInteger(result + 0, OutInteger());
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setNodeValue(unsigned char* result, void*, int a0, const String* a1) {
    Begin(9, 5);
    RecordInteger(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_removeNode(unsigned char* result, void*, int a0) {
    Begin(9, 6);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setAttributeValue(unsigned char* result, void*, int a0, const String* a1, const String* a2) {
    Begin(9, 7);
    RecordInteger(a0);
    RecString(a1);
    RecString(a2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setAttributesAsText(unsigned char* result, void*, int a0, const String* a1, const String* a2) {
    Begin(9, 8);
    RecordInteger(a0);
    RecString(a1);
    RecString(a2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_removeAttribute(unsigned char* result, void*, int a0, const String* a1) {
    Begin(9, 9);
    RecordInteger(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_getSupportedEventNames(unsigned char* result, void*) {
    Begin(9, 10);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_DOM_getEventListenersForNode(unsigned char* result, void*, int a0, const OptionalBoolean* a1) {
    Begin(9, 11);
    RecordInteger(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setEventListenerDisabled(unsigned char* result, void*, int a0, bool a1) {
    Begin(9, 12);
    RecordInteger(a0);
    RecordInteger(a1 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setBreakpointForEventListener(unsigned char* result, void*, int a0, Value** a1) {
    Begin(9, 13);
    RecordInteger(a0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_removeBreakpointForEventListener(unsigned char* result, void*, int a0) {
    Begin(9, 14);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_getAccessibilityPropertiesForNode(unsigned char* result, void*, int a0) {
    Begin(9, 15);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_DOM_getOuterHTML(unsigned char* result, void*, int a0) {
    Begin(9, 16);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setOuterHTML(unsigned char* result, void*, int a0, const String* a1) {
    Begin(9, 17);
    RecordInteger(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_insertAdjacentHTML(unsigned char* result, void*, int a0, const String* a1, const String* a2) {
    Begin(9, 18);
    RecordInteger(a0);
    RecString(a1);
    RecString(a2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_performSearch(unsigned char* result, void*, const String* a0, Value** a1, const OptionalBoolean* a2) {
    Begin(9, 19);
    RecString(a0);
    RecJson(*a1);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    WriteInteger(result + 8, OutInteger());
    result[16] = 1;
}

void APS5_VABI Agent_DOM_getSearchResults(unsigned char* result, void*, const String* a0, int a1, int a2) {
    Begin(9, 20);
    RecString(a0);
    RecordInteger(a1);
    RecordInteger(a2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_DOM_discardSearchResults(unsigned char* result, void*, const String* a0) {
    Begin(9, 21);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_requestNode(unsigned char* result, void*, const String* a0) {
    Begin(9, 22);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteInteger(result + 0, OutInteger());
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setInspectModeEnabled(unsigned char* result, void*, bool a0, Value** a1, Value** a2, Value** a3, const OptionalBoolean* a4) {
    Begin(9, 23);
    RecordInteger(a0 ? 1 : 0);
    RecJson(*a1);
    RecJson(*a2);
    RecJson(*a3);
    RecordOptional(a4->hasValue, a4->hasValue ? a4->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_highlightRect(unsigned char* result, void*, int a0, int a1, int a2, int a3, Value** a4, Value** a5, const OptionalBoolean* a6) {
    Begin(9, 24);
    RecordInteger(a0);
    RecordInteger(a1);
    RecordInteger(a2);
    RecordInteger(a3);
    RecJson(*a4);
    RecJson(*a5);
    RecordOptional(a6->hasValue, a6->hasValue ? a6->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_highlightQuad(unsigned char* result, void*, Value** a0, Value** a1, Value** a2, const OptionalBoolean* a3) {
    Begin(9, 25);
    RecJson(*a0);
    RecJson(*a1);
    RecJson(*a2);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_highlightSelector(unsigned char* result, void*, const String* a0, const String* a1, Value** a2, Value** a3, Value** a4, const OptionalBoolean* a5) {
    Begin(9, 26);
    RecString(a0);
    RecString(a1);
    RecJson(*a2);
    RecJson(*a3);
    RecJson(*a4);
    RecordOptional(a5->hasValue, a5->hasValue ? a5->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_highlightNode(unsigned char* result, void*, const OptionalInteger* a0, const String* a1, Value** a2, Value** a3, Value** a4, const OptionalBoolean* a5) {
    Begin(9, 27);
    RecordOptional(a0->hasValue, a0->hasValue ? a0->value : 0);
    RecString(a1);
    RecJson(*a2);
    RecJson(*a3);
    RecJson(*a4);
    RecordOptional(a5->hasValue, a5->hasValue ? a5->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_highlightNodeList(unsigned char* result, void*, Value** a0, Value** a1, Value** a2, Value** a3, const OptionalBoolean* a4) {
    Begin(9, 28);
    RecJson(*a0);
    RecJson(*a1);
    RecJson(*a2);
    RecJson(*a3);
    RecordOptional(a4->hasValue, a4->hasValue ? a4->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_hideHighlight(unsigned char* result, void*) {
    Begin(9, 29);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_highlightFrame(unsigned char* result, void*, const String* a0, Value** a1, Value** a2) {
    Begin(9, 30);
    RecString(a0);
    RecJson(*a1);
    RecJson(*a2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_showGridOverlay(unsigned char* result, void*, int a0, Value** a1) {
    Begin(9, 31);
    RecordInteger(a0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_hideGridOverlay(unsigned char* result, void*, const OptionalInteger* a0) {
    Begin(9, 32);
    RecordOptional(a0->hasValue, a0->hasValue ? a0->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_showFlexOverlay(unsigned char* result, void*, int a0, Value** a1) {
    Begin(9, 33);
    RecordInteger(a0);
    RecJson(*a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_hideFlexOverlay(unsigned char* result, void*, const OptionalInteger* a0) {
    Begin(9, 34);
    RecordOptional(a0->hasValue, a0->hasValue ? a0->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_pushNodeByPathToFrontend(unsigned char* result, void*, const String* a0) {
    Begin(9, 35);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteInteger(result + 0, OutInteger());
    result[8] = 1;
}

void APS5_VABI Agent_DOM_resolveNode(unsigned char* result, void*, int a0, const String* a1) {
    Begin(9, 36);
    RecordInteger(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_DOM_getAttributes(unsigned char* result, void*, int a0) {
    Begin(9, 37);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_DOM_moveTo(unsigned char* result, void*, int a0, int a1, const OptionalInteger* a2) {
    Begin(9, 38);
    RecordInteger(a0);
    RecordInteger(a1);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteInteger(result + 0, OutInteger());
    result[8] = 1;
}

void APS5_VABI Agent_DOM_undo(unsigned char* result, void*) {
    Begin(9, 39);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_redo(unsigned char* result, void*) {
    Begin(9, 40);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_markUndoableState(unsigned char* result, void*) {
    Begin(9, 41);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_focus(unsigned char* result, void*, int a0) {
    Begin(9, 42);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setInspectedNode(unsigned char* result, void*, int a0) {
    Begin(9, 43);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOM_setAllowEditingUserAgentShadowTrees(unsigned char* result, void*, bool a0) {
    Begin(9, 44);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const DOMAgentVtable[] = {reinterpret_cast<const void*>(&Agent_DOM_getDocument), reinterpret_cast<const void*>(&Agent_DOM_requestChildNodes), reinterpret_cast<const void*>(&Agent_DOM_querySelector), reinterpret_cast<const void*>(&Agent_DOM_querySelectorAll), reinterpret_cast<const void*>(&Agent_DOM_setNodeName), reinterpret_cast<const void*>(&Agent_DOM_setNodeValue), reinterpret_cast<const void*>(&Agent_DOM_removeNode), reinterpret_cast<const void*>(&Agent_DOM_setAttributeValue), reinterpret_cast<const void*>(&Agent_DOM_setAttributesAsText), reinterpret_cast<const void*>(&Agent_DOM_removeAttribute), reinterpret_cast<const void*>(&Agent_DOM_getSupportedEventNames), reinterpret_cast<const void*>(&Agent_DOM_getEventListenersForNode), reinterpret_cast<const void*>(&Agent_DOM_setEventListenerDisabled), reinterpret_cast<const void*>(&Agent_DOM_setBreakpointForEventListener), reinterpret_cast<const void*>(&Agent_DOM_removeBreakpointForEventListener), reinterpret_cast<const void*>(&Agent_DOM_getAccessibilityPropertiesForNode), reinterpret_cast<const void*>(&Agent_DOM_getOuterHTML), reinterpret_cast<const void*>(&Agent_DOM_setOuterHTML), reinterpret_cast<const void*>(&Agent_DOM_insertAdjacentHTML), reinterpret_cast<const void*>(&Agent_DOM_performSearch), reinterpret_cast<const void*>(&Agent_DOM_getSearchResults), reinterpret_cast<const void*>(&Agent_DOM_discardSearchResults), reinterpret_cast<const void*>(&Agent_DOM_requestNode), reinterpret_cast<const void*>(&Agent_DOM_setInspectModeEnabled), reinterpret_cast<const void*>(&Agent_DOM_highlightRect), reinterpret_cast<const void*>(&Agent_DOM_highlightQuad), reinterpret_cast<const void*>(&Agent_DOM_highlightSelector), reinterpret_cast<const void*>(&Agent_DOM_highlightNode), reinterpret_cast<const void*>(&Agent_DOM_highlightNodeList), reinterpret_cast<const void*>(&Agent_DOM_hideHighlight), reinterpret_cast<const void*>(&Agent_DOM_highlightFrame), reinterpret_cast<const void*>(&Agent_DOM_showGridOverlay), reinterpret_cast<const void*>(&Agent_DOM_hideGridOverlay), reinterpret_cast<const void*>(&Agent_DOM_showFlexOverlay), reinterpret_cast<const void*>(&Agent_DOM_hideFlexOverlay), reinterpret_cast<const void*>(&Agent_DOM_pushNodeByPathToFrontend), reinterpret_cast<const void*>(&Agent_DOM_resolveNode), reinterpret_cast<const void*>(&Agent_DOM_getAttributes), reinterpret_cast<const void*>(&Agent_DOM_moveTo), reinterpret_cast<const void*>(&Agent_DOM_undo), reinterpret_cast<const void*>(&Agent_DOM_redo), reinterpret_cast<const void*>(&Agent_DOM_markUndoableState), reinterpret_cast<const void*>(&Agent_DOM_focus), reinterpret_cast<const void*>(&Agent_DOM_setInspectedNode), reinterpret_cast<const void*>(&Agent_DOM_setAllowEditingUserAgentShadowTrees), nullptr, nullptr};

void APS5_VABI Agent_DOMDebugger_setDOMBreakpoint(unsigned char* result, void*, int a0, int a1, Value** a2) {
    Begin(10, 0);
    RecordInteger(a0);
    RecordInteger(a1);
    RecJson(*a2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOMDebugger_removeDOMBreakpoint(unsigned char* result, void*, int a0, int a1) {
    Begin(10, 1);
    RecordInteger(a0);
    RecordInteger(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOMDebugger_setEventBreakpoint(unsigned char* result, void*, int a0, const String* a1, const OptionalBoolean* a2, const OptionalBoolean* a3, Value** a4) {
    Begin(10, 2);
    RecordInteger(a0);
    RecString(a1);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    RecJson(*a4);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOMDebugger_removeEventBreakpoint(unsigned char* result, void*, int a0, const String* a1, const OptionalBoolean* a2, const OptionalBoolean* a3) {
    Begin(10, 3);
    RecordInteger(a0);
    RecString(a1);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOMDebugger_setURLBreakpoint(unsigned char* result, void*, const String* a0, const OptionalBoolean* a1, Value** a2) {
    Begin(10, 4);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    RecJson(*a2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOMDebugger_removeURLBreakpoint(unsigned char* result, void*, const String* a0, const OptionalBoolean* a1) {
    Begin(10, 5);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const DOMDebuggerAgentVtable[] = {reinterpret_cast<const void*>(&Agent_DOMDebugger_setDOMBreakpoint), reinterpret_cast<const void*>(&Agent_DOMDebugger_removeDOMBreakpoint), reinterpret_cast<const void*>(&Agent_DOMDebugger_setEventBreakpoint), reinterpret_cast<const void*>(&Agent_DOMDebugger_removeEventBreakpoint), reinterpret_cast<const void*>(&Agent_DOMDebugger_setURLBreakpoint), reinterpret_cast<const void*>(&Agent_DOMDebugger_removeURLBreakpoint), nullptr, nullptr};

void APS5_VABI Agent_DOMStorage_enable(unsigned char* result, void*) {
    Begin(11, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOMStorage_disable(unsigned char* result, void*) {
    Begin(11, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOMStorage_getDOMStorageItems(unsigned char* result, void*, Value** a0) {
    Begin(11, 2);
    RecJson(*a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_DOMStorage_setDOMStorageItem(unsigned char* result, void*, Value** a0, const String* a1, const String* a2) {
    Begin(11, 3);
    RecJson(*a0);
    RecString(a1);
    RecString(a2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOMStorage_removeDOMStorageItem(unsigned char* result, void*, Value** a0, const String* a1) {
    Begin(11, 4);
    RecJson(*a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_DOMStorage_clearDOMStorageItems(unsigned char* result, void*, Value** a0) {
    Begin(11, 5);
    RecJson(*a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const DOMStorageAgentVtable[] = {reinterpret_cast<const void*>(&Agent_DOMStorage_enable), reinterpret_cast<const void*>(&Agent_DOMStorage_disable), reinterpret_cast<const void*>(&Agent_DOMStorage_getDOMStorageItems), reinterpret_cast<const void*>(&Agent_DOMStorage_setDOMStorageItem), reinterpret_cast<const void*>(&Agent_DOMStorage_removeDOMStorageItem), reinterpret_cast<const void*>(&Agent_DOMStorage_clearDOMStorageItems), nullptr, nullptr};

void APS5_VABI Agent_Heap_enable(unsigned char* result, void*) {
    Begin(12, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Heap_disable(unsigned char* result, void*) {
    Begin(12, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Heap_gc(unsigned char* result, void*) {
    Begin(12, 2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Heap_snapshot(unsigned char* result, void*) {
    Begin(12, 3);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteDouble(result + 0, OutDouble());
    WriteString(result + 8, S(OutString(false)));
    result[16] = 1;
}

void APS5_VABI Agent_Heap_startTracking(unsigned char* result, void*) {
    Begin(12, 4);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Heap_stopTracking(unsigned char* result, void*) {
    Begin(12, 5);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Heap_getPreview(unsigned char* result, void*, int a0) {
    Begin(12, 6);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 24, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(true)));
    WriteJson(result + 8, false, OutJson(true));
    WriteJson(result + 16, false, OutJson(true));
    result[24] = 1;
}

void APS5_VABI Agent_Heap_getRemoteObject(unsigned char* result, void*, int a0, const String* a1) {
    Begin(12, 7);
    RecordInteger(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

const void* const HeapAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Heap_enable), reinterpret_cast<const void*>(&Agent_Heap_disable), reinterpret_cast<const void*>(&Agent_Heap_gc), reinterpret_cast<const void*>(&Agent_Heap_snapshot), reinterpret_cast<const void*>(&Agent_Heap_startTracking), reinterpret_cast<const void*>(&Agent_Heap_stopTracking), reinterpret_cast<const void*>(&Agent_Heap_getPreview), reinterpret_cast<const void*>(&Agent_Heap_getRemoteObject), nullptr, nullptr};

void APS5_VABI Agent_Inspector_enable(unsigned char* result, void*) {
    Begin(13, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Inspector_disable(unsigned char* result, void*) {
    Begin(13, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Inspector_initialized(unsigned char* result, void*) {
    Begin(13, 2);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const InspectorAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Inspector_enable), reinterpret_cast<const void*>(&Agent_Inspector_disable), reinterpret_cast<const void*>(&Agent_Inspector_initialized), nullptr, nullptr};

void APS5_VABI Agent_LayerTree_enable(unsigned char* result, void*) {
    Begin(14, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_LayerTree_disable(unsigned char* result, void*) {
    Begin(14, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_LayerTree_layersForNode(unsigned char* result, void*, int a0) {
    Begin(14, 2);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_LayerTree_reasonsForCompositingLayer(unsigned char* result, void*, const String* a0) {
    Begin(14, 3);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

const void* const LayerTreeAgentVtable[] = {reinterpret_cast<const void*>(&Agent_LayerTree_enable), reinterpret_cast<const void*>(&Agent_LayerTree_disable), reinterpret_cast<const void*>(&Agent_LayerTree_layersForNode), reinterpret_cast<const void*>(&Agent_LayerTree_reasonsForCompositingLayer), nullptr, nullptr};

void APS5_VABI Agent_Network_enable(unsigned char* result, void*) {
    Begin(15, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_disable(unsigned char* result, void*) {
    Begin(15, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_setExtraHTTPHeaders(unsigned char* result, void*, Value** a0) {
    Begin(15, 2);
    RecJson(*a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_getResponseBody(unsigned char* result, void*, const String* a0) {
    Begin(15, 3);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = OutBoolean() ? 1 : 0;
    result[16] = 1;
}

void APS5_VABI Agent_Network_setResourceCachingDisabled(unsigned char* result, void*, bool a0) {
    Begin(15, 4);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_loadResource(void*, const String* a0, const String* a1, CallbackBase** a2) {
    Begin(15, 5);
    RecString(a0);
    RecString(a1);
    if (CallbackFails()) {
        const String error = S(ErrorText());
        _ZN9Inspector17BackendDispatcher12CallbackBase11sendFailureERKN3WTF6StringE(*a2, &error);
    }
}

void APS5_VABI Agent_Network_getSerializedCertificate(unsigned char* result, void*, const String* a0) {
    Begin(15, 6);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = 1;
}

void APS5_VABI Agent_Network_resolveWebSocket(unsigned char* result, void*, const String* a0, const String* a1) {
    Begin(15, 7);
    RecString(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Network_setInterceptionEnabled(unsigned char* result, void*, bool a0) {
    Begin(15, 8);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_addInterception(unsigned char* result, void*, const String* a0, int a1, const OptionalBoolean* a2, const OptionalBoolean* a3) {
    Begin(15, 9);
    RecString(a0);
    RecordInteger(a1);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_removeInterception(unsigned char* result, void*, const String* a0, int a1, const OptionalBoolean* a2, const OptionalBoolean* a3) {
    Begin(15, 10);
    RecString(a0);
    RecordInteger(a1);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_interceptContinue(unsigned char* result, void*, const String* a0, int a1) {
    Begin(15, 11);
    RecString(a0);
    RecordInteger(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_interceptWithRequest(unsigned char* result, void*, const String* a0, const String* a1, const String* a2, Value** a3, const String* a4) {
    Begin(15, 12);
    RecString(a0);
    RecString(a1);
    RecString(a2);
    RecJson(*a3);
    RecString(a4);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_interceptWithResponse(unsigned char* result, void*, const String* a0, const String* a1, bool a2, const String* a3, const OptionalInteger* a4, const String* a5, Value** a6) {
    Begin(15, 13);
    RecString(a0);
    RecString(a1);
    RecordInteger(a2 ? 1 : 0);
    RecString(a3);
    RecordOptional(a4->hasValue, a4->hasValue ? a4->value : 0);
    RecString(a5);
    RecJson(*a6);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_interceptRequestWithResponse(unsigned char* result, void*, const String* a0, const String* a1, bool a2, const String* a3, int a4, const String* a5, Value** a6) {
    Begin(15, 14);
    RecString(a0);
    RecString(a1);
    RecordInteger(a2 ? 1 : 0);
    RecString(a3);
    RecordInteger(a4);
    RecString(a5);
    RecJson(*a6);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Network_interceptRequestWithError(unsigned char* result, void*, const String* a0, int a1) {
    Begin(15, 15);
    RecString(a0);
    RecordInteger(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const NetworkAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Network_enable), reinterpret_cast<const void*>(&Agent_Network_disable), reinterpret_cast<const void*>(&Agent_Network_setExtraHTTPHeaders), reinterpret_cast<const void*>(&Agent_Network_getResponseBody), reinterpret_cast<const void*>(&Agent_Network_setResourceCachingDisabled), reinterpret_cast<const void*>(&Agent_Network_loadResource), reinterpret_cast<const void*>(&Agent_Network_getSerializedCertificate), reinterpret_cast<const void*>(&Agent_Network_resolveWebSocket), reinterpret_cast<const void*>(&Agent_Network_setInterceptionEnabled), reinterpret_cast<const void*>(&Agent_Network_addInterception), reinterpret_cast<const void*>(&Agent_Network_removeInterception), reinterpret_cast<const void*>(&Agent_Network_interceptContinue), reinterpret_cast<const void*>(&Agent_Network_interceptWithRequest), reinterpret_cast<const void*>(&Agent_Network_interceptWithResponse), reinterpret_cast<const void*>(&Agent_Network_interceptRequestWithResponse), reinterpret_cast<const void*>(&Agent_Network_interceptRequestWithError), nullptr, nullptr};

void APS5_VABI Agent_Page_enable(unsigned char* result, void*) {
    Begin(16, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_disable(unsigned char* result, void*) {
    Begin(16, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_reload(unsigned char* result, void*, const OptionalBoolean* a0, const OptionalBoolean* a1) {
    Begin(16, 2);
    RecordOptional(a0->hasValue, a0->hasValue ? a0->value : 0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_navigate(unsigned char* result, void*, const String* a0) {
    Begin(16, 3);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_overrideUserAgent(unsigned char* result, void*, const String* a0) {
    Begin(16, 4);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_overrideSetting(unsigned char* result, void*, int a0, const OptionalBoolean* a1) {
    Begin(16, 5);
    RecordInteger(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_overrideUserPreference(unsigned char* result, void*, int a0, const OptionalInteger* a1) {
    Begin(16, 6);
    RecordInteger(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_getCookies(unsigned char* result, void*) {
    Begin(16, 7);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Page_setCookie(unsigned char* result, void*, Value** a0) {
    Begin(16, 8);
    RecJson(*a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_deleteCookie(unsigned char* result, void*, const String* a0, const String* a1) {
    Begin(16, 9);
    RecString(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_getResourceTree(unsigned char* result, void*) {
    Begin(16, 10);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Page_getResourceContent(unsigned char* result, void*, const String* a0, const String* a1) {
    Begin(16, 11);
    RecString(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = OutBoolean() ? 1 : 0;
    result[16] = 1;
}

void APS5_VABI Agent_Page_setBootstrapScript(unsigned char* result, void*, const String* a0) {
    Begin(16, 12);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_searchInResource(unsigned char* result, void*, const String* a0, const String* a1, const String* a2, const OptionalBoolean* a3, const OptionalBoolean* a4, const String* a5) {
    Begin(16, 13);
    RecString(a0);
    RecString(a1);
    RecString(a2);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    RecordOptional(a4->hasValue, a4->hasValue ? a4->value : 0);
    RecString(a5);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Page_searchInResources(unsigned char* result, void*, const String* a0, const OptionalBoolean* a1, const OptionalBoolean* a2) {
    Begin(16, 14);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Page_setShowRulers(unsigned char* result, void*, bool a0) {
    Begin(16, 15);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_setShowPaintRects(unsigned char* result, void*, bool a0) {
    Begin(16, 16);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_setEmulatedMedia(unsigned char* result, void*, const String* a0) {
    Begin(16, 17);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Page_snapshotNode(unsigned char* result, void*, int a0) {
    Begin(16, 18);
    RecordInteger(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = 1;
}

void APS5_VABI Agent_Page_snapshotRect(unsigned char* result, void*, int a0, int a1, int a2, int a3, int a4) {
    Begin(16, 19);
    RecordInteger(a0);
    RecordInteger(a1);
    RecordInteger(a2);
    RecordInteger(a3);
    RecordInteger(a4);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteString(result + 0, S(OutString(false)));
    result[8] = 1;
}

void APS5_VABI Agent_Page_setScreenSizeOverride(unsigned char* result, void*, const OptionalInteger* a0, const OptionalInteger* a1) {
    Begin(16, 20);
    RecordOptional(a0->hasValue, a0->hasValue ? a0->value : 0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const PageAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Page_enable), reinterpret_cast<const void*>(&Agent_Page_disable), reinterpret_cast<const void*>(&Agent_Page_reload), reinterpret_cast<const void*>(&Agent_Page_navigate), reinterpret_cast<const void*>(&Agent_Page_overrideUserAgent), reinterpret_cast<const void*>(&Agent_Page_overrideSetting), reinterpret_cast<const void*>(&Agent_Page_overrideUserPreference), reinterpret_cast<const void*>(&Agent_Page_getCookies), reinterpret_cast<const void*>(&Agent_Page_setCookie), reinterpret_cast<const void*>(&Agent_Page_deleteCookie), reinterpret_cast<const void*>(&Agent_Page_getResourceTree), reinterpret_cast<const void*>(&Agent_Page_getResourceContent), reinterpret_cast<const void*>(&Agent_Page_setBootstrapScript), reinterpret_cast<const void*>(&Agent_Page_searchInResource), reinterpret_cast<const void*>(&Agent_Page_searchInResources), reinterpret_cast<const void*>(&Agent_Page_setShowRulers), reinterpret_cast<const void*>(&Agent_Page_setShowPaintRects), reinterpret_cast<const void*>(&Agent_Page_setEmulatedMedia), reinterpret_cast<const void*>(&Agent_Page_snapshotNode), reinterpret_cast<const void*>(&Agent_Page_snapshotRect), reinterpret_cast<const void*>(&Agent_Page_setScreenSizeOverride), nullptr, nullptr};

void APS5_VABI Agent_Runtime_parse(unsigned char* result, void*, const String* a0) {
    Begin(17, 0);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 24, S(ErrorText()));
        return;
    }
    WriteInteger(result + 0, OutEnum(11));
    WriteString(result + 8, S(OutString(true)));
    WriteJson(result + 16, false, OutJson(true));
    result[24] = 1;
}

void APS5_VABI Agent_Runtime_evaluate(unsigned char* result, void*, const String* a0, const String* a1, const OptionalBoolean* a2, const OptionalBoolean* a3, const OptionalInteger* a4, const OptionalBoolean* a5, const OptionalBoolean* a6, const OptionalBoolean* a7, const OptionalBoolean* a8) {
    Begin(17, 1);
    RecString(a0);
    RecString(a1);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    RecordOptional(a4->hasValue, a4->hasValue ? a4->value : 0);
    RecordOptional(a5->hasValue, a5->hasValue ? a5->value : 0);
    RecordOptional(a6->hasValue, a6->hasValue ? a6->value : 0);
    RecordOptional(a7->hasValue, a7->hasValue ? a7->value : 0);
    RecordOptional(a8->hasValue, a8->hasValue ? a8->value : 0);
    if (Fail()) {
        WriteError(result, 24, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    WriteOptionalBoolean(result + 8, OutOptionalBoolean());
    WriteOptionalInteger(result + 12);
    result[24] = 1;
}

void APS5_VABI Agent_Runtime_awaitPromise(void*, const String* a0, const OptionalBoolean* a1, const OptionalBoolean* a2, const OptionalBoolean* a3, CallbackBase** a4) {
    Begin(17, 2);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    if (CallbackFails()) {
        const String error = S(ErrorText());
        _ZN9Inspector17BackendDispatcher12CallbackBase11sendFailureERKN3WTF6StringE(*a4, &error);
    }
}

void APS5_VABI Agent_Runtime_callFunctionOn(unsigned char* result, void*, const String* a0, const String* a1, Value** a2, const OptionalBoolean* a3, const OptionalBoolean* a4, const OptionalBoolean* a5, const OptionalBoolean* a6) {
    Begin(17, 3);
    RecString(a0);
    RecString(a1);
    RecJson(*a2);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    RecordOptional(a4->hasValue, a4->hasValue ? a4->value : 0);
    RecordOptional(a5->hasValue, a5->hasValue ? a5->value : 0);
    RecordOptional(a6->hasValue, a6->hasValue ? a6->value : 0);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    WriteOptionalBoolean(result + 8, OutOptionalBoolean());
    result[16] = 1;
}

void APS5_VABI Agent_Runtime_getPreview(unsigned char* result, void*, const String* a0) {
    Begin(17, 4);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, false, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_getProperties(unsigned char* result, void*, const String* a0, const OptionalBoolean* a1, const OptionalInteger* a2, const OptionalInteger* a3, const OptionalBoolean* a4) {
    Begin(17, 5);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    RecordOptional(a4->hasValue, a4->hasValue ? a4->value : 0);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    WriteJson(result + 8, true, OutJson(true));
    result[16] = 1;
}

void APS5_VABI Agent_Runtime_getDisplayableProperties(unsigned char* result, void*, const String* a0, const OptionalInteger* a1, const OptionalInteger* a2, const OptionalBoolean* a3) {
    Begin(17, 6);
    RecString(a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    if (Fail()) {
        WriteError(result, 16, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    WriteJson(result + 8, true, OutJson(true));
    result[16] = 1;
}

void APS5_VABI Agent_Runtime_getCollectionEntries(unsigned char* result, void*, const String* a0, const String* a1, const OptionalInteger* a2, const OptionalInteger* a3) {
    Begin(17, 7);
    RecString(a0);
    RecString(a1);
    RecordOptional(a2->hasValue, a2->hasValue ? a2->value : 0);
    RecordOptional(a3->hasValue, a3->hasValue ? a3->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_saveResult(unsigned char* result, void*, Value** a0, const OptionalInteger* a1) {
    Begin(17, 8);
    RecJson(*a0);
    RecordOptional(a1->hasValue, a1->hasValue ? a1->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteOptionalInteger(result + 0);
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_setSavedResultAlias(unsigned char* result, void*, const String* a0) {
    Begin(17, 9);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_releaseObject(unsigned char* result, void*, const String* a0) {
    Begin(17, 10);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_releaseObjectGroup(unsigned char* result, void*, const String* a0) {
    Begin(17, 11);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_enable(unsigned char* result, void*) {
    Begin(17, 12);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_disable(unsigned char* result, void*) {
    Begin(17, 13);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_getRuntimeTypesForVariablesAtOffsets(unsigned char* result, void*, Value** a0) {
    Begin(17, 14);
    RecJson(*a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_enableTypeProfiler(unsigned char* result, void*) {
    Begin(17, 15);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_disableTypeProfiler(unsigned char* result, void*) {
    Begin(17, 16);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_enableControlFlowProfiler(unsigned char* result, void*) {
    Begin(17, 17);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_disableControlFlowProfiler(unsigned char* result, void*) {
    Begin(17, 18);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Runtime_getBasicBlocks(unsigned char* result, void*, const String* a0) {
    Begin(17, 19);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    WriteJson(result + 0, true, OutJson(false));
    result[8] = 1;
}

const void* const RuntimeAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Runtime_parse), reinterpret_cast<const void*>(&Agent_Runtime_evaluate), reinterpret_cast<const void*>(&Agent_Runtime_awaitPromise), reinterpret_cast<const void*>(&Agent_Runtime_callFunctionOn), reinterpret_cast<const void*>(&Agent_Runtime_getPreview), reinterpret_cast<const void*>(&Agent_Runtime_getProperties), reinterpret_cast<const void*>(&Agent_Runtime_getDisplayableProperties), reinterpret_cast<const void*>(&Agent_Runtime_getCollectionEntries), reinterpret_cast<const void*>(&Agent_Runtime_saveResult), reinterpret_cast<const void*>(&Agent_Runtime_setSavedResultAlias), reinterpret_cast<const void*>(&Agent_Runtime_releaseObject), reinterpret_cast<const void*>(&Agent_Runtime_releaseObjectGroup), reinterpret_cast<const void*>(&Agent_Runtime_enable), reinterpret_cast<const void*>(&Agent_Runtime_disable), reinterpret_cast<const void*>(&Agent_Runtime_getRuntimeTypesForVariablesAtOffsets), reinterpret_cast<const void*>(&Agent_Runtime_enableTypeProfiler), reinterpret_cast<const void*>(&Agent_Runtime_disableTypeProfiler), reinterpret_cast<const void*>(&Agent_Runtime_enableControlFlowProfiler), reinterpret_cast<const void*>(&Agent_Runtime_disableControlFlowProfiler), reinterpret_cast<const void*>(&Agent_Runtime_getBasicBlocks), nullptr, nullptr};

void APS5_VABI Agent_ScriptProfiler_startTracking(unsigned char* result, void*, const OptionalBoolean* a0) {
    Begin(18, 0);
    RecordOptional(a0->hasValue, a0->hasValue ? a0->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_ScriptProfiler_stopTracking(unsigned char* result, void*) {
    Begin(18, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const ScriptProfilerAgentVtable[] = {reinterpret_cast<const void*>(&Agent_ScriptProfiler_startTracking), reinterpret_cast<const void*>(&Agent_ScriptProfiler_stopTracking), nullptr, nullptr};

void APS5_VABI Agent_Target_setPauseOnStart(unsigned char* result, void*, bool a0) {
    Begin(19, 0);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Target_resume(unsigned char* result, void*, const String* a0) {
    Begin(19, 1);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Target_sendMessageToTarget(unsigned char* result, void*, const String* a0, const String* a1) {
    Begin(19, 2);
    RecString(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const TargetAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Target_setPauseOnStart), reinterpret_cast<const void*>(&Agent_Target_resume), reinterpret_cast<const void*>(&Agent_Target_sendMessageToTarget), nullptr, nullptr};

void APS5_VABI Agent_Timeline_enable(unsigned char* result, void*) {
    Begin(20, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Timeline_disable(unsigned char* result, void*) {
    Begin(20, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Timeline_start(unsigned char* result, void*, const OptionalInteger* a0) {
    Begin(20, 2);
    RecordOptional(a0->hasValue, a0->hasValue ? a0->value : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Timeline_stop(unsigned char* result, void*) {
    Begin(20, 3);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Timeline_setAutoCaptureEnabled(unsigned char* result, void*, bool a0) {
    Begin(20, 4);
    RecordInteger(a0 ? 1 : 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Timeline_setInstruments(unsigned char* result, void*, Value** a0) {
    Begin(20, 5);
    RecJson(*a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const TimelineAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Timeline_enable), reinterpret_cast<const void*>(&Agent_Timeline_disable), reinterpret_cast<const void*>(&Agent_Timeline_start), reinterpret_cast<const void*>(&Agent_Timeline_stop), reinterpret_cast<const void*>(&Agent_Timeline_setAutoCaptureEnabled), reinterpret_cast<const void*>(&Agent_Timeline_setInstruments), nullptr, nullptr};

void APS5_VABI Agent_Worker_enable(unsigned char* result, void*) {
    Begin(21, 0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Worker_disable(unsigned char* result, void*) {
    Begin(21, 1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Worker_initialized(unsigned char* result, void*, const String* a0) {
    Begin(21, 2);
    RecString(a0);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

void APS5_VABI Agent_Worker_sendMessageToWorker(unsigned char* result, void*, const String* a0, const String* a1) {
    Begin(21, 3);
    RecString(a0);
    RecString(a1);
    if (Fail()) {
        WriteError(result, 8, S(ErrorText()));
        return;
    }
    result[8] = 1;
}

const void* const WorkerAgentVtable[] = {reinterpret_cast<const void*>(&Agent_Worker_enable), reinterpret_cast<const void*>(&Agent_Worker_disable), reinterpret_cast<const void*>(&Agent_Worker_initialized), reinterpret_cast<const void*>(&Agent_Worker_sendMessageToWorker), nullptr, nullptr};

const void* const* const AgentVtables[] = {AnimationAgentVtable, ApplicationCacheAgentVtable, AuditAgentVtable, BrowserAgentVtable, CanvasAgentVtable, ConsoleAgentVtable, CSSAgentVtable, DatabaseAgentVtable, DebuggerAgentVtable, DOMAgentVtable, DOMDebuggerAgentVtable, DOMStorageAgentVtable, HeapAgentVtable, InspectorAgentVtable, LayerTreeAgentVtable, NetworkAgentVtable, PageAgentVtable, RuntimeAgentVtable, ScriptProfilerAgentVtable, TargetAgentVtable, TimelineAgentVtable, WorkerAgentVtable};

std::vector<Message>* captured;

void APS5_VABI ChannelDestructor(FrontendChannel*) {
}

NKWebKit::InspectorAbi::ConnectionType APS5_VABI ChannelConnectionType(const FrontendChannel*) {
    return NKWebKit::InspectorAbi::ConnectionType::Remote;
}

void APS5_VABI ChannelSendMessage(FrontendChannel*, const String* message) {
    const Text text = T(*message);
    captured->push_back(Message{text.is8Bit, text.text});
}

void* const ChannelVtable[] = {reinterpret_cast<void*>(&ChannelDestructor), reinterpret_cast<void*>(&ChannelDestructor),
    reinterpret_cast<void*>(&ChannelConnectionType), reinterpret_cast<void*>(&ChannelSendMessage)};

struct Agent {
    const void* const* vptr;
};

struct Backend {
    FrontendChannel channel{ChannelVtable};
    FrontendRouter* router;
    BackendDispatcher* backend;
    std::vector<Message> messages;
    Backend() {
        captured = &messages;
        FrontendRouterRef created = _ZN9Inspector14FrontendRouter6createEv();
        router = std::exchange(created.ptr, nullptr);
        _ZN9Inspector14FrontendRouter15connectFrontendERNS_15FrontendChannelE(router, &channel);
        backend = NKWebKit::InspectorAbi::CreateBackendDispatcher(*router);
    }
    ~Backend() {
        NKWebKit::InspectorAbi::DerefBackendDispatcher(backend);
        _ZN9Inspector14FrontendRouter18disconnectFrontendERNS_15FrontendChannelE(router, &channel);
        FrontendRouterRef(std::exchange(router, nullptr));
    }
    std::vector<Message> Dispatch(const std::u16string& text, bool is8Bit) {
        messages.clear();
        const String message = S(Text{text, false, is8Bit});
        _ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE(backend, &message);
        return messages;
    }
};

struct ExportAdapter : Backend {
    std::array<Agent, std::size(DomainExports)> agents{};
    std::vector<DomainBackendDispatcher*> dispatchers;
    ExportAdapter() {
        for (std::size_t i = 0; i < std::size(DomainExports); ++i) {
            agents[i].vptr = AgentVtables[i];
            auto* dispatcher = static_cast<DomainBackendDispatcher*>(_ZN3WTF10fastMallocEm(sizeof(DomainBackendDispatcher)));
            DomainExports[i].complete(dispatcher, backend, &agents[i]);
            dispatchers.push_back(dispatcher);
        }
    }
    ~ExportAdapter() {
        for (DomainBackendDispatcher* dispatcher : dispatchers) NKWebKit::InspectorAbi::DerefDomainBackendDispatcher(dispatcher);
    }
};

std::string Narrow(const std::u16string& text) {
    return std::string(text.begin(), text.end());
}

std::string Send(Backend& backend, const std::string& message) {
    const auto sent = backend.Dispatch(std::u16string(message.begin(), message.end()), true);
    return sent.size() == 1 ? Narrow(sent[0].text) : std::string();
}

void TestTranscripts() {
    for (std::uint32_t seed = 1; seed <= std::size(ExpectedHashes); ++seed) {
        const int before = guestAllocations;
        {
            ExportAdapter adapter;
            Require(Run(adapter, seed * 2654435761u, 400) == ExpectedHashes[seed - 1]);
        }
        Require(guestAllocations == before);
    }
}

void TestExamples() {
    ExportAdapter adapter;
    Require(Send(adapter, "{\"id\":4,\"method\":\"Debugger.nope\"}") ==
        "{\"error\":{\"code\":-32601,\"message\":\"'Debugger.nope' was not found\",\"data\":[{\"code\":-32601,\"message\":\"'Debugger.nope' was not found\"}]},\"id\":4}");
    Require(Send(adapter, "{\"id\":5,\"method\":\"DOMDebugger.setDOMBreakpoint\",\"params\":{\"nodeId\":1,\"type\":\"bogus\"}}") ==
        "{\"error\":{\"code\":-32000,\"message\":\"Unknown type: bogus\",\"data\":[{\"code\":-32000,\"message\":\"Unknown type: bogus\"}]},\"id\":5}");
    Require(Send(adapter, "{\"id\":6,\"method\":\"Timeline.setAutoCaptureEnabled\"}") ==
        "{\"error\":{\"code\":-32602,\"message\":\"Some arguments of method 'Timeline.setAutoCaptureEnabled' can't be processed\",\"data\":[{\"code\":-32602,\"message\":\"'params' object must contain required parameter 'enabled' with type 'Boolean'.\"},{\"code\":-32602,\"message\":\"Some arguments of method 'Timeline.setAutoCaptureEnabled' can't be processed\"}]},\"id\":6}");
}

void RequireVtable(const DomainBackendDispatcher* dispatcher, const void* expected) {
    Require(dispatcher->vptr == expected);
    const auto* prefix = reinterpret_cast<const std::uintptr_t*>(dispatcher->vptr);
    Require(prefix[-2] == 0 && prefix[-1] == 0);
    Require(dispatcher->vptr->completeDestructor && dispatcher->vptr->deletingDestructor && dispatcher->vptr->dispatch);
}

std::string Unknown(const DomainExport& domain) {
    return std::string("{\"id\":3,\"method\":\"") + domain.name + ".noSuchCommand\"}";
}

std::string NotFound(const DomainExport& domain, bool registered) {
    const std::string message = registered ? std::string("'") + domain.name + ".noSuchCommand' was not found" : std::string("'") + domain.name + "' domain was not found";
    return "{\"error\":{\"code\":-32601,\"message\":\"" + message + "\",\"data\":[{\"code\":-32601,\"message\":\"" + message + "\"}]},\"id\":3}";
}

void TestCreate(const DomainExport& domain, const void*& vtable) {
    const int before = guestAllocations;
    {
        Backend backend;
        Require(Send(backend, Unknown(domain)) == NotFound(domain, false));
        Agent agent{nullptr};
        {
            DomainBackendDispatcherRef dispatcher = domain.create(backend.backend, &agent);
            Require(dispatcher.ptr && dispatcher.ptr->refCount == 1 && dispatcher.ptr->agent == &agent);
            Require(dispatcher.ptr->backendDispatcher == backend.backend && backend.backend->refCount == 2);
            vtable = dispatcher.ptr->vptr;
            RequireVtable(dispatcher.ptr, vtable);
            Require(Send(backend, Unknown(domain)) == NotFound(domain, true));
            Require(dispatcher.ptr->refCount == 1 && backend.backend->refCount == 2);
        }
        Require(backend.backend->refCount == 1);
    }
    Require(guestAllocations == before);
}

void TestConstructors(const DomainExport& domain, const void* vtable) {
    const int before = guestAllocations;
    {
        Backend backend;
        for (ConstructFunction construct : {domain.complete, domain.base}) {
            if (!construct) continue;
            auto* dispatcher = static_cast<DomainBackendDispatcher*>(_ZN3WTF10fastMallocEm(sizeof(DomainBackendDispatcher)));
            construct(dispatcher, backend.backend, nullptr);
            Require(dispatcher->refCount == 1 && !dispatcher->agent && dispatcher->backendDispatcher == backend.backend);
            Require(backend.backend->refCount == 2);
            RequireVtable(dispatcher, vtable ? vtable : dispatcher->vptr);
            Require(Send(backend, Unknown(domain)) == NotFound(domain, true));
            if (domain.deleting) domain.deleting(dispatcher);
            else dispatcher->vptr->deletingDestructor(reinterpret_cast<SupplementalBackendDispatcher*>(dispatcher));
            Require(backend.backend->refCount == 1);
        }
        auto* dispatcher = static_cast<DomainBackendDispatcher*>(_ZN3WTF10fastMallocEm(sizeof(DomainBackendDispatcher)));
        (domain.base ? domain.base : domain.complete)(dispatcher, backend.backend, nullptr);
        dispatcher->vptr->completeDestructor(reinterpret_cast<SupplementalBackendDispatcher*>(dispatcher));
        Require(backend.backend->refCount == 1 && !dispatcher->backendDispatcher);
        _ZN3WTF8fastFreeEPv(dispatcher);
    }
    Require(guestAllocations == before);
}

void TestNullArguments(const DomainExport& domain) {
    Backend backend;
    DomainBackendDispatcher storage{};
    if (domain.create) Require(Throws<std::invalid_argument>([&] { domain.create(nullptr, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { domain.complete(nullptr, backend.backend, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { domain.complete(&storage, nullptr, nullptr); }));
    if (domain.base) Require(Throws<std::invalid_argument>([&] { domain.base(nullptr, backend.backend, nullptr); }));
    if (domain.deleting) Require(Throws<std::invalid_argument>([&] { domain.deleting(nullptr); }));
    Require(backend.backend->refCount == 1 && !storage.vptr);
}

void TestNullHandler() {
    Backend backend;
    auto* dispatcher = static_cast<DomainBackendDispatcher*>(_ZN3WTF10fastMallocEm(sizeof(DomainBackendDispatcher)));
    DomainExports[0].complete(dispatcher, backend.backend, nullptr);
    const String message = S(Text{u"{\"id\":1,\"method\":\"Animation.enable\"}", false, true});
    Require(Throws<std::invalid_argument>([&] { _ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE(backend.backend, &message); }));
    Require(dispatcher->refCount == 1 && backend.backend->refCount == 2);
    NKWebKit::InspectorAbi::DerefDomainBackendDispatcher(dispatcher);
}

void TestDistinctVtables() {
    Backend backend;
    std::vector<const void*> seen;
    for (const auto& domain : DomainExports) {
        auto* dispatcher = static_cast<DomainBackendDispatcher*>(_ZN3WTF10fastMallocEm(sizeof(DomainBackendDispatcher)));
        domain.complete(dispatcher, backend.backend, nullptr);
        for (const void* other : seen) Require(other != dispatcher->vptr);
        seen.push_back(dispatcher->vptr);
        dispatcher->vptr->deletingDestructor(reinterpret_cast<SupplementalBackendDispatcher*>(dispatcher));
    }
    Require(seen.size() == 22 && backend.backend->refCount == 1);
}

}  // namespace

int main() {
    RegisterHeap();
    TestTranscripts();
    TestExamples();
    for (const auto& domain : DomainExports) {
        const void* vtable = nullptr;
        if (domain.create) TestCreate(domain, vtable);
        TestConstructors(domain, vtable);
        TestNullArguments(domain);
    }
    TestNullHandler();
    TestDistinctVtables();
    std::puts("Inspector domain dispatcher tests passed");
    return 0;
}
