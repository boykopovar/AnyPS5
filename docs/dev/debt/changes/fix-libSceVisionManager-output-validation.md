# libSceVisionManager

## change [sceVisionManagerGetWorkingMemorySize]

- [sceVisionManagerGetWorkingMemorySize](../../../core/libs/prx/libSceVisionManager/Export.cpp) (libSceVisionManager) - returns `0x80E70000` when output pointers are null or unwritable via `GuestWritable_nid_no_patch`, preserving host errno; console behavior on unmapped pointers is not measured.
