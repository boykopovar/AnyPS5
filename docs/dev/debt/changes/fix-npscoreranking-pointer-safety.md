# libSceNpScoreRanking

## change [sceNpScorePollAsync and sceNpScoreWaitAsync]

- [sceNpScorePollAsync, sceNpScoreWaitAsync and sceNpScoreGetGameData](../../../core/libs/prx/libSceNpScoreRanking/Export.cpp) (libSceNpScoreRanking) - an unwritable `result` pointer, probed under `GuestArena::HostWrite` before checking the request state so `sceNpScoreWaitAsync` does not block, or an unreadable `npId` returns `0x80550704` (`SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT`) as a host-safety fallback preserving host errno; console behavior on unmapped pointers is not measured. Closes when: measured on hardware. Labels: console, tested.
