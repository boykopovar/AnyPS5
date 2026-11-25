# libScePad

## add Functional

- [scePadGetDataInternal](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - copies out default and read controller state through `GuestCopyOut_nid_no_patch` and returns `PAD_ERROR_INVALID_ARG` on inaccessible guest pointers or out-of-range port indices instead of faulting; exact firmware validation order and error behavior for internal port reads are not measured on a console.
