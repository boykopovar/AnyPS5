# libSceVideoOut

## add Functional

- [sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion, sceVideoOutAdjustColorSpaceConversion, sceVideoOutAdjustColor](../../../core/libs/prx/libSceVideoOut/src/Output.cpp) (libSceVideoOut) - validating guest writability and copying settings via `GuestCopyIn_nid_no_patch`/`GuestWritable_nid_no_patch` while preserving host `errno` is a host-safety boundary; whether retail console firmware probes unmapped memory pointers before driver dispatch is unmeasured.
