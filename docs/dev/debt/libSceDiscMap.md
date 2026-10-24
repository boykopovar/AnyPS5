# libSceDiscMap technical debt

### Functional

- [libSceDiscMap](../../../core/libs/prx/libSceDiscMap/Export.cpp): loading disc bitmap metadata and resolving file byte ranges to bitmap blocks are not implemented. All three exports implement the confirmed no-bitmap path: return `SCE_DISC_MAP_ERROR_NO_BITMAP_INFO` before validating any argument and leave every output unchanged. The supplied module confirms signed 64-bit byte offset/size, a 32-bit `IsRequestOnHDD` result and three 64-bit outputs for `ioKMruft1ek`. The latter outputs describe the bitmap selector, first block and block count. Its exported source name remains unavailable.
