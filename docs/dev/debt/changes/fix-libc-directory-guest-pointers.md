# libc

## add Unknown function info

- [opendir, readdir, readdir_r, closedir, scandir, alphasort, rewinddir, telldir, seekdir](../../../core/libs/prx/libc/src/Directory.cpp) (libc) - live directory handles are validated through a thread-safe host handle registry (returning `EBADF` for unknown, closed, or unmapped handles), `alphasort` validates guest `dirent` pointers and NUL-terminated `d_name` bytes through `GuestArenaPrepareHostRead_nid_no_patch` / `GuestReadable_nid_no_patch` / `GuestCopyIn_nid_no_patch` (throwing `std::invalid_argument` on unreadable inputs), and guest outputs in `readdir_r` and `scandir` use `GuestArena::HostWrite` / `GuestWritable_nid_no_patch` while preserving `errno`; console fault behavior for forged handles or unreadable `alphasort` entries is not measured.
