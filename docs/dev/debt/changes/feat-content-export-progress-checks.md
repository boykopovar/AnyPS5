# libSceContentExport

## change [Content export sessions]

- [Content export sessions](../../../core/libs/prx/libSceContentExport/Export.cpp): the host has no media library, so the remaining file export entry points validate the export session before calling NotImplemented_nid_no_patch; sceContentExportFromData writes its chunks to a temporary file instead, as its own entry describes. Session allocation does not reproduce guest allocator calls, worker creation or failure paths. Initialization and session lookup throw for some invalid inputs instead of reproducing console error precedence. Session IDs use the lowest free slot in 0–9; exhaustion, termination with live sessions and cancellation return confirmed errors.

## add Functional

- [sceContentExportValidateContents, sceContentExportGetProgress, sceContentExportFromFileWithTitleIdList and sceContentExportFromDataWithThumbnail](../../../core/libs/prx/libSceContentExport/Export.cpp) (libSceContentExport) - the local checks follow a firmware 9.20 reading (#25863); past them each call reaches the content-management service and calls NotImplemented_nid_no_patch. Not reproduced: the title-id list export's file-open, file-identity, `sce_` name and JSON checks; the TRC notification, callback writes and temporary files of the data export; the progress of a running export. GetProgress models idle and ended sessions only, and throws for a session that has run sceContentExportFromData, whose state changes are unread. The all-zero block at `param+0x101` is taken to be `comment`. An unreadable parameter, callback parameter or string returns `0x809D3016` (`0x809D3019` for the thumbnail type) as a host-safety fallback.
