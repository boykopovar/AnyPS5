# libSceFreeType

## add Functional

- [FT_Load_Sfnt_Table](../../../core/libs/prx/libSceFreeType/src/Sfnt.cpp) validates and transfers `length` and table `buffer` bytes through `GuestWritable_nid_no_patch`, `GuestCopyIn_nid_no_patch`, and `GuestCopyOut_nid_no_patch` while preserving `errno`, returning `FT_Err_Invalid_Argument` without mutating caller memory on unwritable outputs.
