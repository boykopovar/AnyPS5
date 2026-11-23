# libScePad

## add Unknown function info

- [Transactional output safety and descriptor bounds](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - transactional output copyout via GuestCopyOut_nid_no_patch in scePadRead and pad query exports without redundant GuestWritable prechecks, and enforcing descriptor bounds in scePadSetVibrationForce returning PAD_ERROR_INVALID_HANDLE, is host-safety fallback; console behaviour on unmapped user pointers or invalid descriptors is unmeasured.
