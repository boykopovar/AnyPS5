# libSceSaveDataDialog.native

## add Functional

- [sceSaveDataDialogGetResult](../../../core/libs/prx/libSceSaveDataDialog.native/Export.cpp) (libSceSaveDataDialog.native) - destination result and dir_name pointers are probed with GuestWritable_nid_no_patch upfront ensuring zero state mutation on failure with caller errno preserved; transactional non-mutation and error precedence on unmapped dir_name are host safety fallbacks with console behavior unmeasured.
