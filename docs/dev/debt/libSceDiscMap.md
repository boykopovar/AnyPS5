# libSceDiscMap technical debt

### Functional

- [libSceDiscMap](../../../core/libs/prx/libSceDiscMap/Export.cpp): loading disc bitmap metadata and resolving file byte ranges to bitmap blocks are not implemented. Both exports implement the confirmed no-bitmap path: return `SCE_DISC_MAP_ERROR_NO_BITMAP_INFO` before validating any argument and leave every output unchanged. The supplied module confirms signed 64-bit byte offset/size, a 32-bit `IsRequestOnHDD` result and three 64-bit outputs for `ioKMruft1ek`. The latter outputs describe the bitmap selector, first block and block count. Its exported source name remains unavailable.

### Unknown function info

- [sceDiscMapGetPackageSize](../../../core/libs/prx/libSceDiscMap/Export.cpp) (libSceDiscMap) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDiscMap.c); the `(int64_t, int*, int*)` prototype follows [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/f68923fe807f657348e6719ce658bcbd0a2b1c72/src/core/libraries/disc_map/disc_map.cpp#L12), is not measured on a console, and every call throws

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceDiscMap/Export.cpp) - 2 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `5+vOlukvkfg` |
| `Unknown` | `fJgP+wqifno` |
