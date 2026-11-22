# libSceSaveData.native

## add Unknown function info

- [getSaveDataMemory2](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - copies in get_param via GuestCopyIn_nid_no_patch with caller errno preserved; returning SAVE_DATA_ERROR_PARAMETER for an unmapped or unreadable pointer is a host-safety fallback with console behavior unmeasured.
