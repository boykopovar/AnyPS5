# libSceSysCore

## add Functional

- [13.60 pre-init and null-output exports](../../../core/libs/prx/libSceSysCore/Export.cpp) - 3 exports present in firmware 13.60 `libSceSysCore.sprx` (`KJ3Cw7ZH6pU` at `0x1390`, `WoJ7K-2EIKk` at `0x17f0`, `x6tZHyeLijI` at `0x1900`) are import-resolution declarations using literal NID aliases; all three return `0x80AA0004` (`kNotInitialized`) when uninitialized and throw through `NotImplemented_nid_no_patch` past that gate. `sceApplicationGetAppInfoByAppId` also validates a null `info` pointer (`0x80AA0001`) after the initialization check.
