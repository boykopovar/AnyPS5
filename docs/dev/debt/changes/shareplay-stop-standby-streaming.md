# libSceSharePlay

## add Functional

- [sceSharePlayStopStandby, sceSharePlayStopStreaming](../../../core/libs/prx/libSceSharePlay/Export.cpp) - return 0x810E0004 both before and after `sceSharePlayInitialize`, because the host has no Share Play service and so never reaches the connected state (2) the native module requires. The per-export check is taken from the module-wide disassembly note above, not read for these two exports specifically; their signatures and the success path are unverified and not measured on a console.
