# libSceUrlConfigResolver

## remove [`sceUrlConfigResolverGetReleaseVersion`]

## change [sceUrlConfigResolverGetDefaultQueryParameter and sceUrlConfigResolverGetDeviceId]

- [sceUrlConfigResolverGetDefaultQueryParameter](../../../core/libs/prx/libSceUrlConfigResolver/Export.cpp) (libSceUrlConfigResolver) - name and NID are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceUrlConfigResolver.c), and firmware 13.42's module does not export it ([reading](https://github.com/boykopovar/AnyPS5/issues/21867#issuecomment-6948791843)); no credible prototype was found. The declaration has no parameters solely to resolve known imports; this is not an ABI claim and callers must not call it until its signature is verified

## change [Configuration loading]

- [Configuration loading](../../../core/libs/prx/libSceUrlConfigResolver/Export.cpp) (libSceUrlConfigResolver) - not modelled: configuration read from `/SceUrlConfigResolverShm` (published by another process). From 13.42 disassembly ([reading](https://github.com/boykopovar/AnyPS5/issues/21867#issuecomment-6948791843)), not measured on a console, only pre-load steps are answered: `sceUrlConfigResolverUpdate` and `sceUrlConfigResolverGetJscHeapSizeSoftLimit` return `0x80BA8080` for null arguments; `sceUrlConfigResolverGetAppConfig` clears a non-null buffer (`size` bytes) and zeroes a non-null length, then returns `0x80BA8080` for a null key; `sceUrlConfigResolverGetDeviceId`, `sceUrlConfigResolverGetReleaseVersion` and `sceUrlConfigResolverGetSysVer` clear a non-null buffer and zero a non-null length before loading; `sceUrlConfigResolverIsUpdated` returns false (flag set only on load). Past those steps the seven throw; unwritable outputs throw here.
