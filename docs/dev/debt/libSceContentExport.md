# libSceContentExport technical debt

### Functional

- [Content export sessions](../../../core/libs/prx/libSceContentExport/Export.cpp): the host has no media library, so `sceContentExportFromData`, `sceContentExportFromFile`, `sceContentExportFromFileWithThumbnail` and `sceContentExportFromDataWithThumbnail` validate the export session before calling `NotImplemented_nid_no_patch` without invoking data callbacks. Session allocation does not reproduce guest allocator calls, worker creation or failure paths. `sceContentExportGetProgress` calls `NotImplemented_nid_no_patch` because an idle session returns either zero or invalid-state according to caller SDK compatibility mode. Initialization and session lookup throw for some invalid inputs instead of reproducing console error precedence and legacy SDK behavior. Session IDs use the lowest free slot in 0–9; exhaustion, termination with live sessions and cancellation before export return confirmed errors.

- [sceContentExportFromData](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - validates the export session and throws as unimplemented. Two titles disassembled for this entry call it once each with 7 arguments: PPSA01746 (0xba0e14) between `sceContentExportStart` and `sceContentExportFinish`, treating any result other than 0 as failure, and the ContentManagement plugin of PPSA06465 (0x11f9), treating a negative result as failure and any other as success. What the arguments and the callback mean is not shown.

### Unknown function info

- [sceContentExportDataProvideFunction, sceContentExportFree, sceContentExportFrom and sceContentExportMalloc](../../../core/libs/prx/libSceContentExport/Export.cpp): their signatures remain unresolved; these names have no matching exports in the supplied module's C or assembly listing.

- [Content export arguments](../../../core/libs/prx/libSceContentExport/Export.cpp): file/data entry points and content-ID operations have restored argument lists. The Share Factory parameter contains a size, metadata string, content-ID list, count and option; metadata format and option values remain unresolved. Protected export bodies are unchanged.
