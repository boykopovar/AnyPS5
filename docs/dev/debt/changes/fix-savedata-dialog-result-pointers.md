# libSceSaveDataDialog.native

## add Functional

- [sceSaveDataDialogGetResult](../../../core/libs/prx/libSceSaveDataDialog.native/Export.cpp) (libSceSaveDataDialog.native) - non-null `dir_name` destinations are probed upfront under `GuestArena::HostWrite` with `GuestWritable_nid_no_patch` before copying `result` out, ensuring zero partial `result` mutation on failure while preserving caller `errno`; transactional non-mutation and error precedence on unwritable `dir_name` are host-safety fallbacks with console behavior unmeasured. Closes when: measured on hardware. Labels: console, tested.
