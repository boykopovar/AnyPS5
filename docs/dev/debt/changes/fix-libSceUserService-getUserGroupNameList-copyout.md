# libSceUserService

## add Unknown function info

- [sceUserServiceGetUserGroupNameList](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - probes destination writability and copies out user group name list and count via GuestCopyOut_nid_no_patch with caller errno preserved; returning USER_SERVICE_ERROR_INVALID_ARGUMENT for unmapped or unwritable pointers is a host-safety fallback with console behavior unmeasured.
