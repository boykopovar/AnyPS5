# libSceAvPlayerStreaming

## remove sceAvPlayerStreamingRegisterVprinf

## change The null-argument answers

- [The null-argument answers](../../../core/libs/prx/libSceAvPlayerStreaming/Export.cpp) (libSceAvPlayerStreaming) - `sceTs*` exports answer null arguments and throw on valid arguments since no file can be opened here (`sceTsOpenFileEx` throws; `sceTsOpenFile` returns 0). In firmware 13.42's `libSceAvPlayerStreaming.sprx` (SHA-256 `38d13c20506f9b4ae323b4d5e71f726ed2a6340e933a81b850372e481d383196`), read from disassembly, calls compare pointer arguments with 0 before memory reads. `sceTsStopFileStreaming` returns `0xF0000200` on null file; `sceTsStartFileStreaming`, `sceTsGetStreamInfo`, `sceTsGetSeekWindow`, `sceTsGetNextAuInfo`, `sceTsGetNextUnitData`, `sceTsGetAvailableBW`, `sceTsGetLiveStatus`, `sceTsGetRepresentationInfo` and `sceTsRepresentationIsEnabled` return -1 on null file or checked output; the other nine return -1 on null file.

## add Functional

- [sceAvPlayerStreamingRegisterVprinf, sceTsOpenFile and sceTsReleaseBuffer](../../../core/libs/prx/libSceAvPlayerStreaming/Export.cpp) (libSceAvPlayerStreaming) - logging callback registration, stub file open returning 0, and buffer clearing are implemented according to firmware 13.42 disassembly (`libSceAvPlayerStreaming.sprx` sha256 `38d13c20506f9b4ae323b4d5e71f726ed2a6340e933a81b850372e481d383196`). The buffer clear writes the 8-byte and 4-byte fields in order; if the second guest write is denied, the first field remains zero and the export returns -1 while preserving errno. Console behavior for this partial-write failure is unmeasured.
