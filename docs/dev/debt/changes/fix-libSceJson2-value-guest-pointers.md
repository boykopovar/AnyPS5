# libSceJson2

## add Unknown 3.20 import declarations

- [Value guest pointer validation](../../../core/libs/prx/libSceJson2/Export.cpp) - console behaviour on null, inaccessible, read-only or partially overlapping `Value` guest pointers is unmeasured. Closes when: console behaviour on invalid `Value` pointers is measured.
