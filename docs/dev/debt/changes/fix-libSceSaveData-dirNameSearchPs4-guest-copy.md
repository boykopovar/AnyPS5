# libSceSaveData.native

## add Unknown function info

- [dirNameSearchPs4](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData) - copies in search condition and copies out search result via GuestCopyIn_nid_no_patch and GuestCopyOut_nid_no_patch with caller errno preserved; returning SAVE_DATA_ERROR_PARAMETER for unmapped pointers is a host-safety fallback with console behavior unmeasured.
