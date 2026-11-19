# libfreetype

## add Functional

- [FT_Gzip_Uncompress guest pointers](../../../core/libs/prx/libfreetype/src/Gzip.cpp) (libfreetype) copies `memory`, `outputLength`, `input`, and `output` through `GuestCopyIn`/`GuestCopyOut`, preserving `errno` and throwing `std::runtime_error` on invalid or read-only guest addresses instead of dereferencing them on the host.
