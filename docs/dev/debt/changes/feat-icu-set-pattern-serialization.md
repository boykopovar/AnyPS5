# libSceIcu

## add Set property and serialization operations

- [Set property and serialization operations](../../../core/libs/prx/libSceIcu/Set.cpp) implement `uset_open`, `uset_toPattern`, `uset_applyIntPropertyValue`, `uset_applyPropertyAlias`, `uset_spanBackUTF8`, `uset_serialize`, `uset_getSerializedSet`, `uset_setSerializedToOne`, `uset_serializedContains`, `uset_getSerializedRangeCount` and `uset_getSerializedRange` through bundled ICU 78.1. The console export availability, ABI and behavior are unverified. Null and inaccessible guest pointers throw where the console would fault. Closes when: compared with the console.
