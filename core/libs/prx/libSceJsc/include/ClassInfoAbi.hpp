#pragma once

#include <cstddef>
#include <type_traits>

namespace Jsc::JscAbi {

struct HashTablePrefix {
    int numberOfValues;
    int indexMask;
    bool hasSetterOrReadonlyProperties;
};

struct ClassInfoPrefix {
    const char* className;
    const ClassInfoPrefix* parentClass;
    const HashTablePrefix* staticPropHashTable;
};

static_assert(sizeof(void*) == 8);
static_assert(sizeof(int) == 4);
static_assert(sizeof(bool) == 1);
static_assert(std::is_standard_layout_v<ClassInfoPrefix>);
static_assert(std::is_standard_layout_v<HashTablePrefix>);
static_assert(offsetof(ClassInfoPrefix, parentClass) == 8);
static_assert(offsetof(ClassInfoPrefix, staticPropHashTable) == 16);
static_assert(offsetof(HashTablePrefix, hasSetterOrReadonlyProperties) == 8);

} // namespace Jsc::JscAbi
