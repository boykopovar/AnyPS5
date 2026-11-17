# libSceNpManager

## add Unknown function info

- [sce::np::NpOnlineId::GetNpOnlineId](../../../core/libs/prx/libSceNpManager/ValueClasses.cpp) (libSceNpManager) - validates guest User input and NpOnlineId output pointers via GuestCopyIn_nid_no_patch / GuestCopyOut_nid_no_patch with caller errno preserved; returning NP_ERROR_INVALID_ARGUMENT for an unmapped or unwritable pointer is a host-safety fallback with console behavior unmeasured.
