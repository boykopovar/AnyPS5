# libSceLibcInternal

## add Functional

- [getopt, getopt_long, and getopt_long_only](../../../core/libs/prx/libSceLibcInternal/Getopt.cpp) copy guest option strings, argument vector slots, long-option tables, and write back `longindex`/`flag` pointers through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting unreadable or unwritable guest pointers via `NotImplemented_nid_no_patch(__func__)`.
