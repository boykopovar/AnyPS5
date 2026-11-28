# libSceNpCommon

## remove [sceNpGetSandboxMode]

## remove [sceNpUtilGetSandboxMode]

## remove [sceNpUtilGetDateSetAuto]

## remove [sceNpUtilGetFakeDisplayNameMode]

## remove [sceNpUtilGetFakeRateLimit]

## remove [sceNpUtilGetIgnoreNpTitleId]

## remove [sceNpUtilGetNpDebug]

## remove [sceNpUtilGetNpTestPatch]

## remove [sceNpUtilGetShareTitleCheck]

## remove [sceNpUtilGetTrcNotify]

## remove [sceNpUtilGetWebApi2FakeRateLimit]

## remove [sceNpUtilGetWebTraceSetting]

## change sceNpUtilGetNpDebug, sceNpUtilGetNpTestPatch

- [sceNpUtilGetDbgCommerce, sceNpUtilGetSystemLanguage, sceNpUtilGetEnv and sceNpUtilGetWebApi2FakeRateLimitTarget](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) read registry keys through libSceRegMgr and still throw until their parameter prototypes and registry reads are wired.

## add Silent stubs

- [PendingExports](../../../core/libs/prx/libSceNpCommon/PendingExports.cpp) (libSceNpCommon) - `sceNpGetSandboxMode`, `sceNpUtilGetSandboxMode`, `sceNpUtilGetDateSetAuto`, `sceNpUtilGetFakeDisplayNameMode`, `sceNpUtilGetFakeRateLimit`, `sceNpUtilGetIgnoreNpTitleId`, `sceNpUtilGetNpDebug`, `sceNpUtilGetNpTestPatch`, `sceNpUtilGetShareTitleCheck`, `sceNpUtilGetTrcNotify`, `sceNpUtilGetWebApi2FakeRateLimit` and `sceNpUtilGetWebTraceSetting` return their firmware 13.60 `libSceNpCommon.sprx` failure fallbacks (`100` for `sceNpUtilGetFakeRateLimit`, `0` for the rest) without reading `libSceRegMgr`, read from disassembly and not measured on a console.
