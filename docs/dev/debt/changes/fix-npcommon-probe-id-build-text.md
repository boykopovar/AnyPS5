# libSceNpCommon

## add Unknown function info

- [NpTitleId and NpCommId Build text](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) - `NpTitleId::Build` and `NpCommId::Build` clear the caller's buffer in 256-byte blocks through checked guest writes (and `NpTitleId::Build` writes the id the same way; `NpCommId::Build` writes it through `sceNpUtilBuildTitleId`), with `errno` preserved. An unwritable buffer returns `0x80550601`, the error `sceNpUtilBuildTitleId` returns for an unwritable title id, instead of faulting the host. The id objects are still read in place. The console's result for unmapped pointers was not measured.
