# libSceContentExport technical debt

### Functional

- [Content export sessions](../../../core/libs/prx/libSceContentExport/Export.cpp): the host has no media library, so `sceContentExportFromDataWithThumbnail` goes through `NotImplemented_nid_no_patch` for a valid session without invoking data callbacks. Session allocation does not reproduce guest allocator calls, worker creation or their failure paths. `sceContentExportGetProgress` goes through `NotImplemented_nid_no_patch` because an idle session returns either zero or an invalid-state error according to the caller's SDK compatibility mode, which is not available here. Initialization and session lookup still throw for some invalid inputs instead of reproducing the console's error precedence and legacy SDK behavior. Session IDs now use the lowest free slot in 0–9; exhaustion, termination with live sessions and cancellation before an export return the confirmed errors.

### Unknown function info

- [sceContentExportDataProvideFunction, sceContentExportFree, sceContentExportFrom and sceContentExportMalloc](../../../core/libs/prx/libSceContentExport/Export.cpp): their signatures remain unresolved; these names have no matching exports in the supplied module's C or assembly listing.

- [Content export arguments](../../../core/libs/prx/libSceContentExport/Export.cpp): file/data entry points and content-ID operations have restored argument lists. The Share Factory parameter contains a size, metadata string, content-ID list, count and option; metadata format and option values remain unresolved. Protected export bodies are unchanged.
