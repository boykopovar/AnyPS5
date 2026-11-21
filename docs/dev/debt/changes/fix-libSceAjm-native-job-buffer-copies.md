# libSceAjm.native

## add Functional

- [AJM job buffers](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - run jobs read their input, and a control job its input, through `GuestCopyIn_nid_no_patch`, and every job writes its PCM output and sideband through `GuestCopyOut_nid_no_patch`, with caller errno preserved. A single-buffer input is read as it is consumed: the Opus packet lengths and packets, the ATRAC9 RIFF headers and superframes, the MP3 and ADTS headers and frames, and all of a PCM input. So an input whose stated size runs past its readable data still decodes, as long as each header, frame or superframe the job reaches is readable. A multi-buffer input is joined in full before the job runs, as before. An unreadable input, or an unwritable output or sideband, throws `std::runtime_error` from `sceAjmBatchStart` instead of faulting the host. What the console's AJM does with such a buffer is not measured.
