# libSceContentExport technical debt

### Functional

- [sceContentExportInit](../../../core/libs/prx/libSceContentExport/Export.cpp) - validates parameter presence and required allocator callbacks before recording library initialization state; allocator invocation and session worker initialization remain unmodelled
- [sceContentExportFromData parameter validation](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - returning `0x809D3016` (`ErrorInvalidArgument`) for unmapped or unreadable parameter pointer is a defensive host-safety fallback; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.
- [Content export sessions](../../../core/libs/prx/libSceContentExport/Export.cpp): the host has no media library, so the file export entry points and sceContentExportFromDataWithThumbnail validate the export session before calling NotImplemented_nid_no_patch without invoking data callbacks; sceContentExportFromData writes its chunks to a temporary file instead, as its own entry describes. Session allocation does not reproduce guest allocator calls, worker creation or failure paths. sceContentExportGetProgress calls NotImplemented_nid_no_patch because an idle session returns either zero or invalid-state according to caller SDK compatibility mode. Initialization and session lookup throw for some invalid inputs instead of reproducing console error precedence. Session IDs use the lowest free slot in 0–9; exhaustion, termination with live sessions and cancellation return confirmed errors.

- [sceContentExportFromData](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - the temporary file is written and returned as-is: the content-management service reached through IPMI, its registration and thumbnail handling are not modeled, so the media-library path is missing. The firmware 4.03 preamble is reproduced: `0x809D3004` while uninitialized unless the SDK is below 0x4000000 (legacy mode skips the init gate), then `0x809D3016` for an out-of-range or unknown session, null `param` or null `callback`. Cancellation is not observed during the callback; open/write failures map to `0x809D3001`/`0x809D3017`. Chunks go to `/av_contents/content_tmp/sce_cmgcnt%p.<subtype>` (11.00), total must equal `contentLength`, and the path is copied bounded by `pathBufferLength` and 0x400. PPSA01746 (0xba0e14) calls it between Start and Finish and the ContentManagement plugin of PPSA06465 (0x11f9) judges it by result sign; argument and callback meaning are unverified.

### Silent stubs

- [sceContentExportTerm](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) returns 0 without waiting for ongoing operations, tearing down threads, or verifying that all resources have been released.

- [sceContentExportInit2](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - throwing std::invalid_argument for unmapped or unreadable parameter pointers is a host-safety fallback; whether console hardware faults or returns an argument error on unmapped memory is unmeasured.

### Unknown function info

- [sceContentExportDataProvideFunction, sceContentExportFree, sceContentExportFrom and sceContentExportMalloc](../../../core/libs/prx/libSceContentExport/Export.cpp): their signatures remain unresolved; these names have no matching exports in the supplied module's C or assembly listing.

- [Content export arguments](../../../core/libs/prx/libSceContentExport/Export.cpp): file/data entry points and content-ID operations have restored argument lists. The Share Factory parameter contains a size, metadata string, content-ID list, count and option; metadata format and option values remain unresolved. Protected export bodies are unchanged.
