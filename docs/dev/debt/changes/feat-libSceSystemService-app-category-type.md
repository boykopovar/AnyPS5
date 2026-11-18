# libSceSystemService

## add Unknown function info

- [sceLncUtilGetAppCategoryType, sceSystemServiceGetAppCategoryType](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - for the running title they write its `param.json` `applicationCategoryType`, as [sceKernelGetAppCategoryType](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) does. A null output returns `0x80A10003` from `sceSystemServiceGetAppCategoryType` and `0x80940005` from `sceLncUtilGetAppCategoryType`, and an unwritable one returns the same; those results are host-safety choices and are unmeasured. Another app ID throws from `sceLncUtilGetAppCategoryType`, because the console's answer for it is unknown, and `sceSystemServiceGetAppCategoryType` returns 0x8094000f for it, as the app queries entry says. Both throw for a title whose `param.json` has no `applicationCategoryType`. The `(int appId, uint32_t*)` signatures are the ones the pending exports recovered.
