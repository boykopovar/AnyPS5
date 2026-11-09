# libSceContentExport technical debt

### Functional

- [sceContentExportInit](../../../core/libs/prx/libSceContentExport/Export.cpp) - validates parameter presence and required allocator callbacks before recording library initialization state; allocator invocation and session worker initialization remain unmodelled
- [Content export sessions](../../../core/libs/prx/libSceContentExport/Export.cpp): the host has no media library, so the file export entry points and sceContentExportFromDataWithThumbnail validate the export session before calling NotImplemented_nid_no_patch without invoking data callbacks; sceContentExportFromData writes its chunks to a temporary file instead, as its own entry describes. Session allocation does not reproduce guest allocator calls, worker creation or failure paths. sceContentExportGetProgress calls NotImplemented_nid_no_patch because an idle session returns either zero or invalid-state according to caller SDK compatibility mode. Initialization and session lookup throw for some invalid inputs instead of reproducing console error precedence. Session IDs use the lowest free slot in 0–9; exhaustion, termination with live sessions and cancellation return confirmed errors.

- [sceContentExportFromData](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - the temporary file is written and returned as-is: the content-management service the module reaches through IPMI, its registration and thumbnail handling are not modeled, so the final media-library path is missing. Cancellation is not observed while the callback runs, and open or write failures map to `0x809D3001`/`0x809D3017` instead of the console per-errno codes. The chunks go to `/av_contents/content_tmp/sce_cmgcnt%p.<subtype>` (measured from 11.00), the total is checked against `contentLength`, and the path is copied bounded by `pathBufferLength` and 0x400. PPSA01746 (0xba0e14) calls it between Start and Finish and treats any non-zero as failure; the ContentManagement plugin of PPSA06465 (0x11f9) treats a negative result as failure and anything else as success.

### Silent stubs

- [sceContentExportTerm](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) returns 0 without waiting for ongoing operations, tearing down threads, or verifying that all resources have been released.

### Unknown function info

- [sceContentExportDataProvideFunction, sceContentExportFree, sceContentExportFrom and sceContentExportMalloc](../../../core/libs/prx/libSceContentExport/Export.cpp): their signatures remain unresolved; these names have no matching exports in the supplied module's C or assembly listing.

- [Content export arguments](../../../core/libs/prx/libSceContentExport/Export.cpp): file/data entry points and content-ID operations have restored argument lists. The Share Factory parameter contains a size, metadata string, content-ID list, count and option; metadata format and option values remain unresolved. Protected export bodies are unchanged.
