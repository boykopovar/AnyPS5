# libSceGnmDriver

## change [NeoMode-only imports]

- [NeoMode-only imports](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the 19 imports inFAMOUS Second Son asks of `libSceGnmDriver` that only `libSceGnmDriverForNeoMode` exported are now exported here too: `sceGnmSetEmbeddedPsShader` and `sceGnmSetEmbeddedVsShader` reuse the same embedded shader producers, `sceGnmMapComputeQueueWithPriority` maps a queue as `sceGnmMapComputeQueue` does (see Compute queue priorities), and the other 16 throw `NotImplemented_nid_no_patch` until their console behavior is measured (#17581).

## add Functional

- [Compute queue priorities](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - not measured on a console. `sceGnmMapComputeQueueWithPriority` returns what PS4 firmware 10.71 `libSceGnmDriverForNeoMode.sprx` (0x3f40) returns for refused arguments, in its order: `0x80D17000` for a pipe above 6, `0x80D17001` for a queue above 7, `0x80D17002` for a ring size that is not a power of two (0 passes), `0x80D17003` for a ring base with a nonzero low byte and `0x80D17004` for a read pointer that is not 4-byte aligned or a priority above 1. It then maps the queue as `sceGnmMapComputeQueue`, which throws for a missing ring or read pointer, a ring size of 0 and a queue already mapped. Priorities 0 and 1 are accepted and not applied: AGC queues have no priority, so the ring's work is scheduled as any other compute queue's.
