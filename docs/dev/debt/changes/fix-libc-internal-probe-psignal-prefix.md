# libSceLibcInternal

## add Unknown function info

- [psignal prefix](../../../core/libs/prx/libSceLibcInternal/SignalList.cpp) (libSceLibcInternal) - the prefix is copied in with `CopyGuestString_nid_no_patch` (up to 64 KiB) before it is written; `psignal` returns nothing, so an unreadable prefix is left out like a null or empty one and the signal description is still printed, because the console's behaviour on unmapped memory is unmeasured.
