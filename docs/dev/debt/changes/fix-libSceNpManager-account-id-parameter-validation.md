# libSceNpManager

## add Functional

- [sceNpGetAccountId, sceNpGetAccountIdA, sceNpGetOnlineId, sceNpGetNpId](../../../core/libs/prx/libSceNpManager/Export.cpp) (libSceNpManager) - validating user ID and online ID input parameters via `GuestCopyIn_nid_no_patch` while preserving host `errno` is a host-safety boundary; whether retail console firmware checks unmapped pointers before offline reporting is unmeasured.
