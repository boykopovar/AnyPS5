# libSceNpCommon

## change [Online ID and NpId utilities]

- [Online ID and NpId utilities](../../../core/libs/prx/libSceNpCommon/Export.cpp) (libSceNpCommon) - `sceNpCmpOnlineId`, `sceNpCmpNpIdInOrder`, `sceNpGetPlatformType` and `sceNpIntIsValidOnlineId` follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/np/np_common.cpp), including the platform tags read from `opt[4..7]` and the 3 to 16 character online ID rule, confirmed in a PS5 Pro payload process (13.60): nulls give `0x80550003`, validity is boolean, and platform answers are no tag 0, `ps3` 1, `psp2` 2, `ps4` 3, unknown and `comp` `0x80550004` (`IntGet` maps `comp` to 4). The console also refuses `sceNpCmpNpIdInOrder` with `0x80550605` when either NpId's `reserved[0]` is not the canonicalized marker 1 that `sceNpUtilJidToNpId` writes, leaving the order output untouched; the check runs after null and unmapped-pointer errors, other byte orderings were not re-measured.
