# libSceUserService

## add Functional

- `sceUserServiceGetUserName` and `sceUserServiceGetUserGroupName` validate guest output buffers via `GuestCopyOut_nid_no_patch` and return `USER_SERVICE_ERROR_INVALID_ARGUMENT` on unmapped or unwritable memory; unwritable guest buffers for other exports remain unverified.
