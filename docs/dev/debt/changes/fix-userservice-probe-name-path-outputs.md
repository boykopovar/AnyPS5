# libSceUserService

## add Unknown function info

- [default group name and eap home path outputs](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - `sceUserServiceGetDefaultNewUserGroupName` and `ErTMkmSho4E` format into a local and write the string with `GuestCopyOut_nid_no_patch`; an unwritable buffer returns `SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT` like a null one and like the other unwritable outputs of the library, because the console's behaviour on unmapped memory is unmeasured.
