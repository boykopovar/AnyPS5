# libSceDepth2

## add Functional

- [sceDepth2QueryMemory](../../../core/libs/prx/libSceDepth2/Export.cpp) (libSceDepth2) - copying parameter inputs via `GuestCopyIn_nid_no_patch` and memory outputs via `GuestCopyOut_nid_no_patch`, returning `0x80BB000A` (`ErrorNullPointer`) while preserving `errno` for unmapped or unwritable guest pointers, is a host-safety fallback; console fault behavior on invalid buffers has not been measured.
