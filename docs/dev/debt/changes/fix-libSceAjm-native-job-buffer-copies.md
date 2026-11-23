# libSceAjm.native

## add Functional

- [AJM job buffers](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - what the console's AJM does with an inaccessible buffer is not measured. Jobs read inputs through `GuestCopyIn_nid_no_patch` and write PCM output and sideband through `GuestCopyOut_nid_no_patch`, keeping errno. A single-buffer input is read as it is consumed (Opus lengths and packets, ATRAC9 headers and superframes, MP3 and ADTS headers and frames, all of a PCM input), so a stated size past the readable data still decodes while each piece the job reaches is readable; a multi-buffer input is joined first. An Opus packet that runs into unreadable memory ends the job with `AJM_RESULT_INVALID_DATA` and nothing consumed, as an unparseable packet does (Super Meat Boy 3D's FMOD submits such end-of-stream jobs). Any other unreadable input, or an unwritable output or sideband, throws `std::runtime_error` from `sceAjmBatchStart`.
