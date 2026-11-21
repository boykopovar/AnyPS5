# libSceNpManager

## change [sceNpGetState, sceNpHasSignedUp, sceNpIsPlusMember, sceNpManagerIntGetAccountType, sceNpGetNpReachabilityState, sceNpPollAsync, sceNpWaitAsync]

- [sceNpGetState, sceNpHasSignedUp, sceNpIsPlusMember, sceNpManagerIntGetAccountType, sceNpGetNpReachabilityState, sceNpPollAsync, sceNpWaitAsync, sceNpManagerIntGetNpEnv](../../../core/libs/prx/libSceNpManager/Export.cpp) (libSceNpManager) - returning `SCE_NP_ERROR_INVALID_ARGUMENT` for an unwritable output pointer is a host-safety fallback; whether the console faults or returns an argument error on unmapped memory is unmeasured.
