#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "Testing.hpp"

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

int main() {
    alignas(16) unsigned char buffer[256];
    const std::size_t sizes[] = {0, 1, 8, 64, sizeof(buffer), ~static_cast<std::size_t>(0)};
    for (const Entry& entry : entries) {
        for (std::size_t size : sizes) {
            Require(entry.function(size, buffer) == buffer, entry.name);
            Require(entry.function(size, buffer + 1) == buffer + 1, entry.name);
        }
        Require(entry.function(8, nullptr) == nullptr, entry.name);
    }
    for (const NotNullEntry& entry : notNullEntries) {
        for (std::size_t size : sizes) {
            Require(entry.function(size, 0, buffer) == buffer, entry.name);
            Require(entry.function(size, 0, buffer + 1) == buffer + 1, entry.name);
        }
    }
    Require(sizeof(entries) / sizeof(entries[0]) == 2, "entries");
    Require(sizeof(notNullEntries) / sizeof(notNullEntries[0]) == 1, "notNullEntries");
    return 0;
}
