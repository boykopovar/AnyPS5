#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

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
    {"_ZN7bmalloc15IsoHeapImplBasenwEmPv", &_ZN7bmalloc15IsoHeapImplBasenwEmPv},
    {"_ZN9Inspector22InspectorDebuggerAgentnwEmPv", &_ZN9Inspector22InspectorDebuggerAgentnwEmPv},
};

static const NotNullEntry notNullEntries[] = {
    {"_ZN9Inspector14ConsoleMessagenwEm10NotNullTagPv", &_ZN9Inspector14ConsoleMessagenwEm10NotNullTagPv},
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
    Check(sizeof(entries) / sizeof(entries[0]) == 2, "entries", __LINE__);
    Check(sizeof(notNullEntries) / sizeof(notNullEntries[0]) == 1, "notNullEntries", __LINE__);
    return 0;
}
