# libSceUserService

## add Functional

- [13.60 user settings exports](../../../core/libs/prx/libSceUserService/Inventory1360Exports.cpp) (libSceUserService) - pre-init and null-pointer check order matches firmware 13.60 `libSceUserService.sprx` (`0x4de0`, `0x8650`, `0x8680`, `0x3d70`, `0x3df0`, `0x3e10`, `0x3e40`, `0x3e60`, `0x3e90`). Daemon-side IPC (`0x30004`-`0x30009`, `0x3002c`) user-slot checks and settings persistence are modeled in-process with `GuestCopyOut_nid_no_patch`/`CopyGuestString_nid_no_patch` fallbacks preserving `errno` and reset on `sceUserServiceTerminate`.

## change [sceUserServiceGetUserWithHearingAssist]

- [sceUserServiceGetUserWithHearingAssist](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - 13.60 ABI is `(int32_t* userId)` and dispatches query `0x3002b`; reads the in-process hearing-assist user ID (reset on `sceUserServiceTerminate`) with `GuestCopyOut_nid_no_patch` fallback preserving `errno` while daemon-side persistence remains unmodeled.
