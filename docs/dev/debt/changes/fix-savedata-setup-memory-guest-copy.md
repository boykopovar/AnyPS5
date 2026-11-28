# libSceSaveData.native

## add Functional

- [sceSaveDataSetupSaveDataMemory2](../../../core/libs/prx/libSceSaveData.native/Export.cpp) (libSceSaveData.native) - guest parameters are copied in with GuestCopyIn_nid_no_patch and destination result is probed with GuestWritable_nid_no_patch upfront with caller errno preserved; rejecting unmapped or unwritable pointer is host safety fallback with console behavior unmeasured.
