# libfreetype

## add Functional

- [FT_Gzip_Uncompress guest pointers](../../../core/libs/prx/libfreetype/src/Gzip.cpp) (libfreetype) copies `memory`, `outputLength`, `input`, and `output` through `GuestCopyIn`/`GuestCopyOut`, preserving `errno` and calling `NotImplemented_nid_no_patch` on invalid or read-only guest addresses instead of dereferencing them on the host.
