# libSceHttpCache

## add Functional

- [libSceHttpCache guest header copies](../../../core/libs/prx/libSceHttpCache/Export.cpp) - `AppendHeaders` caps the copied header array at 4096 entries and 65536 bytes per name or value string via `GuestCopyIn_nid_no_patch` and `CopyGuestString_nid_no_patch`, returning `0x817a0012` on unreadable or oversized inputs, and `ErrnoGuard` preserves caller `errno` across internal guest-memory helpers; these bounds and `errno` guards are host-safety behavior not measured on console.
