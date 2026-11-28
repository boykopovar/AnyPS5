# libSceIcu

## add Functional

- [`ucnv_compareNames`, `ucnv_io_stripASCIIForCompare` and `ucnv_io_stripEBCDICForCompare`](../../../core/libs/prx/libSceIcu/ConverterHelpers.cpp) read guest converter name inputs in bounded page-aligned slices via `GuestCopyIn_nid_no_patch` and verify and write normalized output via `GuestWritable_nid_no_patch` under `GuestArena::HostWrite` and `GuestCopyOut_nid_no_patch` while preserving `errno`. Null pointers keep throwing `invalid_argument` and unreadable or unwritable non-null guest memory throws `NotImplemented_nid_no_patch(__func__)` before writing to the destination; native invalid-pointer behavior remains unverified.
