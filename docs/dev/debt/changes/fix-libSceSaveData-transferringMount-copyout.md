# libSceSaveData.native

## add Unknown function info

- [transferringMount](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData) - copies out mount result via GuestCopyOut_nid_no_patch with caller errno preserved; returning SAVE_DATA_ERROR_PARAMETER for an unmapped or unwritable pointer is a host-safety fallback with console behavior unmeasured.
