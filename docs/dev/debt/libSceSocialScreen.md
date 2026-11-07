# libSceSocialScreen technical debt

### Silent stubs

- [sceSocialScreenInitialize and sceSocialScreenTerminate](../../../core/libs/prx/libSceSocialScreen/Export.cpp) track initialization state transitions without initializing social screen display services.

### Unknown function info

- [sceSocialScreenCloseSeparateMode, sceSocialScreenConfigureSeparateMode, sceSocialScreenCrashDaemon, sceSocialScreenGetIgnoreSeparateMode, sceSocialScreenOpenSeparateMode, sceSocialScreenReserveHighFrameRateVideoCaptureRequest, sceSocialScreenSetIgnoreSeparateMode, sceSocialScreenSetMode](../../../core/libs/prx/libSceSocialScreen/Export.cpp) (libSceSocialScreen) - names and NIDs are present in the PS5 3.20 export inventory. Separate mode and configuration exports enforce initialization returning 0x81440001 when uninitialized; remaining parameters and runtime behavior remain unverified and throw through NotImplemented_nid_no_patch.
- [sceSocialScreenInitializeSeparateModeParameter](../../../core/libs/prx/libSceSocialScreen/Export.cpp) (libSceSocialScreen) - the layout of the 32-byte parameter block is unknown. It follows firmware 13.42's libSceSocialScreen.sprx, read from its disassembly and run natively on x86-64 (not a console): a null pointer returns `0x81440003`; otherwise it stores the 32-bit value 1 at offset 0, clears bytes 4 to 31, leaves later bytes unchanged and returns 0. Returning `0x81440003` for an unwritable parameter is a host-safety fallback; console behaviour for unwritable memory has not been measured.
