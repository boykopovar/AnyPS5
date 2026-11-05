# libSceDiscMap technical debt

### Functional

- [libSceDiscMap](../../../core/libs/prx/libSceDiscMap/Export.cpp): loading disc bitmap metadata and resolving file byte ranges to bitmap blocks are not implemented. `sceDiscMapIsRequestOnHDD`, `ioKMruft1ek` and `sceDiscMapGetPackageSize` implement the confirmed no-bitmap path: return `SCE_DISC_MAP_ERROR_NO_BITMAP_INFO` (`0x81100004`) before validating any argument and leave outputs unchanged. The supplied module confirms signed 64-bit byte offset/size, a 32-bit `IsRequestOnHDD` result and three 64-bit outputs for `ioKMruft1ek`. A PC run of the firmware 13.42 module on x86-64 (#8647 comment 6416230506, not a console measurement) confirms `sceDiscMapGetPackageSize` returns `0x81100004` before argument checks while its bitmap-state word is clear; its loaded state and bitmap loading remain unimplemented.

### Unknown function info

- [sceDiscMapGetPackageSize](../../../core/libs/prx/libSceDiscMap/Export.cpp) (libSceDiscMap) - name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDiscMap.c). A PC run of the 13.42 module (#8647 comment 6416230506, not a console measurement) establishes two 64-bit outputs (`uint64_t*`). The unloaded state returns `0x81100004` without writing outputs; the loaded state has other results and remains unimplemented.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceDiscMap/Export.cpp) - 2 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `5+vOlukvkfg` |
| `Unknown` | `fJgP+wqifno` |
