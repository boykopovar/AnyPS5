# libSceDiscMap technical debt

### Functional

- [libSceDiscMap](../../../core/libs/prx/libSceDiscMap/Export.cpp): loading disc bitmap metadata and resolving file byte ranges to bitmap blocks are not implemented. Both exports implement the confirmed no-bitmap path: return `SCE_DISC_MAP_ERROR_NO_BITMAP_INFO` before validating any argument and leave every output unchanged. The supplied module confirms signed 64-bit byte offset/size, a 32-bit `IsRequestOnHDD` result and three 64-bit outputs for `ioKMruft1ek`. The latter outputs describe the bitmap selector, first block and block count. Its exported source name remains unavailable.

### Unknown function info

- [sceDiscMapGetPackageSize](../../../core/libs/prx/libSceDiscMap/Export.cpp) (libSceDiscMap) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDiscMap.c); the `(int64_t, int*, int*)` prototype follows [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/disc_map/disc_map.cpp#L12), is not measured on a console, and every call throws
