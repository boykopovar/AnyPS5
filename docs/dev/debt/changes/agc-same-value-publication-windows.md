# libSceAgcDriver

## add Functional

- [Same-value store during a guarded publication](../../../core/libs/prx/libSceAgcDriver/Execution/src/GuestMemory.cpp) (libSceAgcDriver) - Linux orders a guest store against the commit by making the part's pages read-only and writing the GPU bytes through the shared backing alias; a store in the window faults, waits for the commit to end, and then lands last. Windows private guest memory has no alias for a host write to a read-only page: `WriteProcessMemory` fails with 998 and `NtWriteVirtualMemory` with `STATUS_ACCESS_DENIED`, so `agc_write_tracking` skips the same-value regression there and a store in the window can still be lost. Closes when: Windows can write the page through a second mapping, or another ordering mechanism covers private guest pages.
