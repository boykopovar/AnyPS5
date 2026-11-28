# libSceNpCommon

## add Unknown function info

- [sceNpCmpNpIdInOrder](../../../core/libs/prx/libSceNpCommon/Export.cpp) (libSceNpCommon) - copies in compared NpIdFull structures and copies out order via GuestCopyIn_nid_no_patch and GuestCopyOut_nid_no_patch with caller errno preserved; returning NP_ERROR_INVALID_ARGUMENT for unmapped pointers is a host-safety fallback with console behavior unmeasured.
