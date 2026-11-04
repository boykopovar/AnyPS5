#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

using PlacementNew = void* (APS5_VABI*)(std::size_t, void*);
using NotNullPlacementNew = void* (APS5_VABI*)(std::size_t, unsigned int, void*);

struct Entry {
    const char* name;
    PlacementNew function;
};

struct NotNullEntry {
    const char* name;
    NotNullPlacementNew function;
};

static const Entry entries[] = {
    {"_ZN3JSC19HeapSnapshotBuildernaEmPv", &_ZN3JSC19HeapSnapshotBuildernaEmPv},
    {"_ZN3JSC19HeapSnapshotBuildernwEmPv", &_ZN3JSC19HeapSnapshotBuildernwEmPv},
    {"_ZN3JSC4Yarr17RegularExpressionnaEmPv", &_ZN3JSC4Yarr17RegularExpressionnaEmPv},
    {"_ZN3JSC4Yarr17RegularExpressionnwEmPv", &_ZN3JSC4Yarr17RegularExpressionnwEmPv},
    {"_ZN7bmalloc11IsoTLSEntrynaEmPv", &_ZN7bmalloc11IsoTLSEntrynaEmPv},
    {"_ZN7bmalloc11IsoTLSEntrynwEmPv", &_ZN7bmalloc11IsoTLSEntrynwEmPv},
    {"_ZN7bmalloc15IsoHeapImplBasenaEmPv", &_ZN7bmalloc15IsoHeapImplBasenaEmPv},
    {"_ZN9Inspector14ConsoleMessagenaEmPv", &_ZN9Inspector14ConsoleMessagenaEmPv},
    {"_ZN9Inspector14ConsoleMessagenwEmPv", &_ZN9Inspector14ConsoleMessagenwEmPv},
    {"_ZN9Inspector14InspectorAgentnaEmPv", &_ZN9Inspector14InspectorAgentnaEmPv},
    {"_ZN9Inspector14InspectorAgentnwEmPv", &_ZN9Inspector14InspectorAgentnwEmPv},
    {"_ZN9Inspector18InspectorHeapAgentnaEmPv", &_ZN9Inspector18InspectorHeapAgentnaEmPv},
    {"_ZN9Inspector18InspectorHeapAgentnwEmPv", &_ZN9Inspector18InspectorHeapAgentnwEmPv},
    {"_ZN9Inspector19InspectorAuditAgentnaEmPv", &_ZN9Inspector19InspectorAuditAgentnaEmPv},
    {"_ZN9Inspector19InspectorAuditAgentnwEmPv", &_ZN9Inspector19InspectorAuditAgentnwEmPv},
    {"_ZN9Inspector20InspectorTargetAgentnaEmPv", &_ZN9Inspector20InspectorTargetAgentnaEmPv},
    {"_ZN9Inspector20InspectorTargetAgentnwEmPv", &_ZN9Inspector20InspectorTargetAgentnwEmPv},
    {"_ZN9Inspector21InjectedScriptManagernaEmPv", &_ZN9Inspector21InjectedScriptManagernaEmPv},
    {"_ZN9Inspector21InjectedScriptManagernwEmPv", &_ZN9Inspector21InjectedScriptManagernwEmPv},
    {"_ZN9Inspector21InspectorConsoleAgentnaEmPv", &_ZN9Inspector21InspectorConsoleAgentnaEmPv},
    {"_ZN9Inspector21InspectorConsoleAgentnwEmPv", &_ZN9Inspector21InspectorConsoleAgentnwEmPv},
    {"_ZN9Inspector21InspectorRuntimeAgentnaEmPv", &_ZN9Inspector21InspectorRuntimeAgentnaEmPv},
    {"_ZN9Inspector21InspectorRuntimeAgentnwEmPv", &_ZN9Inspector21InspectorRuntimeAgentnwEmPv},
    {"_ZN9Inspector28InspectorScriptProfilerAgentnaEmPv", &_ZN9Inspector28InspectorScriptProfilerAgentnaEmPv},
    {"_ZN9Inspector28InspectorScriptProfilerAgentnwEmPv", &_ZN9Inspector28InspectorScriptProfilerAgentnwEmPv},
};

static const NotNullEntry notNullEntries[] = {
    {"_ZN3JSC19HeapSnapshotBuildernwEm10NotNullTagPv", &_ZN3JSC19HeapSnapshotBuildernwEm10NotNullTagPv},
    {"_ZN3JSC4Yarr17RegularExpressionnwEm10NotNullTagPv", &_ZN3JSC4Yarr17RegularExpressionnwEm10NotNullTagPv},
    {"_ZN9Inspector14InspectorAgentnwEm10NotNullTagPv", &_ZN9Inspector14InspectorAgentnwEm10NotNullTagPv},
    {"_ZN9Inspector18InspectorHeapAgentnwEm10NotNullTagPv", &_ZN9Inspector18InspectorHeapAgentnwEm10NotNullTagPv},
    {"_ZN9Inspector21InjectedScriptManagernwEm10NotNullTagPv", &_ZN9Inspector21InjectedScriptManagernwEm10NotNullTagPv},
    {"_ZN9Inspector21InspectorConsoleAgentnwEm10NotNullTagPv", &_ZN9Inspector21InspectorConsoleAgentnwEm10NotNullTagPv},
    {"_ZN9Inspector21InspectorRuntimeAgentnwEm10NotNullTagPv", &_ZN9Inspector21InspectorRuntimeAgentnwEm10NotNullTagPv},
    {"_ZN9Inspector28InspectorScriptProfilerAgentnwEm10NotNullTagPv", &_ZN9Inspector28InspectorScriptProfilerAgentnwEm10NotNullTagPv},
};

static void Check(bool value, const char* name, int line) {
    if (!value) {
        std::fprintf(stderr, "placement new check failed for %s at line %d\n", name, line);
        std::abort();
    }
}

int main() {
    alignas(16) unsigned char buffer[256];
    const std::size_t sizes[] = {0, 1, 8, 64, sizeof(buffer), ~static_cast<std::size_t>(0)};
    for (const Entry& entry : entries) {
        for (std::size_t size : sizes) {
            Check(entry.function(size, buffer) == buffer, entry.name, __LINE__);
            Check(entry.function(size, buffer + 1) == buffer + 1, entry.name, __LINE__);
        }
        Check(entry.function(8, nullptr) == nullptr, entry.name, __LINE__);
    }
    for (const NotNullEntry& entry : notNullEntries) {
        for (std::size_t size : sizes) {
            Check(entry.function(size, 0, buffer) == buffer, entry.name, __LINE__);
            Check(entry.function(size, 0, buffer + 1) == buffer + 1, entry.name, __LINE__);
        }
    }
    Check(sizeof(entries) / sizeof(entries[0]) == 25, "entries", __LINE__);
    Check(sizeof(notNullEntries) / sizeof(notNullEntries[0]) == 8, "notNullEntries", __LINE__);
    return 0;
}
