# libSceContentExport

## change [sceContentExportFromData parameter and path validation]

- [sceContentExportFromData parameter and path validation](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - returning `0x809D3016` (`ErrorInvalidArgument`) for unmapped or unwritable parameter or output path pointers or unterminated content type strings is a defensive host-safety fallback; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.
