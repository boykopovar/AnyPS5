#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

namespace Jsc::JscAbi {

struct HashTable {
    int numberOfValues;
    int indexMask;
    bool hasSetterOrReadonlyProperties;
};

struct ClassInfo {
    const char* className;
    const ClassInfo* parentClass;
    const HashTable* staticPropHashTable;
};

static_assert(offsetof(HashTable, numberOfValues) == 0);
static_assert(offsetof(HashTable, indexMask) == 4);
static_assert(offsetof(HashTable, hasSetterOrReadonlyProperties) == 8);
static_assert(offsetof(ClassInfo, className) == 0);
static_assert(offsetof(ClassInfo, parentClass) == 8);
static_assert(offsetof(ClassInfo, staticPropHashTable) == 16);

} // namespace Jsc::JscAbi

extern "C" {

bool APS5_VABI _ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv(const Jsc::JscAbi::ClassInfo* self);

}
