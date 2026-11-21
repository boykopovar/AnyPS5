# libSceSocialScreen

## change sceSocialScreenCloseSeparateMode

- [sceSocialScreenCloseSeparateMode, sceSocialScreenConfigureSeparateMode, sceSocialScreenCrashDaemon, sceSocialScreenGetIgnoreSeparateMode, sceSocialScreenOpenSeparateMode, sceSocialScreenReserveHighFrameRateVideoCaptureRequest, sceSocialScreenSetIgnoreSeparateMode and sceSocialScreenSetMode](../../../core/libs/prx/libSceSocialScreen/Export.cpp) (libSceSocialScreen) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSocialScreen.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

## add Functional

- [sceSocialScreenInitialize and sceSocialScreenTerminate](../../../core/libs/prx/libSceSocialScreen/Export.cpp) (libSceSocialScreen) - lifecycle state transitions and module initialization/termination are implemented according to firmware 13.42 disassembly (`libSceSocialScreen.sprx` sha256 `ff405ecf36ade15bff81938663f11a80a3f978aa6b5876df900e63dc3d9ece6c`). When uninitialized, calls return `0x81440001`; `sceSocialScreenInitialize` returns `0x81440002` when already initialized and 0 on missing daemon connection.
