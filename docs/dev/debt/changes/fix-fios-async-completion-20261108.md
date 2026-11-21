# libSceFios2

## change [Operations]

- [Operations](../../../core/libs/prx/libSceFios2/src/Runtime.cpp) (libSceFios2) - only one I/O worker is modeled and there are no decompressor threads. The `deadline`, `priority`, `opflags`, `userTag`, `userPtr` and scheduler fields are stored and returned but have no effect, and `SCE_FIOS_OPEVENT_START` is never sent.

## remove [Sync wait on a suspended operation]

## change [Operation threading]

- [Operation threading](../../../core/libs/prx/libSceFios2/src/Runtime.cpp) (libSceFios2) - callback and teardown ordering still need comparison with a title or console. Termination from a callback, initialization, parameter updates or operation submission during termination, and waiting on or deleting another incomplete operation from a callback are unmeasured and throw through `NotImplemented_nid_no_patch`. Multi-worker pool configuration in `SceFiosParams` is ignored. A synchronous wait lets the worker complete the requested asynchronous operation while unrelated operations remain suspended; its console scheduling policy remains unverified. Queued cancellation and explicit deletion still deliver callbacks on their calling thread; their console callback affinity is unverified.
