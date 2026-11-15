# libSceNpGameIntent

## add Functional

- [sceNpGameIntentGetPropertyValueString guest pointers](../../../core/libs/prx/libSceNpGameIntent/Export.cpp) - reading the first byte of `key` and writing the terminator to `valueBuf` through checked guest copies, returning `0x80553804` with caller `errno` preserved on unreadable or unwritable pointers, is a host-safety fallback; console behaviour on unmapped memory is unmeasured.
