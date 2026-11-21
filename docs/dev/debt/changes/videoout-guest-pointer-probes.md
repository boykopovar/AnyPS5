# libSceVideoOut

## add Functional

- [Buffer, mode and status pointer probes](../../../core/libs/prx/libSceVideoOut/src/Output.cpp) (libSceVideoOut) - buffer registration and attributes, system mode inspection, event counting, capability queries, and output/flip/resolution status queries safely probe and copy guest memory parameters using GuestCopyIn_nid_no_patch and GuestCopyOut_nid_no_patch instead of dereferencing guest pointers directly.
