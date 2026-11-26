# libSceNpScoreRanking

## change sceNpScoreAbortRequest

- [sceNpScoreWaitAsync](../../../core/libs/prx/libSceNpScoreRanking/Export.cpp) (libSceNpScoreRanking) - `sceNpScoreAbortRequest` only checks the request id and answers 0: the module's abort sends the service an IPC call and never sets the request's done flag or signals its semaphore ([disassembly](https://github.com/boykopovar/AnyPS5/pull/12611#issuecomment-6520890176), not run), so a wait blocked on a request with no operation started stays blocked and a poll keeps answering 1 (running). An operation finishes here before its call returns, so an abort after one leaves the finished result, and `0x80550707` (`SCE_NP_COMMUNITY_ERROR_ABORTED`) is no longer answered. Whether the service ends a running operation sooner was not read. Whether deleting the request's title context releases it was not read; here it stays blocked, as for a deleted request
