#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "Testing.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

using PlacementNew = void* (APS5_VABI*)(std::size_t, void*);

struct Entry {
    const char* name;
    PlacementNew function;
};

static const Entry entries[] = {
    {"_ZN9Inspector21CSSFrontendDispatchernaEmPv", &_ZN9Inspector21CSSFrontendDispatchernaEmPv},
    {"_ZN9Inspector21CSSFrontendDispatchernwEmPv", &_ZN9Inspector21CSSFrontendDispatchernwEmPv},
    {"_ZN9Inspector21DOMFrontendDispatchernaEmPv", &_ZN9Inspector21DOMFrontendDispatchernaEmPv},
    {"_ZN9Inspector21DOMFrontendDispatchernwEmPv", &_ZN9Inspector21DOMFrontendDispatchernwEmPv},
    {"_ZN9Inspector22HeapFrontendDispatchernaEmPv", &_ZN9Inspector22HeapFrontendDispatchernaEmPv},
    {"_ZN9Inspector22HeapFrontendDispatchernwEmPv", &_ZN9Inspector22HeapFrontendDispatchernwEmPv},
    {"_ZN9Inspector22PageFrontendDispatchernaEmPv", &_ZN9Inspector22PageFrontendDispatchernaEmPv},
    {"_ZN9Inspector22PageFrontendDispatchernwEmPv", &_ZN9Inspector22PageFrontendDispatchernwEmPv},
    {"_ZN9Inspector24CanvasFrontendDispatchernaEmPv", &_ZN9Inspector24CanvasFrontendDispatchernaEmPv},
    {"_ZN9Inspector24CanvasFrontendDispatchernwEmPv", &_ZN9Inspector24CanvasFrontendDispatchernwEmPv},
    {"_ZN9Inspector24TargetFrontendDispatchernaEmPv", &_ZN9Inspector24TargetFrontendDispatchernaEmPv},
    {"_ZN9Inspector24TargetFrontendDispatchernwEmPv", &_ZN9Inspector24TargetFrontendDispatchernwEmPv},
    {"_ZN9Inspector24WorkerFrontendDispatchernaEmPv", &_ZN9Inspector24WorkerFrontendDispatchernaEmPv},
    {"_ZN9Inspector24WorkerFrontendDispatchernwEmPv", &_ZN9Inspector24WorkerFrontendDispatchernwEmPv},
    {"_ZN9Inspector25BrowserFrontendDispatchernaEmPv", &_ZN9Inspector25BrowserFrontendDispatchernaEmPv},
    {"_ZN9Inspector25BrowserFrontendDispatchernwEmPv", &_ZN9Inspector25BrowserFrontendDispatchernwEmPv},
    {"_ZN9Inspector25ConsoleFrontendDispatchernaEmPv", &_ZN9Inspector25ConsoleFrontendDispatchernaEmPv},
    {"_ZN9Inspector25ConsoleFrontendDispatchernwEmPv", &_ZN9Inspector25ConsoleFrontendDispatchernwEmPv},
    {"_ZN9Inspector25NetworkFrontendDispatchernaEmPv", &_ZN9Inspector25NetworkFrontendDispatchernaEmPv},
    {"_ZN9Inspector25NetworkFrontendDispatchernwEmPv", &_ZN9Inspector25NetworkFrontendDispatchernwEmPv},
    {"_ZN9Inspector25RuntimeFrontendDispatchernaEmPv", &_ZN9Inspector25RuntimeFrontendDispatchernaEmPv},
    {"_ZN9Inspector25RuntimeFrontendDispatchernwEmPv", &_ZN9Inspector25RuntimeFrontendDispatchernwEmPv},
    {"_ZN9Inspector26DatabaseFrontendDispatchernaEmPv", &_ZN9Inspector26DatabaseFrontendDispatchernaEmPv},
    {"_ZN9Inspector26DatabaseFrontendDispatchernwEmPv", &_ZN9Inspector26DatabaseFrontendDispatchernwEmPv},
    {"_ZN9Inspector26DebuggerFrontendDispatchernaEmPv", &_ZN9Inspector26DebuggerFrontendDispatchernaEmPv},
    {"_ZN9Inspector26DebuggerFrontendDispatchernwEmPv", &_ZN9Inspector26DebuggerFrontendDispatchernwEmPv},
    {"_ZN9Inspector26TimelineFrontendDispatchernaEmPv", &_ZN9Inspector26TimelineFrontendDispatchernaEmPv},
    {"_ZN9Inspector26TimelineFrontendDispatchernwEmPv", &_ZN9Inspector26TimelineFrontendDispatchernwEmPv},
    {"_ZN9Inspector27AnimationFrontendDispatchernaEmPv", &_ZN9Inspector27AnimationFrontendDispatchernaEmPv},
    {"_ZN9Inspector27AnimationFrontendDispatchernwEmPv", &_ZN9Inspector27AnimationFrontendDispatchernwEmPv},
    {"_ZN9Inspector27InspectorFrontendDispatchernaEmPv", &_ZN9Inspector27InspectorFrontendDispatchernaEmPv},
    {"_ZN9Inspector27InspectorFrontendDispatchernwEmPv", &_ZN9Inspector27InspectorFrontendDispatchernwEmPv},
    {"_ZN9Inspector27LayerTreeFrontendDispatchernaEmPv", &_ZN9Inspector27LayerTreeFrontendDispatchernaEmPv},
    {"_ZN9Inspector27LayerTreeFrontendDispatchernwEmPv", &_ZN9Inspector27LayerTreeFrontendDispatchernwEmPv},
    {"_ZN9Inspector28DOMStorageFrontendDispatchernaEmPv", &_ZN9Inspector28DOMStorageFrontendDispatchernaEmPv},
    {"_ZN9Inspector28DOMStorageFrontendDispatchernwEmPv", &_ZN9Inspector28DOMStorageFrontendDispatchernwEmPv},
    {"_ZN9Inspector32ScriptProfilerFrontendDispatchernaEmPv", &_ZN9Inspector32ScriptProfilerFrontendDispatchernaEmPv},
    {"_ZN9Inspector32ScriptProfilerFrontendDispatchernwEmPv", &_ZN9Inspector32ScriptProfilerFrontendDispatchernwEmPv},
    {"_ZN9Inspector34ApplicationCacheFrontendDispatchernaEmPv", &_ZN9Inspector34ApplicationCacheFrontendDispatchernaEmPv},
    {"_ZN9Inspector34ApplicationCacheFrontendDispatchernwEmPv", &_ZN9Inspector34ApplicationCacheFrontendDispatchernwEmPv},
};

int main() {
    alignas(16) unsigned char buffer[256];
    const std::size_t sizes[] = {0, 1, 8, 64, sizeof(buffer), ~static_cast<std::size_t>(0)};
    for (const Entry& entry : entries) {
        for (std::size_t size : sizes) {
            Require(entry.function(size, buffer) == buffer, entry.name);
            Require(entry.function(size, buffer + 8) == buffer + 8, entry.name);
        }
        Require(entry.function(sizeof(buffer), nullptr) == nullptr, entry.name);
    }
    Require(sizeof(entries) / sizeof(entries[0]) == 40, "entries");
    return 0;
}
