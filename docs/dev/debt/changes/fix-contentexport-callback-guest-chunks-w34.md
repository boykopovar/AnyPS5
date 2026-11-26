# libSceContentExport

## add Functional

- [sceContentExportFromData callback chunks](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - callback buffer bytes are staged through `GuestCopyIn_nid_no_patch` in 4 KiB blocks. An unreadable or address-wrapping callback buffer returns `0x809D3017` (`ErrorStorage`) as a host-safety fallback, preserving caller `errno` across checked guest reads. Console behavior for an unreadable callback buffer is unmeasured.
