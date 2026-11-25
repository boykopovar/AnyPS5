# libSceShareUtility

## change [sceShareUtilityInitialize, sceShareUtilityInitializeEx2, sceShareUtilityAdvanceInitialize and sceShareUtilityAdvanceSetUploadContentData]

- [sceShareUtilityInitialize, sceShareUtilityInitializeEx2, sceShareUtilityAdvanceInitialize and sceShareUtilityAdvanceSetUploadContentData](../../../core/libs/prx/libSceShareUtility/Export.cpp) (libSceShareUtility) - track module basic/advance initialization state, verify `priority` (`0x100..0x2FF`) and non-null `affinity` in `sceShareUtilityInitializeEx2` and `sceShareUtilityAdvanceInitialize` (`0xf30`, `0xf80`, `0x1350`), and copy upload payload data (`<= 0x1FFF` bytes) into module storage in `sceShareUtilityAdvanceSetUploadContentData` (`0x13d0`) following firmware 13.60 `libSceShareUtility.sprx` disassembly (not measured on a console). Unreadable non-null upload pointers return `0x81170003` while preserving `errno`, and ShellUI dialog/IPC submission beyond storing state remains unimplemented.
