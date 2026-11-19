# libSceSystemService

## add Unknown function info

- [sceSystemServiceGetMainAppTitleId](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - takes a title-ID buffer and writes the running title's title ID, as `sceSystemServiceGetAppTitleId` does for the running app ID. A null buffer returns `0x80A10003` (`SYSTEM_SERVICE_ERROR_PARAMETER`), as [SharpEmu](https://github.com/sharpemu/sharpemu/blob/80e81c9aab38e87e98d955e2c78f037fbb7f4a3f/src/SharpEmu.Libs/SystemService/SystemServiceExports.cs#L109-L120) does for its one-argument export. The console's result for an unwritable buffer and for a title ID of 10 or more characters is unmeasured, so those calls throw after the checks `CopyRunningTitleId` already makes.
