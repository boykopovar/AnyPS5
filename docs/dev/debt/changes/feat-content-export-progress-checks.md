# libSceContentExport

## change [Content export sessions]

- [Content export sessions](../../../core/libs/prx/libSceContentExport/Export.cpp): the host has no media library, so the remaining file export entry points validate the export session before calling NotImplemented_nid_no_patch; sceContentExportFromData writes its chunks to a temporary file instead, as its own entry describes. Session allocation does not reproduce guest allocator calls, worker creation or failure paths. Initialization and session lookup throw for some invalid inputs instead of reproducing console error precedence. Session IDs use the lowest free slot in 0–9; exhaustion, termination with live sessions and cancellation return confirmed errors.

## add Functional

- [sceContentExportValidateContents, sceContentExportGetProgress, sceContentExportFromFileWithTitleIdList and sceContentExportFromDataWithThumbnail](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - local checks follow a firmware 9.20 reading (#25863); past them each call reaches the content-management service and calls NotImplemented_nid_no_patch. The title-id export throws for malformed JSON (parser codes unread) and for elements whose rendered length is unsettled; another session holding the same file can't happen here, so `0x809D301A` comes only from source and thumbnail being one file. Not reproduced: the data export's TRC notification, callback writes and temporary files; a running export's progress. GetProgress throws after sceContentExportFromData. `param+0x101` is taken to be `comment`. Unreadable parameters or strings return `0x809D3016` (`0x809D3019` for the thumbnail type).
