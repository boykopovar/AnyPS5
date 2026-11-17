# libSceNpScoreRanking

## add Functional

- [sceNpScorePollAsync](../../../core/libs/prx/libSceNpScoreRanking/Export.cpp) (libSceNpScoreRanking) - destination result pointer is validated upfront with GuestWritable_nid_no_patch and npId is read via GuestCopyIn_nid_no_patch preserving caller errno; rejection of unmapped memory is host safety fallback with console behavior unmeasured.
