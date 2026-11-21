# libSceAjm.native

## add Functional

- [sceAjmDecWVorbisCreateHeaderPacket, sceAjmBatchJobInitialize, sceAjmBatchJobSetGaplessDecode, sceAjmBatchWait, sceAjmBatchJobRunSplit, sceAjmBatchStart, sceAjmBatchStartBuffer](../../../core/libs/prx/libSceAjm.native/Export.cpp) (libSceAjm.native) - safely probe and copy guest memory for Vorbis header packet parameters, setup packets, batch job initialize parameters, gapless decode descriptors, batch run and batch start structures, batch buffer records and batch wait error outputs using GuestCopyIn_nid_no_patch and GuestCopyOut_nid_no_patch instead of dereferencing guest pointers directly.
